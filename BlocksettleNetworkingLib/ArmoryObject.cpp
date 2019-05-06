#include "ArmoryObject.h"

#include <QFile>
#include <QProcess>
#include <QPointer>

#include <cassert>
#include <exception>
#include <condition_variable>

#include "ClientClasses.h"
#include "DbHeader.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include "JSON_codec.h"
#include "ManualResetEvent.h"
#include "SocketIncludes.h"

namespace {

const int DefaultArmoryDBStartTimeoutMsec = 500;

} // namesapce

Q_DECLARE_METATYPE(ArmoryConnection::State)
Q_DECLARE_METATYPE(BDMPhase)
Q_DECLARE_METATYPE(NetworkType)
Q_DECLARE_METATYPE(NodeStatus)
Q_DECLARE_METATYPE(bs::TXEntry)
Q_DECLARE_METATYPE(std::vector<bs::TXEntry>)


ArmoryObject::ArmoryObject(const std::shared_ptr<spdlog::logger> &logger
   , const std::string &txCacheFN, bool cbInMainThread)
   : QObject(nullptr), ArmoryConnection(logger)
   , cbInMainThread_(cbInMainThread), txCache_(txCacheFN)
{
   qRegisterMetaType<ArmoryConnection::State>();
   qRegisterMetaType<BDMPhase>();
   qRegisterMetaType<NetworkType>();
   qRegisterMetaType<NodeStatus>();
   qRegisterMetaType<bs::TXEntry>();
   qRegisterMetaType<std::vector<bs::TXEntry>>();

   cbStateChanged_ = [this](State state) {
      emit stateChanged(state);
   };
   setRefreshCb([this](std::vector<BinaryData> ids, bool online) {
      emit refresh(ids, online);
   });
   cbNewBlock_ = [this](unsigned int blockNo) {
      emit newBlock(blockNo);
   };
   cbZCReceived_ = [this](std::vector<bs::TXEntry> zcEntries) {
      emit zeroConfReceived(zcEntries);
   };
   cbZCInvalidated_ = [this](std::vector<bs::TXEntry> zcEntries) {
      emit zeroConfInvalidated(zcEntries);
   };
   cbProgress_ = [this](BDMPhase phase, float prog, unsigned int secondsRem, unsigned int numProgress) {
      emit progress(phase, prog, secondsRem, numProgress);
   };
   cbError_ = [this](const std::string &errStr, const std::string &extraMsg) {
      emit error(QString::fromStdString(errStr), QString::fromStdString(extraMsg));
   };
   cbTxBcError_ = [this](const std::string &txHash, const std::string &errStr) {
      emit txBroadcastError(QString::fromStdString(txHash), QString::fromStdString(errStr));
   };
}

bool ArmoryObject::startLocalArmoryProcess(const ArmorySettings &settings)
{
   if (armoryProcess_ && (armoryProcess_->state() == QProcess::Running)) {
      logger_->info("[{}] Armory process {} is already running with PID {}"
         , __func__, settings.armoryExecutablePath.toStdString()
         , armoryProcess_->processId());
      return true;
   }
   const QString armoryDBPath = settings.armoryExecutablePath;
   if (QFile::exists(armoryDBPath)) {
      armoryProcess_ = std::make_shared<QProcess>();

      QStringList args;
      switch (settings.netType) {
      case NetworkType::TestNet:
         args.append(QString::fromStdString("--testnet"));
         break;
      case NetworkType::RegTest:
         args.append(QString::fromStdString("--regtest"));
         break;
      default: break;
      }

//      args.append(QLatin1String("--db-type=DB_FULL"));
      args.append(QLatin1String("--listen-port=") + QString::number(settings.armoryDBPort));
      args.append(QLatin1String("--satoshi-datadir=\"") + settings.bitcoinBlocksDir + QLatin1String("\""));
      args.append(QLatin1String("--dbdir=\"") + settings.dbDir + QLatin1String("\""));
      args.append(QLatin1String("--public"));

      logger_->debug("[{}] running {} {}", __func__, settings.armoryExecutablePath.toStdString()
         , args.join(QLatin1Char(' ')).toStdString());
      armoryProcess_->start(settings.armoryExecutablePath, args);
      if (armoryProcess_->waitForStarted(DefaultArmoryDBStartTimeoutMsec)) {
         return true;
      }
      armoryProcess_.reset();
   }
   return false;
}

void ArmoryObject::setupConnection(const ArmorySettings &settings, const BIP151Cb &bip150PromptUserCb)
{
   emit prepareConnection(settings);

   if (settings.runLocally) {
      if (!startLocalArmoryProcess(settings)) {
         logger_->error("[{}] failed to start Armory from {}", __func__
                        , settings.armoryExecutablePath.toStdString());
         setState(State::Offline);
         return;
      }
   }

   // Add BIP 150 server keys
   BinaryData serverBIP15xKey;
   if (!settings.armoryDBKey.isEmpty()) {
      serverBIP15xKey = READHEX(settings.armoryDBKey.toStdString());
   }

   const auto &cbError = [this](const std::string &errDesc) {
      emit connectionError(QString::fromStdString(errDesc));
   };

   ArmoryConnection::setupConnection(settings.netType, settings.armoryDBIp.toStdString()
      , std::to_string(settings.armoryDBPort), settings.dataDir.toStdString(), serverBIP15xKey
      , cbError, bip150PromptUserCb);
}

std::string ArmoryObject::registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &wallet
   , const std::string &walletId, const std::vector<BinaryData> &addrVec
   , const RegisterWalletCb &cb
   , bool asNew)
{
   const auto &cbWrap = [this, cb](const std::string &regId) {
      if (cb) {
         if (cbInMainThread_) {
            QMetaObject::invokeMethod(this, [cb, regId] { cb(regId); });
         } else {
            cb(regId);
         }
      }
   };
   return ArmoryConnection::registerWallet(wallet, walletId, addrVec, cbWrap, asNew);
}

bool ArmoryObject::getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &cb)
{
   const auto &cbWrap = [this, cb](std::vector<ClientClasses::LedgerEntry> le) {
      if (!cb) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, le] { cb(std::move(le)); });
      } else {
         cb(std::move(le));
      }
   };
   return ArmoryConnection::getWalletsHistory(walletIDs, cbWrap);
}

bool ArmoryObject::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr
   , const LedgerDelegateCb &cb, QObject *context)
{
   QPointer<QObject> contextSmartPtr = context;
   const auto &cbWrap = [this, cb, context, contextSmartPtr]
                        (const std::shared_ptr<AsyncClient::LedgerDelegate> &ld) {
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, ld, context, contextSmartPtr]{
            if (context) {
               if (contextSmartPtr) {
                  cb(ld);
               }
            } else {
               cb(ld);
            }
         });
      }
      else {
         cb(ld);
      }
   };
   return ArmoryConnection::getLedgerDelegateForAddress(walletId, addr, cbWrap);
}

bool ArmoryObject::getWalletsLedgerDelegate(const LedgerDelegateCb &cb)
{
   const auto &cbWrap = [this, cb](const std::shared_ptr<AsyncClient::LedgerDelegate> &ld) {
      if (!cb) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, ld] {
            cb(ld);
         });
      } else {
         cb(ld);
      }
   };
   return ArmoryConnection::getWalletsLedgerDelegate(cbWrap);
}

bool ArmoryObject::getTxByHash(const BinaryData &hash, const TxCb &cb)
{
   const auto tx = txCache_.get(hash);
   if (tx.isInitialized()) {
      cb(tx);
      return true;
   }
   const auto &cbWrap = [this, cb, hash](Tx tx) {
      txCache_.put(hash, tx);
      if (!cb) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, tx] { cb(tx); });
      }
      else {
         cb(tx);
      }
   };
   return ArmoryConnection::getTxByHash(hash, cbWrap);
}

bool ArmoryObject::getTXsByHash(const std::set<BinaryData> &hashes, const TXsCb &cb)
{
   auto result = std::make_shared<std::vector<Tx>>();
   std::set<BinaryData> missedHashes;
   for (const auto &hash : hashes) {
      const auto tx = txCache_.get(hash);
      if (tx.isInitialized()) {
         result->push_back(tx);
      }
      else {
         missedHashes.insert(hash);
      }
   }
   if (missedHashes.empty()) {
      cb(*result);
      return true;
   }
   const auto &cbWrap = [this, cb, result](const std::vector<Tx> &txs) {
      for (const auto &tx : txs) {
         if (tx.isInitialized()) {
            txCache_.put(tx.getThisHash(), tx);
         }
         result->push_back(tx);
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, result] { cb(*result); });
      }
      else {
         cb(*result);
      }
   };
   return ArmoryConnection::getTXsByHash(missedHashes, cbWrap);
}

bool ArmoryObject::getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &callback)
{
   const auto &cbWrap = [this, callback](BinaryData header) {
      if (!callback) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [callback, header] { callback(std::move(header)); });
      } else {
         callback(header);
      }
   };
   return ArmoryConnection::getRawHeaderForTxHash(inHash, cbWrap);
}

bool ArmoryObject::getHeaderByHeight(const unsigned int inHeight, const BinaryDataCb &callback)
{
   const auto &cbWrap = [this, callback](BinaryData header) {
      if (!callback) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [callback, header] { callback(std::move(header)); });
      } else {
         callback(header);
      }
   };
   return ArmoryConnection::getHeaderByHeight(inHeight, cbWrap);
}

// Frontend for Armory's estimateFee() call. Used to get the "conservative" fee
// that Bitcoin Core estimates for successful insertion into a block within a
// given number (2-1008) of blocks.
bool ArmoryObject::estimateFee(unsigned int nbBlocks, const FloatCb &cb)
{
   const auto &cbWrap = [this, cb](float fee) {
      if (!cb) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, fee] { cb(fee); });
      } else {
         cb(fee);
      }
   };
   return ArmoryConnection::estimateFee(nbBlocks, cbWrap);
}

// Frontend for Armory's getFeeSchedule() call. Used to get the range of fees
// that Armory caches. The fees/byte are estimates for what's required to get
// successful insertion of a TX into a block within X number of blocks.
bool ArmoryObject::getFeeSchedule(const FloatMapCb &cb)
{
   const auto &cbWrap = [this, cb](std::map<unsigned int, float> fees) {
      if (!cb) {
         return;
      }
      if (cbInMainThread_) {
         QMetaObject::invokeMethod(this, [cb, fees] { cb(fees); });
      }
      else {
         cb(fees);
      }
   };
   return ArmoryConnection::getFeeSchedule(cbWrap);
}

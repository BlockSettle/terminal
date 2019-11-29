/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

   const int kDefaultArmoryDBStartTimeoutMsec = 500;

} // namespace


ArmoryObject::ArmoryObject(const std::shared_ptr<spdlog::logger> &logger
   , const std::string &txCacheFN, bool cbInMainThread)
   : ArmoryConnection(logger)
   , cbInMainThread_(cbInMainThread)
#ifdef USE_LOCAL_TX_CACHE
   , txCache_(txCacheFN)
#endif
{}

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
      if (armoryProcess_->waitForStarted(kDefaultArmoryDBStartTimeoutMsec)) {
         return true;
      }
      armoryProcess_.reset();
   }
   return false;
}

bool ArmoryObject::needInvokeCb() const
{
   return cbInMainThread_ && (thread() != QThread::currentThread());
}

void ArmoryObject::setupConnection(const ArmorySettings &settings, const BIP151Cb &bip150PromptUserCb)
{
   if (settings.runLocally) {
      if (!startLocalArmoryProcess(settings)) {
         logger_->error("[{}] failed to start Armory from {}", __func__
                        , settings.armoryExecutablePath.toStdString());
         setState(ArmoryState::Offline);
         return;
      }
   }

   // Add BIP 150 server keys
   BinaryData serverBIP15xKey;
   if (!settings.armoryDBKey.isEmpty()) {
      try {
         serverBIP15xKey = READHEX(settings.armoryDBKey.toStdString());
      } catch (const std::exception &e) {
         logger_->error("invalid armory key detected: {}: {}", settings.armoryDBKey.toStdString(), e.what());
      }
   }

   ArmoryConnection::setupConnection(settings.netType, settings.armoryDBIp.toStdString()
      , std::to_string(settings.armoryDBPort), settings.dataDir.toStdString(), serverBIP15xKey
      , settings.password, bip150PromptUserCb);
}

bool ArmoryObject::getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &cb)
{
   const auto &cbWrap = [this, cb](std::vector<ClientClasses::LedgerEntry> le) {
      if (!cb) {
         return;
      }
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [cb, le] { cb(std::move(le)); });
      } else {
         cb(std::move(le));
      }
   };
   return ArmoryConnection::getWalletsHistory(walletIDs, cbWrap);
}

bool ArmoryObject::getWalletsLedgerDelegate(const LedgerDelegateCb &cb)
{
   const auto &cbWrap = [this, cb](const std::shared_ptr<AsyncClient::LedgerDelegate> &ld) {
      if (!cb) {
         return;
      }
      if (needInvokeCb()) {
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
{      // don't use local cache to have actual height in TX
#ifdef USE_LOCAL_TX_CACHE
   const auto tx = txCache_.get(hash);
   if (tx.isInitialized()) {
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [cb, tx] {
            cb(tx);
         });
      } else {
         cb(tx);
      }
      return true;
   }
#endif   //USE_LOCAL_TX_CACHE
   const auto &cbWrap = [this, cb, hash](Tx tx) {
#ifdef USE_LOCAL_TX_CACHE
      txCache_.put(hash, tx);
#endif
      if (!cb) {
         return;
      }
      if (needInvokeCb()) {
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
#ifdef USE_LOCAL_TX_CACHE  // TXs need to contain the actual height, caching by hash forbids this
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
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [cb, result] {
            cb(*result);
         });
      } else {
         cb(*result);
      }
      return true;
   }
#endif   //USE_LOCAL_TX_CACHE
   const auto &cbWrap = [this, cb, result]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         cb({}, exPtr);
         return;
      }
      for (const auto &tx : txs) {
#ifdef USE_LOCAL_TX_CACHE
         if (tx.isInitialized()) {
            txCache_.put(tx.getThisHash(), tx);
         }
#endif   // USE_LOCAL_TX_CACHE
         result->push_back(tx);
      }
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [cb, result] { cb(*result, nullptr); });
      }
      else {
         cb(*result, nullptr);
      }
   };
   return ArmoryConnection::getTXsByHash(/*missedHashes*/hashes, cbWrap);
}

bool ArmoryObject::getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &callback)
{
   const auto &cbWrap = [this, callback](BinaryData header) {
      if (!callback) {
         return;
      }
      if (needInvokeCb()) {
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
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [callback, header] { callback(std::move(header)); });
      } else {
         callback(header);
      }
   };
   return ArmoryConnection::getHeaderByHeight(inHeight, cbWrap);
}

// Frontend for Armory's estimateFee() call. Used to get the "economical" fee
// that Bitcoin Core estimates for successful insertion into a block within a
// given number (2-1008) of blocks.
bool ArmoryObject::estimateFee(unsigned int nbBlocks, const FloatCb &cb)
{
   const auto &cbWrap = [this, cb](float fee) {
      if (!cb) {
         return;
      }
      if (needInvokeCb()) {
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
      if (needInvokeCb()) {
         QMetaObject::invokeMethod(this, [cb, fees] { cb(fees); });
      }
      else {
         cb(fees);
      }
   };
   return ArmoryConnection::getFeeSchedule(cbWrap);
}

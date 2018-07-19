#include "ArmoryConnection.h"

#include <QFile>
#include <QProcess>

#include <cassert>
#include <exception>

#include "ClientClasses.h"
#include "DbHeader.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include "JSON_codec.h"
#include "ManualResetEvent.h"
#include "SocketIncludes.h"

const int DefaultArmoryDBStartTimeoutMsec = 500;

#if 0    // Disabled code below is kept for re-implementing it in ArmoryConnection
bool PyBlockDataManager::StartLocalArmoryDB()
{
   const QString armoryDBPath = settings_.armoryExecutablePath;
   if (QFile::exists(armoryDBPath)) {
      armoryProcess_ = std::make_shared<QProcess>();

      QStringList args;
      switch (settings_.netType) {
      case NetworkType::TestNet:
         args.append(QString::fromStdString("--testnet"));
         break;
      case NetworkType::RegTest:
         args.append(QString::fromStdString("--regtest"));
         break;
      default: break;
      }

      std::string spawnID = SecureBinaryData().GenerateRandom(32).toHexStr();
      args.append(QLatin1String("--spawnId=\"") + QString::fromStdString(spawnID) + QLatin1String("\""));
      args.append(QLatin1String("--satoshi-datadir=\"") + settings_.bitcoinBlocksDir + QLatin1String("\""));
      args.append(QLatin1String("--dbdir=\"") + settings_.dbDir + QLatin1String("\""));

      armoryProcess_->start(settings_.armoryExecutablePath, args);
      if (armoryProcess_->waitForStarted(DefaultArmoryDBStartTimeoutMsec)) {
         return true;
      }
      armoryProcess_.reset();
   }
   return false;
}

void PyBlockDataManager::onZCReceived(const std::vector<ClientClasses::LedgerEntry> &entries)
{
   {
      FastLock lock(bdvLock_);
      for (const auto &entry : entries) {
         pendingBroadcasts_.erase(entry.getTxHash());
      }
   }
   emit zeroConfReceived(entries);
}

void PyBlockDataManager::onScheduleRPCBroadcast(const BinaryData& rawTx)
{
   Tx tx(rawTx);

   if (tx.getThisHash().isNull()) {
      qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] TX hash null";
   }

   QTimer::singleShot(30000, [this, tx, rawTx] {
      {
         FastLock lock(bdvLock_);
         if (pendingBroadcasts_.find(tx.getThisHash()) == pendingBroadcasts_.end()) {
            return;
         }
         pendingBroadcasts_.erase(tx.getThisHash());
      }

      const auto &result = bdv_->broadcastThroughRPC(rawTx);
      if (result != "success") {
         qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] broadcast error";
         emit txBroadcastError(QString::fromStdString(tx.getThisHash().toHexStr(true))
            , QString::fromStdString(result));
      } else {
         qDebug() << "[PyBlockDataManager::onScheduleRPCBroadcast] broadcasted through RPC";
      }
   });
}
#endif //0

Q_DECLARE_METATYPE(ArmoryConnection::State)
Q_DECLARE_METATYPE(BDMPhase)
Q_DECLARE_METATYPE(NetworkType)

//==========================================================================================================
ArmoryConnection::ArmoryConnection(const std::shared_ptr<spdlog::logger> &logger, const std::string &txCacheFN)
   : QObject(nullptr)
   , logger_(logger)
   , txCache_(txCacheFN)
   , regThreadRunning_(false)
   , connThreadRunning_(false)
   , maintThreadRunning_(true)
   , reqIdSeq_(1)
   , zcPersistenceTimeout_(30)
{
   qRegisterMetaType<ArmoryConnection::State>();
   qRegisterMetaType<BDMPhase>();
   qRegisterMetaType<NetworkType>();

   const auto &cbZCMaintenance = [this] {
      while (maintThreadRunning_) {
         if (zcMaintCV_.wait_for(std::unique_lock<std::mutex>(zcMaintMutex_), std::chrono::seconds{ 10 })
            != std::cv_status::timeout) {
            if (!maintThreadRunning_) {
               break;
            }
         }

         std::vector<ReqIdType> zcToDelete;
         const auto curTime = std::chrono::system_clock::now();

         FastLock lock(zcLock_);
         for (const auto &zc : zcData_) {
            const std::chrono::duration<double> timeDiff = curTime - zc.second.received;
            if (timeDiff > zcPersistenceTimeout_) {
               zcToDelete.push_back(zc.first);
            }
         }
         for (const auto &reqId : zcToDelete) {
            zcData_.erase(reqId);
         }
      }
   };
   std::thread(cbZCMaintenance).detach();
}

ArmoryConnection::~ArmoryConnection()
{
   stopServiceThreads();
}

void ArmoryConnection::stopServiceThreads()
{
   regThreadRunning_ = false;
   maintThreadRunning_ = false;
   zcMaintCV_.notify_one();
}

void ArmoryConnection::setupConnection(const ArmorySettings &settings)
{
   emit prepareConnection(settings.netType, settings.armoryDBIp, settings.armoryDBPort);

   const auto &registerRoutine = [this, settings] {
      logger_->debug("[ArmoryConnection::registerRoutine] started");
      while (regThreadRunning_) {
         try {
            registerBDV(settings.netType);
            if (!bdv_->getID().empty()) {
               cbRemote_ = std::make_shared<ArmoryCallback>(bdv_->getRemoteCallbackSetupStruct(), this, logger_);
               logger_->debug("[ArmoryConnection::registerRoutine] got BDVid: {}", bdv_->getID());
               setState(State::Connected);
               break;
            }
         }
         catch (const BDVAlreadyRegistered &) {
            logger_->warn("[ArmoryConnection::setup] BDV already registered");
            break;
         }
         catch (const std::exception &e) {
            logger_->error("[ArmoryConnection::setup] registerBDV exception: {}", e.what());
            emit connectionError(QLatin1String(e.what()));
            setState(State::Error);
         }
         catch (...) {
            logger_->error("[ArmoryConnection::setup] registerBDV exception");
            emit connectionError(QString());
         }
         std::this_thread::sleep_for(1s);
      }
      regThreadRunning_ = false;
      logger_->debug("[ArmoryConnection::registerRoutine] completed");
   };

   const auto &connectRoutine = [this, settings, registerRoutine] {
      if (connThreadRunning_) {
         return;
      }
      connThreadRunning_ = true;
      setState(State::Offline);
      stopServiceThreads();
      if (bdv_) {
         bdv_->unregisterFromDB();
         bdv_.reset();
      }
      if (cbRemote_) {
         cbRemote_->shutdown();
         cbRemote_.reset();
      }
      bdv_ = std::make_shared<AsyncClient::BlockDataViewer>(AsyncClient::BlockDataViewer::getNewBDV(
         settings.armoryDBIp, settings.armoryDBPort, SocketWS/*settings.socketType*/));
      logger_->debug("[ArmoryConnection::connectRoutine] BDV connected");

      regThreadRunning_ = true;
      std::thread(registerRoutine).detach();
      connThreadRunning_ = false;
   };
   std::thread(connectRoutine).detach();
}

bool ArmoryConnection::goOnline()
{
   if ((state_ != State::Connected) || !bdv_) {
      logger_->error("[ArmoryConnection::goOnline] invalid state: {}", (int)state_);
      return false;
   }
   bdv_->goOnline();
   return true;
}

unsigned int ArmoryConnection::topBlock() const
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::topBlock] invalid state: {}", (int)state_);
      return 0;
   }
   return bdv_->getTopBlock();
}

void ArmoryConnection::registerBDV(NetworkType netType)
{
   BinaryData magicBytes;
   switch (netType) {
   case NetworkType::TestNet:
      magicBytes = READHEX(TESTNET_MAGIC_BYTES);
      break;
   case NetworkType::RegTest:
      magicBytes = READHEX(REGTEST_MAGIC_BYTES);
      break;
   case NetworkType::MainNet:
      magicBytes = READHEX(MAINNET_MAGIC_BYTES);
      break;
   default:
      throw std::runtime_error("unknown network type");
   }
   bdv_->registerWithDB(magicBytes);
}

void ArmoryConnection::setState(State state)
{
   if (state_ != state) {
      logger_->debug("[ArmoryConnection::setState] from {} to {}", (int)state_, (int)state);
      state_ = state;
      emit stateChanged(state);
   }
}

bool ArmoryConnection::broadcastZC(const BinaryData& rawTx)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
      logger_->error("[ArmoryConnection::broadcastZC] invalid state: {} (BDV null: {})"
         , (int)state_, (bdv_ == nullptr));
      return false;
   }

   Tx tx(rawTx);
   if (!tx.isInitialized() || tx.getThisHash().isNull()) {
      logger_->error("[ArmoryConnection::broadcastZC] invalid TX data (size {}) - aborting broadcast", rawTx.getSize());
      return false;
   }

   bdv_->broadcastZC(rawTx);
   return true;
}

ArmoryConnection::ReqIdType ArmoryConnection::setZC(const std::vector<ClientClasses::LedgerEntry> &entries)
{
   const auto reqId = reqIdSeq_++;
   FastLock lock(zcLock_);
   zcData_[reqId] = ZCData{ std::chrono::system_clock::now(), std::move(entries) };
   return reqId;
}

std::vector<ClientClasses::LedgerEntry> ArmoryConnection::getZCentries(ArmoryConnection::ReqIdType reqId) const
{
   FastLock lock(zcLock_);
   const auto &it = zcData_.find(reqId);
   if (it != zcData_.end()) {
      return it->second.entries;
   }
   return {};
}

std::string ArmoryConnection::registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &wallet
   , const std::string &walletId, const std::vector<BinaryData> &addrVec, bool asNew)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
      logger_->error("[ArmoryConnection::registerWallet] invalid state: {}", (int)state_);
      return {};
   }
   if (!wallet) {
      wallet = std::make_shared<AsyncClient::BtcWallet>(bdv_->instantiateWallet(walletId));
   }
   return wallet->registerAddresses(addrVec, asNew);
}

bool ArmoryConnection::getWalletsHistory(const std::vector<std::string> &walletIDs
   , std::function<void(std::vector<ClientClasses::LedgerEntry>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getWalletsHistory] invalid state: {}", (int)state_);
      return false;
   }
   bdv_->getHistoryForWalletSelection(walletIDs, "ascending", cb);
   return true;
}

bool ArmoryConnection::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr
   , std::function<void(AsyncClient::LedgerDelegate)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getLedgerDelegateForAddress] invalid state: {}", (int)state_);
      return false;
   }
   bdv_->getLedgerDelegateForScrAddr(walletId, addr.id(), cb);
   return true;
}

bool ArmoryConnection::getLedgerDelegatesForAddresses(const std::string &walletId, const std::vector<bs::Address> addresses
   , std::function<void(std::map<bs::Address, AsyncClient::LedgerDelegate>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getLedgerDelegatesForAddresses] invalid state: {}", (int)state_);
      return false;
   }

   auto addrSet = new std::set<bs::Address>;
   auto result = new std::map<bs::Address, AsyncClient::LedgerDelegate>;
   for (const auto &addr : addresses) {
      addrSet->insert(addr);
      const auto &cbProcess = [this, addrSet, result, addr, cb](AsyncClient::LedgerDelegate delegate) {
         addrSet->erase(addr);
         (*result)[addr] = delegate;
         if (addrSet->empty()) {
            delete addrSet;
            cb(*result);
            delete result;
         }
      };
      bdv_->getLedgerDelegateForScrAddr(walletId, addr.id(), cbProcess);
   }
   return true;
}

bool ArmoryConnection::getWalletsLedgerDelegate(std::function<void(AsyncClient::LedgerDelegate)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getWalletsLedgerDelegate] invalid state: {}", (int)state_);
      return false;
   }
   bdv_->getLedgerDelegateForWallets(cb);
   return true;
}

bool ArmoryConnection::getTxByHash(const BinaryData &hash, std::function<void(Tx)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getTxByHash] invalid state: {}", (int)state_);
      return false;
   }
   const auto &tx = txCache_.get(hash);
   if (tx.isInitialized()) {
      cb(tx);
      return true;
   }
   const auto &cbUpdateCache = [this, hash, cb](Tx tx) {
      if (tx.isInitialized()) {
         txCache_.put(hash, tx);
      }
      cb(tx);
   };
   bdv_->getTxByHash(hash, cbUpdateCache);
   return true;
}

bool ArmoryConnection::getTXsByHash(const std::set<BinaryData> &hashes, std::function<void(std::vector<Tx>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getTXsByHash] invalid state: {}", (int)state_);
      return false;
   }
   unsigned int cbCount = 0;
   auto hashSet = new std::set<BinaryData>(hashes);
   auto result = new std::vector<Tx>;

   const auto &cbAppendTx = [this, hashSet, result, cb](Tx tx) {
      hashSet->erase(tx.getThisHash());
      result->emplace_back(tx);
      if (hashSet->empty()) {
         delete hashSet;
         logger_->debug("[ArmoryConnection::getTXsByHash] collected all TX responses");
         cb(*result);
         delete result;
      }
   };
   const auto &cbGetTx = [this, cbAppendTx](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[ArmoryConnection::getTXsByHash] received uninitialized TX");
         return;
      }
      txCache_.put(tx.getThisHash(), tx);
      cbAppendTx(tx);
   };
   for (const auto &hash : hashes) {
      const auto &tx = txCache_.get(hash);
      if (tx.isInitialized()) {
         cbAppendTx(tx);
      }
      else {
         bdv_->getTxByHash(hash, cbGetTx);
      }
   }
   return true;
}

bool ArmoryConnection::estimateFee(unsigned int nbBlocks, std::function<void(float)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::estimateFee] invalid state: {}", (int)state_);
      return false;
   }
   const auto &cbProcess = [cb](ClientClasses::FeeEstimateStruct feeStruct) {
      if (feeStruct.error_.empty()) {
         cb(feeStruct.val_);
      }
      else {
         cb(0);
      }
   };
   bdv_->estimateFee(nbBlocks, FEE_STRAT_CONSERVATIVE, cbProcess);
   return true;
}

unsigned int ArmoryConnection::getConfirmationsNumber(const ClientClasses::LedgerEntry &item) const
{
   if (item.getBlockNum() < uint32_t(-1)) {
      return topBlock() + 1 - item.getBlockNum();
   }
   return 0;
}

bool ArmoryConnection::isTransactionVerified(const ClientClasses::LedgerEntry &item) const
{
   return getConfirmationsNumber(item) >= 6;
}

bool ArmoryConnection::isTransactionConfirmed(const ClientClasses::LedgerEntry &item) const
{
   return getConfirmationsNumber(item) > 1;
}


void ArmoryCallback::progress(BDMPhase phase, const vector<string> &walletIdVec, float progress,
   unsigned secondsRem, unsigned progressNumeric)
{
   logger_->debug("[ArmoryCallback::progress] {}, {} wallets, {} ({}), {} seconds remain", (int)phase, walletIdVec.size()
      , progress, progressNumeric, secondsRem);
   emit connection_->progress(phase, progress, secondsRem, progressNumeric);
}

void ArmoryCallback::run(BDMAction action, void* ptr, int block)
{
   switch (action) {
   case BDMAction_Ready:
      logger_->debug("[ArmoryCallback::run] BDMAction_Ready");
      connection_->setState(ArmoryConnection::State::Ready);
      break;

   case BDMAction_NewBlock:
      logger_->debug("[ArmoryCallback::run] BDMAction_NewBlock");
      connection_->setState(ArmoryConnection::State::Ready);
      emit connection_->newBlock((unsigned int)block);
      break;

   case BDMAction_ZC: {
      logger_->debug("[ArmoryCallback::run] BDMAction_ZC");
      const auto reqId = connection_->setZC(*reinterpret_cast<std::vector<ClientClasses::LedgerEntry>*>(ptr));
      emit connection_->zeroConfReceived(reqId);
      break;
   }

   case BDMAction_Refresh:
      logger_->debug("[ArmoryCallback::run] BDMAction_Refresh");
      if (connection_->state() == ArmoryConnection::State::Ready) {
         emit connection_->refresh(*reinterpret_cast<std::vector<BinaryData> *>(ptr));
      }
      break;

   case BDMAction_NodeStatus: {
      logger_->debug("[ArmoryCallback::run] BDMAction_NodeStatus");
      const auto nodeStatus = *reinterpret_cast<ClientClasses::NodeStatusStruct *>(ptr);
      emit connection_->nodeStatus(nodeStatus.status(), nodeStatus.isSegWitEnabled(), nodeStatus.rpcStatus());
      break;
   }

   case BDMAction_BDV_Error: {
      const auto bdvError = *reinterpret_cast<BDV_Error_Struct *>(ptr);
      logger_->debug("[ArmoryCallback::run] BDMAction_BDV_Error {}, str: {}, msg: {}", (int)bdvError.errType_, bdvError.errorStr_, bdvError.extraMsg_);
      switch (bdvError.errType_) {
      case Error_ZC:
         emit connection_->txBroadcastError(QString::fromStdString(bdvError.extraMsg_), QString::fromStdString(bdvError.errorStr_));
         break;
      default:
         emit connection_->error(QString::fromStdString(bdvError.errorStr_), QString::fromStdString(bdvError.extraMsg_));
         break;
      }
      break;
   }

   default:
      logger_->debug("[ArmoryCallback::run] unknown BDMAction: {}", (int)action);
      break;
   }
}

#include "ArmoryConnection.h"

#include <QFile>
#include <QProcess>

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

const int DefaultArmoryDBStartTimeoutMsec = 500;

Q_DECLARE_METATYPE(ArmoryConnection::State)
Q_DECLARE_METATYPE(BDMPhase)
Q_DECLARE_METATYPE(NetworkType)
Q_DECLARE_METATYPE(NodeStatus)

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
   qRegisterMetaType<NodeStatus>();

   const auto &cbZCMaintenance = [this] {
      while (maintThreadRunning_) {
         std::unique_lock<std::mutex> cvLock(zcMaintMutex_);
         if (zcMaintCV_.wait_for(cvLock, std::chrono::seconds(10))
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
         if (!zcToDelete.empty()) {
            logger_->debug("[ArmoryConnection::zc maintenance] Erasing {} ZC entries"
               , zcToDelete.size());
            for (const auto &reqId : zcToDelete) {
               zcData_.erase(reqId);
            }
         }
      }

      logger_->error("[ArmoryConnection::zc maintenance] stoped");
   };
   std::thread(cbZCMaintenance).detach();
}

ArmoryConnection::~ArmoryConnection() noexcept
{
   stopServiceThreads();
}

void ArmoryConnection::stopServiceThreads()
{
   regThreadRunning_ = false;
   maintThreadRunning_ = false;
   zcMaintCV_.notify_one();
}

bool ArmoryConnection::startLocalArmoryProcess(const ArmorySettings &settings)
{
   if (armoryProcess_ && (armoryProcess_->state() == QProcess::Running)) {
      logger_->info("Armory process {} is already running with PID {}"
         , settings.armoryExecutablePath.toStdString(), armoryProcess_->processId());
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

      args.append(QLatin1String("--satoshi-datadir=\"") + settings.bitcoinBlocksDir + QLatin1String("\""));
      args.append(QLatin1String("--dbdir=\"") + settings.dbDir + QLatin1String("\""));

      armoryProcess_->start(settings.armoryExecutablePath, args);
      if (armoryProcess_->waitForStarted(DefaultArmoryDBStartTimeoutMsec)) {
         return true;
      }
      armoryProcess_.reset();
   }
   return false;
}

void ArmoryConnection::setupConnection(const ArmorySettings &settings)
{
   emit prepareConnection(settings.netType, settings.armoryDBIp, settings.armoryDBPort);

   if (settings.runLocally) {
      if (!startLocalArmoryProcess(settings)) {
         logger_->error("Failed to start Armory from {}", settings.armoryExecutablePath.toStdString());
         setState(State::Offline);
         return;
      }
   }

   const auto &registerRoutine = [this, settings] {
      logger_->debug("[ArmoryConnection::registerRoutine] started");
      while (regThreadRunning_) {
         try {
            registerBDV(settings.netType);
            if (!bdv_->getID().empty()) {
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
         std::this_thread::sleep_for(std::chrono::seconds(10));
      }
      regThreadRunning_ = false;
      logger_->debug("[ArmoryConnection::registerRoutine] completed");
   };

   const auto &connectRoutine = [this, settings, registerRoutine] {
      if (connThreadRunning_) {
         return;
      }
      connThreadRunning_ = true;
      setState(State::Unknown);
      stopServiceThreads();
      if (bdv_) {
         bdv_->unregisterFromDB();
         bdv_.reset();
      }
      if (cbRemote_) {
         cbRemote_.reset();
      }
      isOnline_ = false;
      bool connected = false;
      do {
         cbRemote_ = std::make_shared<ArmoryCallback>(this, logger_);
         logger_->debug("[ArmoryConnection::connectRoutine] connecting to Armory {}:{}", settings.armoryDBIp, settings.armoryDBPort);
         bdv_ = AsyncClient::BlockDataViewer::getNewBDV(settings.armoryDBIp, settings.armoryDBPort, cbRemote_);
         if (!bdv_) {
            logger_->error("[ArmoryConnection::connectRoutine] failed to create BDV");
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
         }
         connected = bdv_->connectToRemote();
         if (!connected) {
            logger_->warn("[ArmoryConnection::connectRoutine] BDV connection failed");
            std::this_thread::sleep_for(std::chrono::seconds(30));
         }
      } while (!connected);
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
      logger_->error("[ArmoryConnection::goOnline] invalid state: {}", static_cast<int>(state_.load()));
      return false;
   }
   bdv_->goOnline();
   isOnline_ = true;
   return true;
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
      logger_->debug("[ArmoryConnection::setState] from {} to {}", (int)state_.load(), (int)state);
      state_ = state;
      emit stateChanged(state);
   }
}

bool ArmoryConnection::broadcastZC(const BinaryData& rawTx)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
      logger_->error("[ArmoryConnection::broadcastZC] invalid state: {} (BDV null: {})"
         , (int)state_.load(), (bdv_ == nullptr));
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
   , const std::string &walletId, const std::vector<BinaryData> &addrVec, std::function<void()> cb
   , bool asNew)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
      logger_->error("[ArmoryConnection::registerWallet] invalid state: {}", (int)state_.load());
      return {};
   }
   if (!wallet) {
      wallet = std::make_shared<AsyncClient::BtcWallet>(bdv_->instantiateWallet(walletId));
   }
   const auto &regId = wallet->registerAddresses(addrVec, asNew);
   if (!isOnline_) {
      preOnlineRegIds_[regId] = cb;
   }
   else {
      if (cb) {
         cb();
      }
   }
   return regId;
}

bool ArmoryConnection::getWalletsHistory(const std::vector<std::string> &walletIDs
   , std::function<void(std::vector<ClientClasses::LedgerEntry>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getWalletsHistory] invalid state: {}", (int)state_.load());
      return false;
   }
   bdv_->getHistoryForWalletSelection(walletIDs, "ascending", cb);
   return true;
}

bool ArmoryConnection::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr
   , std::function<void(AsyncClient::LedgerDelegate)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getLedgerDelegateForAddress] invalid state: {}", (int)state_.load());
      return false;
   }
   bdv_->getLedgerDelegateForScrAddr(walletId, addr.id(), cb);
   return true;
}

bool ArmoryConnection::getLedgerDelegatesForAddresses(const std::string &walletId, const std::vector<bs::Address> addresses
   , std::function<void(std::map<bs::Address, AsyncClient::LedgerDelegate>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getLedgerDelegatesForAddresses] invalid state: {}", (int)state_.load());
      return false;
   }

   auto addrSet = new std::set<bs::Address>;
   auto result = new std::map<bs::Address, AsyncClient::LedgerDelegate>;
   for (const auto &addr : addresses) {
      addrSet->insert(addr);
      const auto &cbProcess = [addrSet, result, addr, cb](AsyncClient::LedgerDelegate delegate) {
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
      logger_->error("[ArmoryConnection::getWalletsLedgerDelegate] invalid state: {}", (int)state_.load());
      return false;
   }
   bdv_->getLedgerDelegateForWallets(cb);
   return true;
}

bool ArmoryConnection::addGetTxCallback(const BinaryData &hash, const std::function<void(Tx)> &cb)
{
   FastLock lock(txCbLock_);
   const auto &it = txCallbacks_.find(hash);
   if (it != txCallbacks_.end()) {
      it->second.push_back(cb);
      return true;
   }
   else {
      txCallbacks_[hash].push_back(cb);
   }
   return false;
}

void ArmoryConnection::callGetTxCallbacks(const BinaryData &hash, const Tx &tx)
{
   std::vector<std::function<void(Tx)>> callbacks;
   {
      FastLock lock(txCbLock_);
      const auto &it = txCallbacks_.find(hash);
      if (it == txCallbacks_.end()) {
         logger_->error("[ArmoryConnection::callGetTxCallbacks] no callbacks found for hash {}", hash.toHexStr(true));
         return;
      }
      callbacks = it->second;
      txCallbacks_.erase(it);
   }
   for (const auto &callback : callbacks) {
      callback(tx);
   }
}

bool ArmoryConnection::getTxByHash(const BinaryData &hash, std::function<void(Tx)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getTxByHash] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &tx = txCache_.get(hash);
   if (tx.isInitialized()) {
      cb(tx);
      return true;
   }
   if (addGetTxCallback(hash, cb)) {
      return true;
   }
   const auto &cbUpdateCache = [this, hash](Tx tx) {
      if (tx.isInitialized()) {
         txCache_.put(hash, tx);
      }
      else {
         logger_->warn("[ArmoryConnection::getTxByHash] received uninited TX for hash {}", hash.toHexStr(true));
      }
      callGetTxCallbacks(hash, tx);
   };
   bdv_->getTxByHash(hash, cbUpdateCache);
   return true;
}

bool ArmoryConnection::getTXsByHash(const std::set<BinaryData> &hashes, std::function<void(std::vector<Tx>)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getTXsByHash] invalid state: {}", (int)state_.load());
      return false;
   }

   auto hashSet = new std::set<BinaryData>(hashes);
   auto result = new std::vector<Tx>;
   const auto origHashes = hashes;

   const auto &cbAppendTx = [hashSet, result, cb](Tx tx) {
      const auto &txHash = tx.getThisHash();
      hashSet->erase(txHash);
      result->emplace_back(tx);
      if (hashSet->empty()) {
         delete hashSet;
         cb(*result);
         delete result;
      }
   };
   const auto &cbUpdateTx = [this, cbAppendTx](Tx tx) {
      if (tx.isInitialized()) {
         txCache_.put(tx.getThisHash(), tx);
      }
      else {
         logger_->error("[ArmoryConnection::getTXsByHash] received uninitialized TX");
      }
      cbAppendTx(tx);
   };
   for (const auto &hash : origHashes) {
      const auto &tx = txCache_.get(hash);
      if (tx.isInitialized()) {
         cbAppendTx(tx);
      }
      else {
         if (addGetTxCallback(hash, cbUpdateTx)) {
            return true;
         }
         bdv_->getTxByHash(hash, [this, hash](Tx tx) {
            callGetTxCallbacks(hash, tx);
         });
      }
   }
   return true;
}

bool ArmoryConnection::getRawHeaderForTxHash(const BinaryData& inHash,
                                             std::function<void(BinaryData)> callback)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getRawHeaderForTxHash] invalid state: {}",
                     (int)state_.load());
      return false;
   }

   // For now, don't worry about chaining callbacks or Tx caches. Just dump
   // everything into the BDV. This may need to change in the future, making the
   // call more like getTxByHash().
   bdv_->getRawHeaderForTxHash(inHash, callback);

   return true;
}

bool ArmoryConnection::getHeaderByHeight(const unsigned& inHeight,
                                         std::function<void(BinaryData)> callback)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::getHeaderByHeight] invalid state: {}",
                     (int)state_.load());
      return false;
   }

   // For now, don't worry about chaining callbacks or Tx caches. Just dump
   // everything into the BDV. This may need to change in the future, making the
   // call more like getTxByHash().
   bdv_->getHeaderByHeight(inHeight, callback);

   return true;
}

bool ArmoryConnection::estimateFee(unsigned int nbBlocks, std::function<void(float)> cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[ArmoryConnection::estimateFee] invalid state: {}", (int)state_.load());
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

unsigned int ArmoryConnection::getConfirmationsNumber(uint32_t blockNum) const
{
   const auto curBlock = topBlock();
   if ((curBlock != UINT32_MAX) && (blockNum < uint32_t(-1))) {
      return curBlock + 1 - blockNum;
   }
   return 0;
}

unsigned int ArmoryConnection::getConfirmationsNumber(const ClientClasses::LedgerEntry &item) const
{
   return getConfirmationsNumber(item.getBlockNum());
}

bool ArmoryConnection::isTransactionVerified(const ClientClasses::LedgerEntry &item) const
{
   return isTransactionVerified(item.getBlockNum());
}

bool ArmoryConnection::isTransactionVerified(uint32_t blockNum) const
{
   return getConfirmationsNumber(blockNum) >= 6;
}

bool ArmoryConnection::isTransactionConfirmed(const ClientClasses::LedgerEntry &item) const
{
   return getConfirmationsNumber(item) > 1;
}

void ArmoryConnection::onRefresh(std::vector<BinaryData> ids)
{
   if (!preOnlineRegIds_.empty()) {
      for (const auto &id : ids) {
         const auto regIdIt = preOnlineRegIds_.find(id.toBinStr());
         if (regIdIt != preOnlineRegIds_.end()) {
            logger_->debug("[ArmoryConnection::onRefresh] found preOnline registration id: {}", id.toBinStr());
            regIdIt->second();
            preOnlineRegIds_.erase(regIdIt);
         }
      }
   }
   if (state_ == ArmoryConnection::State::Ready) {
      std::string idString;
      for (const auto &id : ids) {
         idString += id.toBinStr() + " ";
      }
      logger_->debug("[ArmoryConnection::onRefresh] {}", idString);
      emit refresh(ids);
   }
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
   if (block > 0) {
      connection_->setTopBlock(static_cast<unsigned int>(block));
   }
   switch (action) {
   case BDMAction_Ready:
      logger_->debug("[ArmoryCallback::run] BDMAction_Ready");
      connection_->setState(ArmoryConnection::State::Ready);
      break;

   case BDMAction_NewBlock:
      logger_->debug("[ArmoryCallback::run] BDMAction_NewBlock {}", block);
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
      connection_->onRefresh(*reinterpret_cast<std::vector<BinaryData> *>(ptr));
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

void ArmoryCallback::disconnected()
{
   logger_->debug("[ArmoryCallback::disconnected]");
   connection_->regThreadRunning_ = false;
   connection_->setState(ArmoryConnection::State::Offline);
}

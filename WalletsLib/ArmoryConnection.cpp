#include "ArmoryConnection.h"

#include <cassert>
#include <exception>
#include <condition_variable>
#include <spdlog/spdlog.h>

#include "ClientClasses.h"
#include "DbHeader.h"
#include "EncryptionUtils.h"
#include "JSON_codec.h"
#include "ManualResetEvent.h"
#include "SocketIncludes.h"


ArmoryCallbackTarget::ArmoryCallbackTarget()
{}

ArmoryCallbackTarget::~ArmoryCallbackTarget()
{
   assert(!armory_);
}

void ArmoryCallbackTarget::init(ArmoryConnection *armory)
{
   if (!armory_ && armory) {
      armory_ = armory;
      armory_->addTarget(this);
   }
}

void ArmoryCallbackTarget::cleanup()
{
   if (armory_) {
      armory_->removeTarget(this);
      onDestroy();
   }
}

void ArmoryCallbackTarget::onDestroy()
{
   armory_ = nullptr;
}


ArmoryConnection::ArmoryConnection(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{
   maintThreadRunning_ = true;
   maintThread_ = std::thread(&ArmoryConnection::maintenanceThreadFunc, this);
}

ArmoryConnection::~ArmoryConnection() noexcept
{
   {
      std::unique_lock<std::mutex> lock(actMutex_);
      maintThreadRunning_ = false;
      actCV_.notify_one();
   }
   stopServiceThreads();

   if (cbRemote_) {
      cbRemote_->resetConnection();
   }

   if (maintThread_.joinable()) {
      maintThread_.join();
   }
   for (const auto &tgt : activeTargets_) {
      tgt->onDestroy();
   }
}

bool ArmoryConnection::addTarget(ArmoryCallbackTarget *act)
{
   std::unique_lock<std::mutex> lock(cbMutex_);
   const auto &it = activeTargets_.find(act);
   if (it != activeTargets_.end()) {
      logger_->warn("[ArmoryConnection::addTarget] target {} already exists", (void*)act);
      return false;
   }
   actChanged_ = true;
   activeTargets_.insert(act);
   return true;
}

bool ArmoryConnection::removeTarget(ArmoryCallbackTarget *act)
{
   std::promise<bool> done;
   auto doneFut = done.get_future();

   runOnMaintThread([this, &done, act] () {
      std::unique_lock<std::mutex> lock(cbMutex_);
      const auto &it = activeTargets_.find(act);
      if (it == activeTargets_.end()) {
         logger_->warn("[ArmoryConnection::removeTarget] target {} wasn't added", (void*)act);
         done.set_value(false);
         return;
      }
      actChanged_ = true;
      activeTargets_.erase(it);
      done.set_value(true);
   });

   bool result = doneFut.get();
   return result;
}

void ArmoryConnection::maintenanceThreadFunc()
{
   const auto &forEachTarget = [this](const std::function<void(ArmoryCallbackTarget *tgt)> &cb) {
      do {
         decltype(activeTargets_) tempACT;
         {
            std::unique_lock<std::mutex> lock(cbMutex_);
            actChanged_ = false;
            tempACT = activeTargets_;
         }

         for (const auto &tgt : tempACT) {
            if (!maintThreadRunning_ || actChanged_) {
               break;
            }
            cb(tgt);
         }
      } while (actChanged_ && maintThreadRunning_);
   };

   while (maintThreadRunning_) {
      {
         std::unique_lock<std::mutex> lock(actMutex_);
         if (actQueue_.empty()) {
            actCV_.wait_for(lock, std::chrono::milliseconds{ 100 });
         }
      }
      if (!maintThreadRunning_) {
         break;
      }

      processDelayedZC();

      decltype(actQueue_) tempQueue;
      decltype(runQueue_) tempRunQueue;
      {
         std::unique_lock<std::mutex> lock(actMutex_);
         tempQueue.swap(actQueue_);
         tempRunQueue.swap(runQueue_);
      }

      if (!tempRunQueue.empty()) {
         for (const auto &cb : tempRunQueue) {
            cb();
         }
      }

      if (!tempQueue.empty()) {
         for (const auto &cb : tempQueue) {
            forEachTarget(cb);
            if (!maintThreadRunning_) {
               break;
            }
         }
      }
   }
}

void ArmoryConnection::addMergedWalletId(const std::string &walletId, const std::string &mergedWalletId)
{
   std::unique_lock<std::mutex> lock(zcMutex_);
   zcMergedWalletIds_[walletId] = mergedWalletId;
}

const std::string &ArmoryConnection::getMergedWalletId(const std::string &walletId)
{
   auto it = zcMergedWalletIds_.find(walletId);
   if (it == zcMergedWalletIds_.end()) {
      return walletId;
   }
   return it->second;
}

void ArmoryConnection::addToMaintQueue(const CallbackQueueCb &cb)
{
   std::unique_lock<std::mutex> lock(actMutex_);
   actQueue_.push_back(cb);
   actCV_.notify_one();
}

void ArmoryConnection::runOnMaintThread(ArmoryConnection::EmptyCb cb)
{
   std::unique_lock<std::mutex> lock(actMutex_);
   runQueue_.push_back(std::move(cb));
   actCV_.notify_one();
}

void ArmoryConnection::stopServiceThreads()
{
   regThreadRunning_ = false;
   {
      std::unique_lock<std::mutex> lock(regMutex_);
      regCV_.notify_one();
   }
   if (regThread_.joinable()) {
      regThread_.join();
   }
}

void ArmoryConnection::setupConnection(NetworkType netType, const std::string &host
   , const std::string &port, const std::string &dataDir, const BinaryData &serverKey
   , const BIP151Cb &cbBIP151)
{
   addToMaintQueue([netType, host, port](ArmoryCallbackTarget *tgt) {
      tgt->onPrepareConnection(netType, host, port);
   });

   // Add BIP 150 server keys
   if (!serverKey.isNull()) {
      bsBIP150PubKeys_.push_back(serverKey);
   }

   needsBreakConnectionLoop_.store(false);

   const auto &registerRoutine = [this, netType] {
      logger_->debug("[ArmoryConnection::setupConnection] started");
      while (regThreadRunning_) {
         try {
            registerBDV(netType);
            if (!bdv_->getID().empty()) {
               logger_->debug("[ArmoryConnection::setupConnection] got BDVid: {}", bdv_->getID());
               setState(ArmoryState::Connected);
               break;
            }
         }
         catch (const BDVAlreadyRegistered &) {
            logger_->warn("[ArmoryConnection::setupConnection] BDV already registered");
            break;
         }
         catch (const std::exception &e) {
            logger_->error("[ArmoryConnection::setupConnection] registerBDV exception: {}", e.what());
            setState(ArmoryState::Error);
            addToMaintQueue([e](ArmoryCallbackTarget *tgt) {
               tgt->onError("Connection error", e.what());
            });
         }
         catch (...) {
            logger_->error("[ArmoryConnection::setupConnection] registerBDV exception");
            setState(ArmoryState::Error);
            addToMaintQueue([](ArmoryCallbackTarget *tgt) {
               tgt->onError("Connection error", {});
            });
         }

         std::unique_lock<std::mutex> lock(regMutex_);
         regCV_.wait_for(lock, std::chrono::seconds{ 10 });
      }
      regThreadRunning_ = false;
      logger_->debug("[ArmoryConnection::setupConnection] completed");
   };

   const auto &connectRoutine = [this, registerRoutine, cbBIP151, host, port, dataDir] {
      if (connThreadRunning_) {
         return;
      }
      connThreadRunning_ = true;
      setState(ArmoryState::Connecting);
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
         if (needsBreakConnectionLoop_.load()) {
            setState(ArmoryState::Cancelled);
            return;
         }
         cbRemote_ = std::make_shared<ArmoryCallback>(this, logger_);
         logger_->debug("[ArmoryConnection::setupConnection] connecting to Armory {}:{}"
                        , host, port);

         // Get Armory BDV (gateway to the remote ArmoryDB instance). Must set
         // up BIP 150 keys before connecting. BIP 150/151 is transparent to us
         // otherwise. If it fails, the connection will fail.
         bdv_ = AsyncClient::BlockDataViewer::getNewBDV(host, port
            , dataDir, true // enable ephemeralPeers, because we manage armory keys ourself
            , cbRemote_);

         if (!bdv_) {
            logger_->error("[setupConnection (connectRoutine)] failed to "
               "create BDV");
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
         }

         bdv_->setCheckServerKeyPromptLambda(cbBIP151);

         connected = bdv_->connectToRemote();
         if (!connected) {
            logger_->warn("[ArmoryConnection::setupConnection] BDV connection failed");
            std::this_thread::sleep_for(std::chrono::seconds(30));
         }
      } while (!connected);
      logger_->debug("[ArmoryConnection::setupConnection] BDV connected");

      regThreadRunning_ = true;
      regThread_ = std::thread(registerRoutine);
      connThreadRunning_ = false;
   };
   std::thread(connectRoutine).detach();
}

bool ArmoryConnection::goOnline()
{
   if ((state_ != ArmoryState::Connected) || !bdv_) {
      logger_->error("[ArmoryConnection::goOnline] invalid state: {}"
                     , static_cast<int>(state_.load()));
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

void ArmoryConnection::setTopBlock(unsigned int topBlock)
{
   topBlock_ = topBlock;
}

void ArmoryConnection::setBranchHeight(unsigned int branchHgt)
{
   branchHeight_ = branchHgt;    // not clear where can we use it for now - just saved
}

void ArmoryConnection::setState(ArmoryState state)
{
   if (state_ != state) {
      logger_->debug("[ArmoryConnection::setState] from {} to {}", (int)state_.load(), (int)state);
      state_ = state;
      addToMaintQueue([state](ArmoryCallbackTarget *tgt) {
         tgt->onStateChanged(static_cast<ArmoryState>(state));
      });
   }
}

bool ArmoryConnection::broadcastZC(const BinaryData& rawTx)
{
   if (!bdv_ || ((state_ != ArmoryState::Ready) && (state_ != ArmoryState::Connected))) {
      logger_->error("[ArmoryConnection::broadcastZC] invalid state: {} (BDV null: {})"
         , (int)state_.load(), (bdv_ == nullptr));
      return false;
   }

   if (rawTx.isNull()) {
      SPDLOG_LOGGER_ERROR(logger_, "broadcast failed: empty rawTx");
      return false;
   }

   try
   {
      Tx tx(rawTx);
      if (!tx.isInitialized() || tx.getThisHash().isNull()) {
         logger_->error("[ArmoryConnection::broadcastZC] invalid TX data (size {}) - aborting broadcast"
                        , rawTx.getSize());
         return false;
      }
   } catch (const BlockDeserializingException &e) {
      SPDLOG_LOGGER_ERROR(logger_, "broadcast failed: BlockDeserializingException, details: '{}'", e.what());
      return false;
   } catch (const std::exception &e) {
      SPDLOG_LOGGER_ERROR(logger_, "broadcast failed: {}", e.what());
      return false;
   }

   SPDLOG_LOGGER_DEBUG(logger_, "broadcast new TX: {}", rawTx.toHexStr());

   bdv_->broadcastZC(rawTx);
   return true;
}

std::string ArmoryConnection::registerWallet(const std::shared_ptr<AsyncClient::BtcWallet> &wallet
   , const std::string &walletId, const std::string &mergedWalletId
   , const std::vector<BinaryData> &addrVec, const RegisterWalletCb &cb, bool asNew)
{
   if (!bdv_ || ((state_ != ArmoryState::Ready) && (state_ != ArmoryState::Connected))) {
      logger_->error("[ArmoryConnection::registerWallet] invalid state: {}", (int)state_.load());
      return {};
   }

   std::unique_lock<std::mutex> lock(registrationCallbacksMutex_);

   const auto &regId = wallet->registerAddresses(addrVec, asNew);

   registrationCallbacks_[regId] = cb;

   addMergedWalletId(walletId, mergedWalletId);

   return regId;

   /***
   This triggering of the registration callback does not work in any case. The code 
   needs to wait on the DB refresh signal, as it isn't guaranteed to happen right 
   away, (think fullnode, in home setups). Even with a supernode, the server may
   not process the registration request as soon as it receives it (busy with another
   task). That delay is enough to introduce false positives.
   ***/

   /*if (!isOnline_) {
      preOnlineRegIds_[regId] = cb;
   }
   else {
      if (cb) {
         cb(regId);
      }
   }*/
}

bool ArmoryConnection::getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getWalletsHistory] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb]
                        (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries) {
      try {
         if (cb) {
            cb(entries.get());
         }
      }
      catch(std::exception& e) {
         logger_->error("[ArmoryConnection::getWalletsHistory (cbWrap)] Return data error - {}"
            , e.what());
      }
   };

   bdv_->getHistoryForWalletSelection(walletIDs, "ascending", cbWrap);
   return true;
}

bool ArmoryConnection::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getLedgerDelegateForAddress] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, walletId, addr]
                        (ReturnMessage<AsyncClient::LedgerDelegate> delegate) {
      try {
         auto ld = std::make_shared<AsyncClient::LedgerDelegate>(delegate.get());
         addToMaintQueue([addr, ld] (ArmoryCallbackTarget *tgt) {
            tgt->onLedgerForAddress(addr, ld);
         });
      }
      catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getLedgerDelegateForAddress (cbWrap)] Return data "
            "error - {} - Wallet {} - Address {}", e.what(), walletId
            , addr.display());
         addToMaintQueue([addr](ArmoryCallbackTarget *tgt) {
            tgt->onLedgerForAddress(addr, nullptr);
         });
      }
   };
   bdv_->getLedgerDelegateForScrAddr(walletId, addr.id(), cbWrap);
   return true;
}

bool ArmoryConnection::getWalletsLedgerDelegate(const LedgerDelegateCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getWalletsLedgerDelegate] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb](ReturnMessage<AsyncClient::LedgerDelegate> delegate) {
      try {
         auto ld = std::make_shared< AsyncClient::LedgerDelegate>(delegate.get());
         if (cb) {
            cb(ld);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getWalletsLedgerDelegate (cbWrap)] Return data error "
            "- {}", e.what());
         if (cb) {
            cb(nullptr);
         }
      }
   };
   bdv_->getLedgerDelegateForWallets(cbWrap);
   return true;
}

bool ArmoryConnection::getSpendableTxOutListForValue(const std::vector<std::string> &walletIds
   , uint64_t val, const UTXOsCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getSpendableTxOutListForValue] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb](ReturnMessage<std::vector<UTXO>> retMsg) {
      try {
         const auto &txOutList = retMsg.get();
         if (cb) {
            cb(txOutList);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getSpendableTxOutListForValue] failed: {}", e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getCombinedSpendableTxOutListForValue(walletIds, val, cbWrap);
   return true;
}

bool ArmoryConnection::getSpendableZCoutputs(const std::vector<std::string> &walletIds, const UTXOsCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getSpendableZCoutputs] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb](ReturnMessage<std::vector<UTXO>> retMsg) {
      try {
         const auto &txOutList = retMsg.get();
         if (cb) {
            cb(txOutList);
         }
      } catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getSpendableZCoutputs] failed: {}", e.what());
         if (cb) {
            cb({});
         }
      }
   };

   bdv_->getCombinedSpendableZcOutputs(walletIds, cbWrap);
   return true;
}

bool ArmoryConnection::getNodeStatus(const std::function<void(const std::shared_ptr<::ClientClasses::NodeStatusStruct>)>& userCB)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getNodeStatus] invalid state: {}", (int)state_.load());
      return false;
   }

   if (!userCB) {
      logger_->error("[ArmoryConnection::getNodeStatus] invalid callback");
      return false;
   }

   const auto cbWrap = [this, userCB](ReturnMessage<std::shared_ptr<::ClientClasses::NodeStatusStruct>> reply)
   {
      try {
         const auto nodeStatus = reply.get();
         userCB(nodeStatus);
      } catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getNodeStatus] failed: {}", e.what());
         userCB({});
      }
   };

   bdv_->getNodeStatus(cbWrap);

   return true;
}

bool ArmoryConnection::getRBFoutputs(const std::vector<std::string> &walletIds, const UTXOsCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb](ReturnMessage<std::vector<UTXO>> retMsg) {
      try {
         const auto &txOutList = retMsg.get();
         if (cb) {
            cb(txOutList);
         }
      } catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getRBFoutputs] failed: {}", e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getCombinedRBFTxOuts(walletIds, cbWrap);
   return true;
}

bool ArmoryConnection::getUTXOsForAddress(const bs::Address &addr, const UTXOsCb &cb, bool withZC)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb, addr](ReturnMessage<std::vector<UTXO>> retMsg) {
      try {
         const auto &utxos = retMsg.get();
         if (cb) {
            cb(utxos);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getUTXOsForAddress] {} failed: {}"
            , addr.display(), e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getUTXOsForAddress(addr.id(), withZC, cbWrap);
   return true;
}

bool ArmoryConnection::getOutpointsFor(const std::vector<bs::Address> &addresses
   , const std::function<void(const OutpointBatch &)> &cb
   , unsigned int height, unsigned int zcIndex)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }

   std::set<BinaryData> addrVec;
   for (const auto &addr : addresses) {
      addrVec.insert(addr.prefixed());
   }

   const auto cbWrap = [this, cb, addresses](ReturnMessage<OutpointBatch> opBatch)
   {
      try {
         const auto batch = opBatch.get();
         if (cb) {
            cb(batch);
         }
      } catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getOutpointsFor] {} address[es] failed: {}"
            , addresses.size(), e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getOutpointsForAddresses(addrVec, height, zcIndex, cbWrap);
   return true;
}

bool ArmoryConnection::getCombinedBalances(const std::vector<std::string> &walletIDs
   , const std::function<void(const std::map<std::string, CombinedBalances> &)> &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb](ReturnMessage<std::map<std::string, CombinedBalances>> retMsg)
   {
      try {
         const auto balances = retMsg.get();
         if (cb) {
            cb(balances);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getCombinedBalances] failed to get result: {}", e.what());
      }
   };
   bdv_->getCombinedBalances(walletIDs, cbWrap);
   return true;
}

bool ArmoryConnection::getCombinedTxNs(const std::vector<std::string> &walletIDs
   , const std::function<void(const std::map<std::string, CombinedCounts> &)> &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getCombinedTxNs] invalid state: {}", (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, walletIDs, cb](ReturnMessage<std::map<std::string, CombinedCounts>> retMsg)
   {
      try {
         auto counts = retMsg.get();
         if (counts.empty()) {
            for (const auto &id : walletIDs) {
               counts[id] = {};
            }
         }
         if (cb) {
            cb(counts);
         }
      } catch (const std::exception &e) {
         logger_->error("[ArmoryConnection::getCombinedTxNs] failed to get result: {}", e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getCombinedAddrTxnCounts(walletIDs, cbWrap);
   return true;
}

bool ArmoryConnection::addGetTxCallback(const BinaryData &hash, const TxCb &cb)
{
   std::unique_lock<std::mutex> lock(cbMutex_);

   const auto &it = txCallbacks_.find(hash);
   if (it != txCallbacks_.end()) {
      it->second.push_back(cb);
      return true;
   }

   txCallbacks_[hash].push_back(cb);
   return false;
}

void ArmoryConnection::callGetTxCallbacks(const BinaryData &hash, const Tx &tx)
{
   std::vector<TxCb> callbacks;
   {
      std::unique_lock<std::mutex> lock(cbMutex_);
      const auto &it = txCallbacks_.find(hash);
      if (it == txCallbacks_.end()) {
         logger_->error("[{}] no callbacks found for hash {}", __func__
                        , hash.toHexStr(true));
         return;
      }
      callbacks = it->second;
      txCallbacks_.erase(it);
   }
   for (const auto &callback : callbacks) {
      if (callback) {
         callback(tx);
      }
   }
}

bool ArmoryConnection::getTxByHash(const BinaryData &hash, const TxCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getTxByHash] invalid state: {}", (int)state_.load());
      return false;
   }
   if (addGetTxCallback(hash, cb)) {
      return true;
   }
   const auto &cbUpdateCache = [this, hash](ReturnMessage<Tx> tx)->void {
      try {
         auto retTx = tx.get();
         callGetTxCallbacks(hash, retTx);
      }
      catch (const std::exception &e) {
         logger_->error("[getTxByHash (cbUpdateCache)] Return data error - {} "
            "- hash {}", e.what(), hash.toHexStr());
         callGetTxCallbacks(hash, {});
      }
   };
   bdv_->getTxByHash(hash, cbUpdateCache);
   return true;
}

bool ArmoryConnection::getTXsByHash(const std::set<BinaryData> &hashes, const TXsCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::getTXsByHash] invalid state: {}", (int)state_.load());
      return false;
   }
   if (hashes.empty()) {
      logger_->warn("[ArmoryConnection::getTXsByHash] empty hash set");
      return false;
   }
   if (!cb) {
      logger_->warn("[ArmoryConnection::getTXsByHash] missing callback");
      return false;
   }

   struct Data
   {
      std::mutex m;
      std::set<BinaryData> hashSet;
      std::vector<Tx> result;
   };

   auto data = std::make_shared<Data>();
   data->hashSet = hashes;

   const auto &cbAppendTx = [this, data, cb](const Tx &tx) {
      if (!tx.isInitialized()) {
         logger_->error("[ArmoryConnection::getTXsByHash (cbUpdateTx)] received uninitialized TX");
      }

      bool isEmpty;
      {
         std::lock_guard<std::mutex> lock(data->m);
         const auto &txHash = tx.getThisHash();
         data->hashSet.erase(txHash);
         data->result.emplace_back(tx);
         isEmpty = data->hashSet.empty();
      }

      if (isEmpty) {
         if (cb) {
            cb(data->result);
         }
      }
   };

   for (const auto &hash : hashes) {
      if (addGetTxCallback(hash, cbAppendTx)) {
         continue;
      }
      bdv_->getTxByHash(hash, [this, hash](ReturnMessage<Tx> tx)->void {
         try {
            auto retTx = tx.get();
            callGetTxCallbacks(hash, retTx);
         }
         catch (const std::exception &e) {
            logger_->error("[ArmoryConnection::getTXsByHash (cbUpdateTx)] Return data error - "
               "{} - Hash {}", e.what(), hash.toHexStr(true));
            callGetTxCallbacks(hash, {});
         }
      });
   }
   return true;
}

bool ArmoryConnection::getRawHeaderForTxHash(const BinaryData& inHash, const BinaryDataCb &callback)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}",__func__, (int)state_.load());
      return false;
   }

   // For now, don't worry about chaining callbacks or Tx caches. Just dump
   // everything into the BDV. This may need to change in the future, making the
   // call more like getTxByHash().
   const auto &cbWrap = [this, callback, inHash](ReturnMessage<BinaryData> bd) {
      try {
         if (callback) {
            callback(bd.get());
         }
      }
      catch(std::exception& e) {
         // Switch endian on print to RPC byte order
         logger_->error("[ArmoryConnection::getTXsByHash (cbWrap)] Return data error - "
            "{} - hash {}", e.what(), inHash.toHexStr(true));
      }
   };
   bdv_->getRawHeaderForTxHash(inHash, cbWrap);

   return true;
}

bool ArmoryConnection::getHeaderByHeight(const unsigned int inHeight, const BinaryDataCb &callback)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }

   // For now, don't worry about chaining callbacks or Tx caches. Just dump
   // everything into the BDV. This may need to change in the future, making the
   // call more like getTxByHash().
   const auto &cbWrap = [this, callback, inHeight](ReturnMessage<BinaryData> bd) {
      try {
         if (callback) {
            callback(bd.get());
         }
      }
      catch(std::exception& e) {
         logger_->error("[getHeaderByHeight (cbWrap)] Return data error - {} - "
            "height {}", e.what(), inHeight);
      }
   };
   bdv_->getHeaderByHeight(inHeight, cbWrap);

   return true;
}

// Frontend for Armory's estimateFee() call. Used to get the "conservative" fee
// that Bitcoin Core estimates for successful insertion into a block within a
// given number (2-1008) of blocks.
bool ArmoryConnection::estimateFee(unsigned int nbBlocks, const FloatCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   const auto &cbProcess = [this, cb, nbBlocks](ClientClasses::FeeEstimateStruct feeStruct) {
      if (feeStruct.error_.empty()) {
         if (cb) {
            cb(feeStruct.val_);
         }
      }
      else {
         logger_->warn("[estimateFee (cbProcess)] error '{}' for nbBlocks={}"
            , feeStruct.error_, nbBlocks);
         if (cb) {
            cb(0);
         }
      }
   };
   const auto &cbWrap = [this, cbProcess, cb, nbBlocks]
                        (ReturnMessage<ClientClasses::FeeEstimateStruct> feeStruct) {
      try {
         cbProcess(feeStruct.get());
      }
      catch (const std::exception &e) {
         logger_->error("[estimateFee (cbWrap)] Return data error - {} - {} "
            "blocks", e.what(), nbBlocks);
         if (cb) {
            cb(std::numeric_limits<float>::infinity());
         }
      }
   };
   bdv_->estimateFee(nbBlocks, FEE_STRAT_ECONOMICAL, cbWrap);
   return true;
}

// Frontend for Armory's getFeeSchedule() call. Used to get the range of fees
// that Armory caches. The fees/byte are estimates for what's required to get
// successful insertion of a TX into a block within X number of blocks.
bool ArmoryConnection::getFeeSchedule(const FloatMapCb &cb)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }

   const auto &cbProcess = [this, cb] (std::map<unsigned int
         , ClientClasses::FeeEstimateStruct> feeStructMap) {
      // Create a new map with the # of blocks and the recommended fee/byte.
      std::map<unsigned int, float> feeFloatMap;
      for (auto it : feeStructMap) {
         if (it.second.error_.empty()) {
            feeFloatMap.emplace(it.first, std::move(it.second.val_));
         }
         else {
            logger_->warn("[getFeeSchedule (cbProcess)] error '{}' - {} blocks "
               "- {} sat/byte", it.first, it.second.val_, it.second.error_);
            feeFloatMap.insert(std::pair<unsigned int, float>(it.first, 0.0f));
         }
      }
      if (cb) {
         cb(feeFloatMap);
      }
   };

   const auto &cbWrap = [this, cbProcess]
      (ReturnMessage<std::map<unsigned int,
                              ClientClasses::FeeEstimateStruct>> feeStructMap) {
      try {
         cbProcess(feeStructMap.get());
      }
      catch (const std::exception &e) {
         logger_->error("[getFeeSchedule (cbProcess)] Return data error - {}"
            , e.what());
      }
   };
   bdv_->getFeeSchedule(FEE_STRAT_ECONOMICAL, cbWrap);
   return true;
}

bool ArmoryConnection::pushZC(const BinaryData& rawTx) const
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[ArmoryConnection::pushZC] invalid state: {}", (int)state_.load());
      return false;
   }

   bdv_->broadcastZC(rawTx);
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

void ArmoryConnection::onRefresh(const std::vector<BinaryData>& ids)
{
   logger_->debug("[ArmoryConnection::onRefresh] {} ids", ids.size());


   std::vector< std::pair<std::string, refreshCB> > cbList;
   cbList.reserve(ids.size());

   {
      std::unique_lock<std::mutex> lock(registrationCallbacksMutex_);
      if (!registrationCallbacks_.empty())
      {
         for (const auto &id : ids)
         {
            const auto regIdIt = registrationCallbacks_.find(id.toBinStr());
            if (regIdIt != registrationCallbacks_.end()) {
               logger_->debug("[ArmoryConnection::onRefresh] found preOnline registration id: {}"
                  , id.toBinStr());

               cbList.emplace_back(std::make_pair(regIdIt->first, regIdIt->second));
               registrationCallbacks_.erase(regIdIt);
            }
         }
      }
   }

   //return as soon as possible from this callback, this isn't meant
   //to cascade operations from
   for (const auto& it : cbList) {
      if (it.second) {
         it.second(it.first);
      }
   }

   const bool online = (state_ == ArmoryState::Ready);
   if (logger_->level() <= spdlog::level::debug) {
      std::string idString;
      for (const auto &id : ids) {
         idString += id.toBinStr() + " ";
      }
      logger_->debug("[ArmoryConnection::onRefresh] online={} {}", online, idString);
   }
   addToMaintQueue([ids, online](ArmoryCallbackTarget *tgt) {
      tgt->onRefresh(ids, online);
   });
}

void ArmoryConnection::processDelayedZC()
{
   const auto currentTime = std::chrono::steady_clock::now();

   std::unique_lock<std::mutex> lock(zcMutex_);

   for (auto &waitingEntriesData : zcWaitingEntries_) {
      auto &waitingEntries = waitingEntriesData.second;
      auto &notifiedEntries = zcNotifiedEntries_[waitingEntriesData.first];

      auto it = waitingEntries.begin();
      while (it != waitingEntries.end()) {
         auto &waitingEntry = it->second;
         const auto timeDiff = currentTime - waitingEntry.recvTime;
         if (timeDiff < std::chrono::milliseconds(300)) { // can be tuned later
            ++it;
            continue;
         }

         bs::TXEntry newEntry;
         auto itOld = notifiedEntries.find(waitingEntry.txHash);
         if (itOld != notifiedEntries.end()) {
            auto oldEntry = std::move(itOld->second);
            notifiedEntries.erase(itOld);
            addToMaintQueue([oldEntry](ArmoryCallbackTarget *tgt) {
               tgt->onZCInvalidated({oldEntry});
            });

            newEntry = std::move(oldEntry);
            newEntry.merge(waitingEntry);
         } else {
            newEntry = std::move(waitingEntry);
         }

         notifiedEntries.emplace(newEntry.txHash, newEntry);
         addToMaintQueue([newEntry, this](ArmoryCallbackTarget *tgt) {
            tgt->onZCReceived({newEntry});
         });

         it = waitingEntries.erase(it);
      }
   }
}

void ArmoryConnection::onZCsReceived(const std::vector<std::shared_ptr<ClientClasses::LedgerEntry>> &entries)
{
   auto newEntries = bs::TXEntry::fromLedgerEntries(entries);
   std::vector<bs::TXEntry> immediates;
   std::map<std::string, std::set<BinaryData>> immediateHashes;

   {
      std::unique_lock<std::mutex> lock(zcMutex_);
      for (auto &newEntry : newEntries) {
         std::string mergedWalletId = getMergedWalletId(newEntry.walletId);
         auto &waitingEntries = zcWaitingEntries_[mergedWalletId];
         newEntry.walletId = mergedWalletId;

         auto it = waitingEntries.find(newEntry.txHash);
         if (it != waitingEntries.end()) {
            auto mergedEntry = it->second;
            mergedEntry.merge(newEntry);
            it->second = mergedEntry;
         } else {
            waitingEntries[newEntry.txHash] = newEntry;
            immediateHashes[mergedWalletId].insert(newEntry.txHash);
         }
      }

      for (const auto &imm : immediateHashes) {
         auto &waitingEntries = zcWaitingEntries_[imm.first];
         for (const auto &hash : imm.second) {
            auto it = waitingEntries.find(hash);
            if (it != waitingEntries.end()) {
               immediates.push_back(it->second);
               waitingEntries.erase(it);
            }
         }
      }
   }

   if (!immediates.empty()) {
      addToMaintQueue([immediates](ArmoryCallbackTarget *tgt) {
         tgt->onZCReceived(immediates);
      });
   }
}

void ArmoryConnection::onZCsInvalidated(const std::set<BinaryData> &ids)
{
   std::lock_guard<std::mutex> lock(zcMutex_);

   std::vector<bs::TXEntry> zcInvEntries;
   for (auto &mergedWalletData : zcNotifiedEntries_) {
      auto &notifiedEntries = mergedWalletData.second;
      for (const BinaryData &id : ids) {
         const auto &itEntry = notifiedEntries.find(id);
         if (itEntry != notifiedEntries.end()) {
            zcInvEntries.emplace_back(std::move(itEntry->second));
            notifiedEntries.erase(itEntry);
         }
      }
   }

   if (!zcInvEntries.empty()) {
      addToMaintQueue([zcInvEntries](ArmoryCallbackTarget *tgt) {
         tgt->onZCInvalidated(zcInvEntries);
      });
   }
}

std::shared_ptr<AsyncClient::BtcWallet> ArmoryConnection::instantiateWallet(const std::string &walletId)
{
   if (!bdv_ || (state() == ArmoryState::Offline)) {
      logger_->error("[{}] can't instantiate", __func__);
      return nullptr;
   }
   return std::make_shared<AsyncClient::BtcWallet>(bdv_->instantiateWallet(walletId));
}

float ArmoryConnection::toFeePerByte(float fee)
{
   return float(double(fee) * BTCNumericTypes::BalanceDivider / 1000.0);
}

void ArmoryCallback::progress(BDMPhase phase,
   const std::vector<std::string> &walletIdVec, float progress,
   unsigned secondsRem, unsigned progressNumeric)
{
   std::lock_guard<std::mutex> lock(mutex_);

   logger_->debug("[{}] {}, {} wallets, {} ({}), {} seconds remain", __func__
                  , (int)phase, walletIdVec.size(), progress, progressNumeric
                  , secondsRem);
   if (connection_) {
      connection_->addToMaintQueue([phase, progress, secondsRem, progressNumeric]
      (ArmoryCallbackTarget *tgt) {
         tgt->onLoadProgress(phase, progress, secondsRem, progressNumeric);
      });
   }
}

void ArmoryCallback::run(BdmNotification bdmNotif)
{
   std::lock_guard<std::mutex> lock(mutex_);

   if (!connection_) {
      return;
   }

   switch (bdmNotif.action_) {
   case BDMAction_Ready:
      logger_->debug("[ArmoryCallback::run] BDMAction_Ready");
      connection_->setTopBlock(bdmNotif.height_);
      connection_->setState(ArmoryState::Ready);
      break;

   case BDMAction_NewBlock:
      logger_->debug("[ArmoryCallback::run] BDMAction_NewBlock {}", bdmNotif.height_);
      connection_->setBranchHeight(bdmNotif.branchHeight_);
      connection_->setTopBlock(bdmNotif.height_);
      connection_->setState(ArmoryState::Ready);
      connection_->addToMaintQueue([height=bdmNotif.height_](ArmoryCallbackTarget *tgt) {
         tgt->onNewBlock(height);
      });
      break;

   case BDMAction_ZC:
      logger_->debug("[ArmoryCallback::run] BDMAction_ZC");
      connection_->onZCsReceived(bdmNotif.ledgers_);
      break;

   case BDMAction_InvalidatedZC:
      logger_->debug("[ArmoryCallback::run] BDMAction_InvalidateZC");
      connection_->onZCsInvalidated(bdmNotif.invalidatedZc_);
      break;

   case BDMAction_Refresh:
      logger_->debug("[ArmoryCallback::run] BDMAction_Refresh");
      connection_->onRefresh(bdmNotif.ids_);
      break;

   case BDMAction_NodeStatus: {
      const auto nodeStatus = *bdmNotif.nodeStatus_;
      logger_->debug("[ArmoryCallback::run] BDMAction_NodeStatus: status={}, RPC status={}"
         , (int)nodeStatus.status(), (int)nodeStatus.rpcStatus());
      connection_->addToMaintQueue([nodeStatus](ArmoryCallbackTarget *tgt) {
         tgt->onNodeStatus(nodeStatus.status(), nodeStatus.isSegWitEnabled(), nodeStatus.rpcStatus());
      });
      break;
   }

   case BDMAction_BDV_Error: {
      const auto bdvError = bdmNotif.error_;
      logger_->debug("[ArmoryCallback::run] BDMAction_BDV_Error {}, str: {}, msg: {}"
                     , (int)bdvError.errType_, bdvError.errorStr_
                     , bdvError.extraMsg_);
      switch (bdvError.errType_) {
      case Error_ZC:
         connection_->addToMaintQueue([bdvError](ArmoryCallbackTarget *tgt) {
            tgt->onTxBroadcastError(bdvError.extraMsg_, bdvError.errorStr_);
         });
         break;
      default:
         connection_->addToMaintQueue([bdvError](ArmoryCallbackTarget *tgt) {
            tgt->onError(bdvError.errorStr_, bdvError.extraMsg_);
         });
         break;
      }
      break;
   }

   default:
      logger_->debug("[ArmoryCallback::run] unknown BDMAction: {}", (int)bdmNotif.action_);
      break;
   }
}

void ArmoryCallback::disconnected()
{
   logger_->debug("[ArmoryCallback::disconnected]");
   std::lock_guard<std::mutex> lock(mutex_);
   if (connection_) {
      connection_->regThreadRunning_ = false;
      if (connection_->state() != ArmoryState::Cancelled) {
         connection_->setState(ArmoryState::Offline);
      }
   }
}

void ArmoryCallback::resetConnection()
{
   std::lock_guard<std::mutex> lock(mutex_);
   connection_ = nullptr;
}

void bs::TXEntry::merge(const bs::TXEntry &other)
{
   value += other.value;
   blockNum = other.blockNum;
   merged = true;
}

bs::TXEntry bs::TXEntry::fromLedgerEntry(const ClientClasses::LedgerEntry &entry)
{
   return { entry.getTxHash(), entry.getID(), entry.getValue(), entry.getBlockNum()
         , entry.getTxTime(), entry.isOptInRBF(), entry.isChainedZC(), false
         , std::chrono::steady_clock::now() };
}

std::vector<bs::TXEntry> bs::TXEntry::fromLedgerEntries(const std::vector<ClientClasses::LedgerEntry> &entries)
{
   // Looks like we don't need to merge TXs here (like it was done before).
   // So there would be two TX when two different local wallets are used (with different wallet IDs),
   // but only one for internal TX (if addresses from same wallet are used).
   std::vector<bs::TXEntry> result;
   for (const auto &entry : entries) {
      result.emplace_back(fromLedgerEntry(entry));
   }
   return result;
}

std::vector<bs::TXEntry> bs::TXEntry::fromLedgerEntries(const std::vector<std::shared_ptr<ClientClasses::LedgerEntry>> &entries)
{
   std::vector<bs::TXEntry> result;
   for (const auto &entry : entries) {
      result.emplace_back(fromLedgerEntry(*entry));
   }
   return result;
}

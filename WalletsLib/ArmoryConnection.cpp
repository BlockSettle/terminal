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


ArmoryCallbackTarget::ArmoryCallbackTarget(ArmoryConnection *armory)
   : armory_(armory)
{
   if (armory_) {
      armory_->addTarget(this);
   }
}

ArmoryCallbackTarget::~ArmoryCallbackTarget()
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
      logger_->warn("[{}] target {} already exists", __func__, (void*)act);
      return false;
   }
   actChanged_ = true;
   activeTargets_.insert(act);
   return true;
}

bool ArmoryConnection::removeTarget(ArmoryCallbackTarget *act)
{
   std::unique_lock<std::mutex> lock(cbMutex_);
   const auto &it = activeTargets_.find(act);
   if (it == activeTargets_.end()) {
      logger_->warn("[{}] target {} wasn't added", __func__, (void*)act);
      return false;
   }
   actChanged_ = true;
   activeTargets_.erase(it);
   return true;
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
      {
         std::unique_lock<std::mutex> lock(actMutex_);
         tempQueue.swap(actQueue_);
      }
      if (tempQueue.empty()) {
         continue;
      }

      for (const auto &cb : tempQueue) {
         forEachTarget(cb);
         if (!maintThreadRunning_) {
            break;
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
            break;
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

         // There is a problem with armory keys: we must delete old keys before importing them
         // (AuthorizedPeers does not replace them, see AuthorizedPeers::addPeer for details).
         // Because we manage keys using bip150PromptUserRoutine callback this cause a problem
         // when Armory key changes (it will NOT be replaced despite we accept it trough the callback).
         // If we don't add keys there it works fine.
#if 0
         try {
            for (const auto &x : bsBIP150PubKeys_) {
               bdv_->addPublicKey(x);
            }
         }
         catch (...) {}
#endif

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
      logger_->error("[{}] invalid state: {}", __func__
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

void ArmoryConnection::setState(ArmoryState state)
{
   if (state_ != state) {
      logger_->debug("[{}] from {} to {}", __func__, (int)state_.load(), (int)state);
      state_ = state;
      addToMaintQueue([state](ArmoryCallbackTarget *tgt) {
         tgt->onStateChanged(static_cast<ArmoryState>(state));
      });
   }
}

bool ArmoryConnection::broadcastZC(const BinaryData& rawTx)
{
   if (!bdv_ || ((state_ != ArmoryState::Ready) && (state_ != ArmoryState::Connected))) {
      logger_->error("[{}] invalid state: {} (BDV null: {})", __func__
         , (int)state_.load(), (bdv_ == nullptr));
      return false;
   }

   Tx tx(rawTx);
   if (!tx.isInitialized() || tx.getThisHash().isNull()) {
      logger_->error("[{}] invalid TX data (size {}) - aborting broadcast"
                     , __func__, rawTx.getSize());
      return false;
   }

   bdv_->broadcastZC(rawTx);
   return true;
}

std::string ArmoryConnection::registerWallet(const std::shared_ptr<AsyncClient::BtcWallet> &wallet
   , const std::string &walletId, const std::string &mergedWalletId
   , const std::vector<BinaryData> &addrVec, const RegisterWalletCb &cb, bool asNew)
{
   if (!bdv_ || ((state_ != ArmoryState::Ready) && (state_ != ArmoryState::Connected))) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
         logger_->error("[getWalletsHistory (cbWrap)] Return data error - {}"
            , e.what());
      }
   };

   bdv_->getHistoryForWalletSelection(walletIDs, "ascending", cbWrap);
   return true;
}

bool ArmoryConnection::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr)
{
   if (!bdv_ || (state_ != ArmoryState::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
         logger_->error("[getLedgerDelegateForAddress (cbWrap)] Return data "
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
         logger_->error("[getWalletsLedgerDelegate (cbWrap)] Return data error "
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
         logger_->error("[ArmoryConnection::getSpendableZCoutputs] failed: {}", e.what());
         if (cb) {
            cb({});
         }
      }
   };
   bdv_->getCombinedSpendableZcOutputs(walletIds, cbWrap);
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   if (hashes.empty()) {
      logger_->warn("[{}] empty hash set", __func__);
      return false;
   }
   if (!cb) {
      logger_->warn("[{}] missing callback", __func__);
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
         logger_->error("[getTXsByHash (cbUpdateTx)] received uninitialized TX");
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
            logger_->error("[getTXsByHash (cbUpdateTx)] Return data error - "
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
         logger_->error("[getRawHeaderForTxHash (cbWrap)] Return data error - "
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
   bdv_->estimateFee(nbBlocks, FEE_STRAT_CONSERVATIVE, cbWrap);
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
   bdv_->getFeeSchedule(FEE_STRAT_CONSERVATIVE, cbWrap);
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
   {
      std::unique_lock<std::mutex> lock(registrationCallbacksMutex_);
      if (!registrationCallbacks_.empty())
      {
         for (const auto &id : ids)
         {
            const auto regIdIt = registrationCallbacks_.find(id.toBinStr());
            if (regIdIt != registrationCallbacks_.end()) {
               logger_->debug("[{}] found preOnline registration id: {}", __func__
                  , id.toBinStr());
               const auto regId = regIdIt->first;
               const auto cb = regIdIt->second;
               registrationCallbacks_.erase(regIdIt);

               //return as soon as possible from this callback, this isn't meant
               //to cascade operations from
               if (cb) {
                  cb(regId);
               }
            }
         }
      }
   }

   const bool online = (state_ == ArmoryState::Ready);
   if (logger_->level() <= spdlog::level::debug) {
      std::string idString;
      for (const auto &id : ids) {
         idString += id.toBinStr() + " ";
      }
      logger_->debug("[{}] online={} {}", __func__, online, idString);
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
         if (timeDiff < std::chrono::milliseconds(2300)) { // can be tuned later
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
         addToMaintQueue([newEntry](ArmoryCallbackTarget *tgt) {
            tgt->onZCReceived({newEntry});
         });

         it = waitingEntries.erase(it);
      }
   }
}

void ArmoryConnection::onZCsReceived(const std::vector<ClientClasses::LedgerEntry> &entries)
{
   std::vector<bs::TXEntry> immediates;
   auto newEntries = bs::TXEntry::fromLedgerEntries(entries);

   std::unique_lock<std::mutex> lock(zcMutex_);

   for (auto &newEntry : newEntries) {
      std::string mergedWalletId = getMergedWalletId(newEntry.walletId);
      auto &waitingEntries = zcWaitingEntries_[mergedWalletId];
      newEntry.walletId = mergedWalletId;

      auto it = waitingEntries.find(newEntry.txHash);

      if (it == waitingEntries.end()) {
         waitingEntries.emplace(newEntry.txHash, newEntry);
         continue;
      }

      auto mergedEntry = std::move(it->second);
      waitingEntries.erase(it);

      auto &notifiedEntries = zcNotifiedEntries_[mergedWalletId];
      mergedEntry.merge(newEntry);
      notifiedEntries.emplace(mergedEntry.txHash, mergedEntry);
      immediates.push_back(std::move(mergedEntry));
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


void ArmoryCallback::progress(BDMPhase phase,
   const std::vector<std::string> &walletIdVec, float progress,
   unsigned secondsRem, unsigned progressNumeric)
{
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

void ArmoryCallback::run(BDMAction action, void* ptr, int block)
{
   if (!connection_) {
      return;
   }
   if (block > 0) {
      connection_->setTopBlock(static_cast<unsigned int>(block));
   }
   switch (action) {
   case BDMAction_Ready:
      logger_->debug("[{}] BDMAction_Ready", __func__);
      connection_->setState(ArmoryState::Ready);
      break;

   case BDMAction_NewBlock:
      logger_->debug("[{}] BDMAction_NewBlock {}", __func__, block);
      connection_->setState(ArmoryState::Ready);
      connection_->addToMaintQueue([block](ArmoryCallbackTarget *tgt) {
         tgt->onNewBlock(block);
      });
      break;

   case BDMAction_ZC:
      logger_->debug("[{}] BDMAction_ZC", __func__);
      connection_->onZCsReceived(*reinterpret_cast<std::vector<ClientClasses::LedgerEntry>*>(ptr));
      break;

   case BDMAction_InvalidatedZC:
      logger_->debug("[{}] BDMAction_InvalidateZC", __func__);
      connection_->onZCsInvalidated(*reinterpret_cast<std::set<BinaryData> *>(ptr));
      break;

   case BDMAction_Refresh:
      logger_->debug("[{}] BDMAction_Refresh", __func__);
      connection_->onRefresh(*reinterpret_cast<std::vector<BinaryData> *>(ptr));
      break;

   case BDMAction_NodeStatus: {
      logger_->debug("[{}] BDMAction_NodeStatus", __func__);
      const auto nodeStatus = *reinterpret_cast<ClientClasses::NodeStatusStruct *>(ptr);
      connection_->addToMaintQueue([nodeStatus](ArmoryCallbackTarget *tgt) {
         tgt->onNodeStatus(nodeStatus.status(), nodeStatus.isSegWitEnabled(), nodeStatus.rpcStatus());
      });
      break;
   }

   case BDMAction_BDV_Error: {
      const auto bdvError = *reinterpret_cast<BDV_Error_Struct *>(ptr);
      logger_->debug("[{}] BDMAction_BDV_Error {}, str: {}, msg: {}", __func__
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
      logger_->debug("[{}] unknown BDMAction: {}", __func__, (int)action);
      break;
   }
}

void ArmoryCallback::disconnected()
{
   logger_->debug("[{}]", __func__);
   if (connection_) {
      connection_->regThreadRunning_ = false;
      if (connection_->state() != ArmoryState::Cancelled) {
         connection_->setState(ArmoryState::Offline);
      }
   }
}


void bs::TXEntry::merge(const bs::TXEntry &other)
{
   value += other.value;
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

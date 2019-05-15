#include "ArmoryConnection.h"

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

ArmoryConnection::ArmoryConnection(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , regThreadRunning_(false)
   , connThreadRunning_(false)
   , maintThreadRunning_(true)
{}

ArmoryConnection::~ArmoryConnection() noexcept
{
   maintThreadRunning_ = false;
   stopServiceThreads();
}

void ArmoryConnection::stopServiceThreads()
{
   regThreadRunning_ = false;
}

void ArmoryConnection::setupConnection(NetworkType netType, const std::string &host
   , const std::string &port, const std::string &dataDir, const BinaryData &serverKey
   , const StringCb &cbError, const BIP151Cb &cbBIP151)
{
   // Add BIP 150 server keys
   if (!serverKey.isNull()) {
      bsBIP150PubKeys_.push_back(serverKey);
   }

   needsBreakConnectionLoop_.store(false);

   const auto &registerRoutine = [this, netType, cbError] {
      logger_->debug("[ArmoryConnection::setupConnection] started");
      while (regThreadRunning_) {
         try {
            registerBDV(netType);
            if (!bdv_->getID().empty()) {
               logger_->debug("[ArmoryConnection::setupConnection] got BDVid: {}", bdv_->getID());
               setState(State::Connected);
               break;
            }
         }
         catch (const BDVAlreadyRegistered &) {
            logger_->warn("[ArmoryConnection::setupConnection] BDV already registered");
            break;
         }
         catch (const std::exception &e) {
            logger_->error("[ArmoryConnection::setupConnection] registerBDV exception: {}", e.what());
            cbError(e.what());
            setState(State::Error);
         }
         catch (...) {
            logger_->error("[ArmoryConnection::setupConnection] registerBDV exception");
            cbError("");
         }
         std::this_thread::sleep_for(std::chrono::seconds(10));
      }
      regThreadRunning_ = false;
      logger_->debug("[ArmoryConnection::setupConnection] completed");
   };

   const auto &connectRoutine = [this, registerRoutine, cbBIP151, host, port, dataDir] {
      if (connThreadRunning_) {
         return;
      }
      connThreadRunning_ = true;
      setState(State::Connecting);
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
            setState(State::Cancelled);
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
      std::thread(registerRoutine).detach();
      connThreadRunning_ = false;
   };
   std::thread(connectRoutine).detach();
}

bool ArmoryConnection::goOnline()
{
   if ((state_ != State::Connected) || !bdv_) {
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

void ArmoryConnection::setState(State state)
{
   if (state_ != state) {
      logger_->debug("[{}] from {} to {}", __func__, (int)state_.load(), (int)state);
      state_ = state;
      if (cbStateChanged_) {
         cbStateChanged_(state);
      }
   }
}

bool ArmoryConnection::broadcastZC(const BinaryData& rawTx)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
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

std::string ArmoryConnection::registerWallet(std::shared_ptr<AsyncClient::BtcWallet> &wallet
   , const std::string &walletId, const std::vector<BinaryData> &addrVec
   , const RegisterWalletCb &cb, bool asNew)
{
   if (!bdv_ || ((state_ != State::Ready) && (state_ != State::Connected))) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
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
         cb(regId);
      }
   }
   return regId;
}

bool ArmoryConnection::getWalletsHistory(const std::vector<std::string> &walletIDs, const WalletsHistoryCb &cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
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

bool ArmoryConnection::getLedgerDelegateForAddress(const std::string &walletId, const bs::Address &addr
   , const LedgerDelegateCb &cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
      logger_->error("[{}] invalid state: {}", __func__, (int)state_.load());
      return false;
   }
   const auto &cbWrap = [this, cb, walletId, addr]
                        (ReturnMessage<AsyncClient::LedgerDelegate> delegate) {
      try {
         auto ld = std::make_shared<AsyncClient::LedgerDelegate>(delegate.get());
         if (cb) {
            cb(ld);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[getLedgerDelegateForAddress (cbWrap)] Return data "
            "error - {} - Wallet {} - Address {}", e.what(), walletId
            , addr.display());
      }
   };
   bdv_->getLedgerDelegateForScrAddr(walletId, addr.id(), cbWrap);
   return true;
}

bool ArmoryConnection::getWalletsLedgerDelegate(const LedgerDelegateCb &cb)
{
   if (!bdv_ || (state_ != State::Ready)) {
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
      }
   };
   bdv_->getLedgerDelegateForWallets(cbWrap);
   return true;
}

bool ArmoryConnection::addGetTxCallback(const BinaryData &hash, const TxCb &cb)
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
   std::vector<TxCb> callbacks;
   {
      FastLock lock(txCbLock_);
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
   if (!bdv_ || (state_ != State::Ready)) {
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
   if (!bdv_ || (state_ != State::Ready)) {
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

   auto hashSet = std::make_shared<std::set<BinaryData>>(hashes);
   auto result = std::make_shared<std::vector<Tx>>();
   const auto origHashes = hashes;

   const auto &cbAppendTx = [this, hashSet, result, cb](Tx tx) {
      const auto &txHash = tx.getThisHash();
      hashSet->erase(txHash);
      result->emplace_back(tx);
      if (hashSet->empty()) {
         if (cb) {
            cb(*result);
         }
      }
   };
   const auto &cbUpdateTx = [this, cbAppendTx](Tx tx) {
      if (!tx.isInitialized()) {
         logger_->error("[getTXsByHash (cbUpdateTx)] received uninitialized TX");
      }
      cbAppendTx(tx);
   };
   for (const auto &hash : origHashes) {
      if (addGetTxCallback(hash, cbUpdateTx)) {
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
   if (!bdv_ || (state_ != State::Ready)) {
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
   if (!bdv_ || (state_ != State::Ready)) {
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
   if (!bdv_ || (state_ != State::Ready)) {
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
   const auto &cbWrap = [this, cbProcess, nbBlocks]
                        (ReturnMessage<ClientClasses::FeeEstimateStruct> feeStruct) {
      try {
         cbProcess(feeStruct.get());
      }
      catch (const std::exception &e) {
         logger_->error("[estimateFee (cbWrap)] Return data error - {} - {} "
            "blocks", e.what(), nbBlocks);
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
   if (!bdv_ || (state_ != State::Ready)) {
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
   if (!preOnlineRegIds_.empty()) {
      for (const auto &id : ids) {
         const auto regIdIt = preOnlineRegIds_.find(id.toBinStr());
         if (regIdIt != preOnlineRegIds_.end()) {
            logger_->debug("[{}] found preOnline registration id: {}", __func__
                           , id.toBinStr());
            const auto regId = regIdIt->first;
            const auto cb = regIdIt->second;
            if (cb) {
               cb(regId);
            }
            preOnlineRegIds_.erase(regIdIt);
         }
      }
   }
   const bool online = (state_ == ArmoryConnection::State::Ready);
   if (logger_->level() <= spdlog::level::debug) {
      std::string idString;
      for (const auto &id : ids) {
         idString += id.toBinStr() + " ";
      }
      logger_->debug("[{}] online={} {}", __func__, online, idString);
   }
   for (const auto &cb : cbRefresh_) {
      cb.second(ids, online);
   }
}

void ArmoryConnection::onZCsReceived(const std::vector<ClientClasses::LedgerEntry> &entries)
{
   const auto txEntries = bs::TXEntry::fromLedgerEntries(entries);
   for (const auto &entry : txEntries) {
      zcEntries_[entry.txHash] = entry;
   }
   if (cbZCReceived_) {
      cbZCReceived_(txEntries);
   }
}

void ArmoryConnection::onZCsInvalidated(const std::set<BinaryData> &ids)
{
   std::vector<bs::TXEntry> zcInvEntries;
   for (const auto &id : ids) {
      const auto &itEntry = zcEntries_.find(id);
      if (itEntry != zcEntries_.end()) {
         zcInvEntries.emplace_back(std::move(itEntry->second));
         zcEntries_.erase(itEntry);
      }
   }

   if (cbZCInvalidated_) {
      cbZCInvalidated_(zcInvEntries);
   }
}

unsigned int ArmoryConnection::setRefreshCb(const RefreshCb &cb)
{
   const auto reqId = cbSeqNo_++;
   cbRefresh_[reqId] = cb;
   return reqId;
}

bool ArmoryConnection::unsetRefreshCb(unsigned int reqId)
{
   return (cbRefresh_.erase(reqId) > 0);
}


void ArmoryCallback::progress(BDMPhase phase,
   const std::vector<std::string> &walletIdVec, float progress,
   unsigned secondsRem, unsigned progressNumeric)
{
   logger_->debug("[{}] {}, {} wallets, {} ({}), {} seconds remain", __func__
                  , (int)phase, walletIdVec.size(), progress, progressNumeric
                  , secondsRem);
   if (connection_->cbProgress_) {
      connection_->cbProgress_(phase, progress, secondsRem, progressNumeric);
   }
}

void ArmoryCallback::run(BDMAction action, void* ptr, int block)
{
   if (block > 0) {
      connection_->setTopBlock(static_cast<unsigned int>(block));
   }
   switch (action) {
   case BDMAction_Ready:
      logger_->debug("[{}] BDMAction_Ready", __func__);
      connection_->setState(ArmoryConnection::State::Ready);
      break;

   case BDMAction_NewBlock:
      logger_->debug("[{}] BDMAction_NewBlock {}", __func__, block);
      connection_->setState(ArmoryConnection::State::Ready);
      if (connection_->cbNewBlock_) {
         connection_->cbNewBlock_((unsigned int)block);
      }
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
      if (connection_->cbNodeStatus_) {
         connection_->cbNodeStatus_(nodeStatus.status(), nodeStatus.isSegWitEnabled(), nodeStatus.rpcStatus());
      }
      break;
   }

   case BDMAction_BDV_Error: {
      const auto bdvError = *reinterpret_cast<BDV_Error_Struct *>(ptr);
      logger_->debug("[{}] BDMAction_BDV_Error {}, str: {}, msg: {}", __func__
                     , (int)bdvError.errType_, bdvError.errorStr_
                     , bdvError.extraMsg_);
      switch (bdvError.errType_) {
      case Error_ZC:
         if (connection_->cbTxBcError_) {
            connection_->cbTxBcError_(bdvError.extraMsg_, bdvError.errorStr_);
         }
         break;
      default:
         if (connection_->cbError_) {
            connection_->cbError_(bdvError.errorStr_, bdvError.extraMsg_);
         }
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
   connection_->regThreadRunning_ = false;
   if (connection_->state() != ArmoryConnection::State::Cancelled) {
      connection_->setState(ArmoryConnection::State::Offline);
   }
}


bs::TXEntry bs::TXEntry::fromLedgerEntry(const ClientClasses::LedgerEntry &entry)
{
   return { entry.getTxHash(), entry.getID(), entry.getValue(), entry.getBlockNum()
         , entry.getTxTime(), entry.isOptInRBF(), entry.isChainedZC() };
}

std::vector<bs::TXEntry> bs::TXEntry::fromLedgerEntries(std::vector<ClientClasses::LedgerEntry> entries)
{
   std::vector<bs::TXEntry> result;
   for (const auto &entry : entries) {
      result.push_back(fromLedgerEntry(entry));
   }
   return result;
}

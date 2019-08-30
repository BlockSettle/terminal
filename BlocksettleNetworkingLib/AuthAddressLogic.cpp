#include "AuthAddressLogic.h"

constexpr uint64_t AUTH_VALUE_THRESHOLD = 1000;

///////////////////////////////////////////////////////////////////////////////
void ValidationAddressACT::onRefresh(const std::vector<BinaryData>& ids, bool online)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
   dbns->ids_ = ids;
   dbns->online_ = online;

   notifQueue_.push_back(std::move(dbns));
}

////
void ValidationAddressACT::onZCReceived(const std::vector<bs::TXEntry> &zcs)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
   dbns->zc_ = zcs;

   notifQueue_.push_back(std::move(dbns));
}

////
void ValidationAddressACT::onNewBlock(unsigned int height)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
   dbns->block_ = height;

   notifQueue_.push_back(std::move(dbns));
}

////
void ValidationAddressACT::processNotification()
{
   while (true) {
      std::shared_ptr<DBNotificationStruct> dbNotifPtr;
      try {
         dbNotifPtr = notifQueue_.pop_front();
      }
      catch (StopBlockingLoop&) {
         break;
      }

      switch (dbNotifPtr->type_) {
      case DBNS_NewBlock:
      case DBNS_ZC:
         vamPtr_->update();
         break;

      case DBNS_Refresh:
         vamPtr_->pushRefreshID(dbNotifPtr->ids_);
         break;

      default:
         throw std::runtime_error("unexpected notification type");
      }
   }
}

////
void ValidationAddressACT::start()
{
   if (vamPtr_ == nullptr) {
      throw std::runtime_error("null validation address manager ptr");
   }
   auto thrLbd = [this](void)->void
   {
      processNotification();
   };

   processThr_ = std::thread(thrLbd);
}

////
void ValidationAddressACT::stop()
{
   notifQueue_.terminate();

   if (processThr_.joinable()) {
      processThr_.join();
   }
}

///////////////////////////////////////////////////////////////////////////////
ValidationAddressManager::ValidationAddressManager(
   std::shared_ptr<ArmoryConnection> conn) :
   connPtr_(conn)
{
   ready_.store(false, std::memory_order_relaxed);

   auto&& wltIdSbd = CryptoPRNG::generateRandom(12);
   walletObj_ = connPtr_->instantiateWallet(wltIdSbd.toHexStr());
}

////
void ValidationAddressManager::pushRefreshID(std::vector<BinaryData>& idVec)
{
   for (auto& id : idVec) {
      refreshQueue_.push_back(std::move(id));
   }
}

////
void ValidationAddressManager::waitOnRefresh(const std::string& id)
{
   BinaryDataRef idRef;
   idRef.setRef(id);

   while (true) {
      auto&& notifId = refreshQueue_.pop_front();
      if (notifId == idRef) {
         break;
      }
   }
}

////
void ValidationAddressManager::setCustomACT(const
   std::shared_ptr<ValidationAddressACT> &actPtr)
{
   //have to set the ACT before going online
   if (ready_.load(std::memory_order_relaxed)) {
      throw std::runtime_error("ValidationAddressManager is already online");
   }
   actPtr_ = actPtr;
}

////
std::shared_ptr<ValidationAddressStruct> 
ValidationAddressManager::getValidationAddress(
   const bs::Address& addr)
{
   auto iter = validationAddresses_.find(addr.prefixed());
   if (iter == validationAddresses_.end()) {
      return nullptr;
   }
   //acquire to make sure we see update thread changes
   auto ptrCopy = std::atomic_load_explicit(
      &iter->second, std::memory_order_acquire);
   return ptrCopy;
}

////
const std::shared_ptr<ValidationAddressStruct>
ValidationAddressManager::getValidationAddress(
   const bs::Address& addr) const
{
   auto iter = validationAddresses_.find(addr.prefixed());
   if (iter == validationAddresses_.end()) {
      return nullptr;
   }
   //acquire to make sure we see update thread changes
   auto ptrCopy = std::atomic_load_explicit(
      &iter->second, std::memory_order_acquire);
   return ptrCopy;
}

////
void ValidationAddressManager::addValidationAddress(const bs::Address &addr)
{
   validationAddresses_.insert({ addr
      , std::make_shared<ValidationAddressStruct>() });
}

////
unsigned ValidationAddressManager::goOnline()
{
   /*
   For the sake of simplicity, this assumes the BDV is already online.
   This process is therefor equivalent to registering the validation addresses,
   waiting for the notification and grabbing all txouts for each address.

   Again, for the sake of simplicity, this method blocks untill the setup
   is complete.

   You cannot change the validation address list post setup. You need to 
   destroy this object and create a new one with the updated list.
   */

   //pthread_once behavior
   if (ready_.load(std::memory_order_relaxed)) {
      return UINT32_MAX;
   }
   //use default ACT is none is set
   if (actPtr_ == nullptr) {
      actPtr_ = std::make_shared<ValidationAddressACT>(connPtr_.get());
   }
   //set the act manager ptr to process notifications
   actPtr_->setAddressMgr(this);
   actPtr_->start();

   //register validation addresses
   std::vector<BinaryData> addrVec;

   for (auto& addrPair : validationAddresses_) {
      addrVec.push_back(addrPair.first.prefixed());
   }
   auto&& regID = walletObj_->registerAddresses(addrVec, false);
   waitOnRefresh(regID);

   auto aopCount = update();

   //find & set first outpoints
   for (auto& maPair : validationAddresses_) {
      const auto& maStruct = *maPair.second.get();

      std::shared_ptr<AuthOutpoint> aopPtr;
      BinaryDataRef txHash;
      for (auto& hashPair : maStruct.outpoints_) {
         for (auto& opPair : hashPair.second) {
            if (*opPair.second < aopPtr) {
               aopPtr = opPair.second;
               txHash = hashPair.first.getRef();
            }
         }
      }

      if (aopPtr == nullptr || aopPtr->isZc()) {
         std::stringstream ss;
         ss << "validation address " <<
            maPair.first.display() << " has no valid first outpoint";
         throw std::runtime_error(ss.str());
      }

      maPair.second->firstOutpointHash_ = txHash;
      maPair.second->firstOutpointIndex_ = aopPtr->txOutIndex();
   }

   //set ready & return outpoint count
   ready_.store(true, std::memory_order_relaxed);
   return aopCount;
}

////
unsigned ValidationAddressManager::update()
{
   std::vector<BinaryData> addrVec;
   for (auto& addrPair : validationAddresses_) {
      addrVec.push_back(addrPair.first.prefixed());
   }
   //keep track of txout changes in validation addresses since last seen block
   auto promPtr = std::make_shared<std::promise<unsigned>>();
   auto futPtr = promPtr->get_future();
   auto opLbd = [this, promPtr](ReturnMessage<OutpointBatch> outpointBatch)->void
   {
      unsigned opCount = 0;
      auto batch = outpointBatch.get();
      for (auto& outpointPair : batch.outpoints_) {
         auto& outpointVec = outpointPair.second;
         if (outpointVec.size() == 0) {
            continue;
         }
         opCount += outpointVec.size();

         //create copy of validation address struct
         auto updateValidationAddrStruct = std::make_shared<ValidationAddressStruct>();

         //get existing address struct
         auto maIter = validationAddresses_.find(outpointPair.first);
         if (maIter != validationAddresses_.end()) {
            /*
            Copy the existing struct over to the new one.

            While all notification
            based caller of update() come from the same thread, it is called by
            goOnline() once, from a thread we don't control, therefor the copy
            of the existing struct into the new one is preceded by an acquire
            operation.
            */
            auto maStruct = std::atomic_load_explicit(
               &maIter->second, std::memory_order_acquire);
            *updateValidationAddrStruct = *maStruct;
         }
         else {
            //can't be missing a validation address
            throw std::runtime_error("missing validation address");
         }

         //populate new outpoints
         for (auto& op : outpointVec) {
            auto aop = std::make_shared<AuthOutpoint>(
               op.txHeight_, op.txIndex_, op.txOutIndex_,
               op.value_, op.isSpent_, op.spenderHash_);

            auto hashIter = updateValidationAddrStruct->outpoints_.find(op.txHash_);
            if (hashIter == updateValidationAddrStruct->outpoints_.end()) {
               hashIter = updateValidationAddrStruct->outpoints_.insert(std::make_pair(
                  op.txHash_,
                  std::map<unsigned, std::shared_ptr<AuthOutpoint>>())).first;
            }

            //update existing outpoints if the spent flag is set
            auto fIter = hashIter->second.find(aop->txOutIndex());
            if (fIter != hashIter->second.end()) {
               aop->updateFrom(*fIter->second);

               //remove spender hash entry as the ref will die after this swap
               if (fIter->second->isSpent()) {
                  updateValidationAddrStruct->spenderHashes_.erase(
                     fIter->second->spenderHash().getRef());
               }
               fIter->second = aop;
               if (op.isSpent_) {
                  //set valid spender hash ref
                  updateValidationAddrStruct->spenderHashes_.insert(
                     fIter->second->spenderHash().getRef());
               }
               continue;
            }

            hashIter->second.emplace(std::make_pair(aop->txOutIndex(), aop));
            if (op.isSpent_) {
               //we can just insert the spender hash without worry, as it wont fail to
               //replace an expiring reference
               updateValidationAddrStruct->spenderHashes_.insert(aop->spenderHash().getRef());
            }
         }

         //store with release semantics to make the changes visible to reader threads
         std::atomic_store_explicit(
            &maIter->second, updateValidationAddrStruct, std::memory_order_release);
      }

      //update cutoffs
      topBlock_ = batch.heightCutoff_;
      zcIndex_ = batch.zcIndexCutoff_;

      promPtr->set_value(opCount);
   };

   //grab all txouts
   connPtr_->bdv()->getOutpointsForAddresses(
      addrVec, topBlock_, zcIndex_, opLbd);
   return futPtr.get();
}

////
bool ValidationAddressManager::isValid(const bs::Address& addr) const
{
   auto maStructPtr = getValidationAddress(addr);
   if (maStructPtr == nullptr) {
      return false;
   }
   auto firstOutpoint = maStructPtr->getFirsOutpoint();
   if (firstOutpoint == nullptr) {
      throw std::runtime_error("uninitialized first output");
   }
   if (!firstOutpoint->isValid()) {
      return false;
   }
   if (firstOutpoint->isSpent()) {
      return false;
   }
   return true;
}


bool ValidationAddressManager::getOutpointBatch(const bs::Address &addr
   , const std::function<void(const OutpointBatch &)> &cb) const
{
   if (!connPtr_ || !connPtr_->bdv()) {
      return false;
   }
   //#1: check the user address has no history
   std::vector<BinaryData> addrVec;
   addrVec.push_back(addr.prefixed());

   auto outpointCb = [cb](ReturnMessage<OutpointBatch> outputBatch)->void
   {
      try {
         const auto batch = outputBatch.get();
         if (cb) {
            cb(batch);
         }
      }
      catch (const std::exception &) {
         if (cb) {
            cb({});
         }
      }
   };
   connPtr_->bdv()->getOutpointsForAddresses(addrVec, 0, 0, outpointCb);
   return true;
}

bool ValidationAddressManager::getSpendableTxOutList(const std::function<void(const std::vector<UTXO> &)> &cb) const
{
   if (!connPtr_ || (connPtr_->state() != ArmoryState::Ready)) {
      return false;
   }
   auto spendableCb = [this, cb](
      ReturnMessage<std::vector<UTXO>> utxoVec)->void
   {
      try {
         const auto& utxos = utxoVec.get();
         if (utxos.size() == 0) {
            throw AuthLogicException("no utxos available");
         }
         if (cb) {
            cb(utxos);
         }
      } catch (const std::exception &) {
         if (cb) {
            cb({});
         }
      }
   };
   walletObj_->getSpendableTxOutListForValue(UINT64_MAX, spendableCb);
   return true;
}

UTXO ValidationAddressManager::getVettingUtxo(const bs::Address &validationAddr
   , const std::vector<UTXO> &utxos) const
{
   for (const auto& utxo : utxos) {
      //find the validation address for this utxo
      auto scrAddr = utxo.getRecipientScrAddr();

      //filter by desired validation address if one was provided
      if (!validationAddr.isNull() && (scrAddr != validationAddr.prefixed())) {
         continue;
      }
      auto maStructPtr = getValidationAddress(scrAddr);
      if (maStructPtr == nullptr) {
         continue;
      }
      //is validation address valid?
      if (!isValid(scrAddr)) {
         continue;
      }
      //The first utxo of a validation address isn't eligible to vet
      //user addresses with. Filter that out.

      if (maStructPtr->isFirstOutpoint(utxo.getTxHash(), utxo.getTxOutIndex())) {
         continue;
      }
      //utxo should have enough value to cover vetting amount +
      //vetting tx fee + return tx fee
      if (utxo.getValue() < 3000) { //arbitrary place holder value
         continue;
      }
      return utxo;
   }
   return {};
}

////
BinaryData ValidationAddressManager::fundUserAddress(
   const bs::Address& addr,
   std::shared_ptr<ResolverFeed> feedPtr,
   const bs::Address& validationAddr) const
{  /*
   To vet a user address, send it coins from a validation address.
   */

   //#1: check the user address has no history
   std::vector<BinaryData> addrVec;
   addrVec.push_back(addr.prefixed());

   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   auto outpointCb = [promPtr](const OutpointBatch &batch)->void
   {
      if (batch.outpoints_.size() > 0) {
         promPtr->set_value(false);
      }
      else {
         promPtr->set_value(true);
      }
   };

   getOutpointBatch(addr, outpointCb);
   if (!fut.get()) {
      throw AuthLogicException("can only vet virgin user addresses");
   }

   std::unique_lock<std::mutex> lock(vettingMutex_);

   //#2: grab a utxo from a validation address
   auto promPtr2 = std::make_shared<std::promise<UTXO>>();
   auto fut2 = promPtr2->get_future();
   auto spendableCb = [this, promPtr2, &validationAddr](
      const std::vector<UTXO> &utxos)->void
   {
      if (utxos.empty()) {
         auto ePtr = std::current_exception();
         promPtr2->set_exception(ePtr);
         return;
      }
      const auto utxo = getVettingUtxo(validationAddr, utxos);
      if (utxo.isInitialized()) {
         promPtr2->set_value(utxo);
         return;
      }
      throw AuthLogicException("could not select a utxo to vet with");
   };
   getSpendableTxOutList(spendableCb);

   return fundUserAddress(addr, feedPtr, fut2.get());
}

// fundUserAddress was divided because actual signing will be performed
// in OT which doesn't have access to ArmoryConnection
BinaryData ValidationAddressManager::fundUserAddress(
   const bs::Address& addr,
   std::shared_ptr<ResolverFeed> feedPtr,
   const UTXO &vettingUtxo) const
{
   //#3: create vetting tx
   Signer signer;
   signer.setFeed(feedPtr);

   //spender
   auto spenderPtr = std::make_shared<ScriptSpender>(vettingUtxo);
   signer.addSpender(spenderPtr);

   //vetting output
   signer.addRecipient(addr.getRecipient(AUTH_VALUE_THRESHOLD));

   const auto scrAddr = vettingUtxo.getRecipientScrAddr();
   const auto addrIter = validationAddresses_.find(scrAddr);
   if (addrIter == validationAddresses_.end()) {
      throw AuthLogicException("input addr not found in validation addresses");
   }

   //change: vetting coin value + fee
   const uint64_t changeVal = vettingUtxo.getValue() - AUTH_VALUE_THRESHOLD - 1000;
   if (changeVal > 0) {
      signer.addRecipient(addrIter->first.getRecipient(changeVal));
   }

   //sign & serialize tx
   signer.sign();
   return signer.serialize();
}

BinaryData ValidationAddressManager::vetUserAddress(
   const bs::Address& addr,
   std::shared_ptr<ResolverFeed> feedPtr,
   const bs::Address& validationAddr) const
{
   const auto signedTx = fundUserAddress(addr, feedPtr, validationAddr);

   //broadcast the zc
   connPtr_->pushZC(signedTx);

   Tx txObj(signedTx);
   return txObj.getThisHash();
}

////
BinaryData ValidationAddressManager::revokeValidationAddress(
   const bs::Address& addr, std::shared_ptr<ResolverFeed> feedPtr) const
{  /*
   To revoke a validation address, spend its first UTXO.
   */

   //find the MA
   auto maStructPtr = getValidationAddress(addr);
   if (maStructPtr == nullptr) {
      throw AuthLogicException("unknown validation address!");
   }
   std::unique_lock<std::mutex> lock(vettingMutex_);

   //grab UTXOs
   auto promPtr = std::make_shared<std::promise<UTXO>>();
   auto fut = promPtr->get_future();
   auto spendableCb = [this, promPtr, maStructPtr]
      (ReturnMessage<std::vector<UTXO>> utxoVec)->void
   {
      try {
         const auto& utxos = utxoVec.get();
         if (utxos.size() == 0) {
            throw AuthLogicException("no utxo to revoke");
         }

         for (const auto& utxo : utxos) {
            if (!maStructPtr->isFirstOutpoint(
               utxo.getTxHash(), utxo.getTxOutIndex())) {
               continue;
            }
            promPtr->set_value(utxo);
            return;
         }

         throw AuthLogicException("could not select first outpoint");
      }
      catch (const std::exception &) {
         promPtr->set_exception(std::current_exception());
      }
   };

   walletObj_->getSpendableTxOutListForValue(UINT64_MAX, spendableCb);
   auto&& firstUtxo = fut.get();

   //spend it
   Signer signer;
   signer.setFeed(feedPtr);

   //spender
   auto spenderPtr = std::make_shared<ScriptSpender>(firstUtxo);
   signer.addSpender(spenderPtr);

   //revocation output, no need for change
   signer.addRecipient(addr.getRecipient((uint64_t)(firstUtxo.getValue() - 1000)));

   //sign & serialize tx
   signer.sign();
   auto signedTx = signer.serialize();
   if (signedTx.isNull()) {
      throw AuthLogicException("failed to sign");
   }
   //broadcast the zc
   connPtr_->pushZC(signedTx);

   Tx txObj(signedTx);
   return txObj.getThisHash();
}

BinaryData ValidationAddressManager::revokeUserAddress(
   const bs::Address& addr, std::shared_ptr<ResolverFeed> feedPtr)
{
   /*
   To revoke a user address from a validation address, send it coins from
   its own validation address.
   */

   //1: find validation address vetting this address
   auto paths = AuthAddressLogic::getValidPaths(*this, addr);
   if (paths.size() != 1) {
      throw AuthLogicException("invalid user auth address");
   }
   auto& validationAddr = findValidationAddressForTxHash(paths[0].txHash_);
   if (!validationAddr.isValid()) {
      throw AuthLogicException("invalidated validation address");
   }
   auto validationAddrPtr = getValidationAddress(validationAddr);

   std::unique_lock<std::mutex> lock(vettingMutex_);

   //2: get utxo from the validation address
   auto promPtr = std::make_shared<std::promise<UTXO>>();
   auto fut = promPtr->get_future();
   auto utxoLbd = [this, promPtr, validationAddrPtr](
      ReturnMessage<std::vector<UTXO>> utxoVec)->void
   {
      try {
         const auto& utxos = utxoVec.get();
         if (utxos.size() == 0) {
            throw AuthLogicException("no utxos to revoke with");
         }
         for (const auto& utxo : utxos) {
            //cannot use the validation address first utxo
            if (validationAddrPtr->isFirstOutpoint(
               utxo.getTxHash(), utxo.getTxOutIndex())) {
               continue;
            }
            if (utxo.getValue() < AUTH_VALUE_THRESHOLD + 1000ULL) {
               continue;
            }
            promPtr->set_value(utxo);
            return;
         }

         throw AuthLogicException("could not select utxo to revoke with");
      }
      catch (const std::exception &) {
         promPtr->set_exception(std::current_exception());
      }
   };

   connPtr_->bdv()->getUTXOsForAddress(
      validationAddr.prefixed(), false, utxoLbd);
   auto&& utxo = fut.get();

   //3: spend to the user address
   Signer signer;
   signer.setFeed(feedPtr);

   //spender
   auto spenderPtr = std::make_shared<ScriptSpender>(utxo);
   signer.addSpender(spenderPtr);

   //revocation output
   signer.addRecipient(addr.getRecipient(AUTH_VALUE_THRESHOLD));

   //change
   signer.addRecipient(validationAddr.getRecipient(
      utxo.getValue() - AUTH_VALUE_THRESHOLD - 1000));

   //sign & serialize tx
   signer.sign();
   auto signedTx = signer.serialize();

   //broadcast the zc
   connPtr_->pushZC(signedTx);

   Tx txObj(signedTx);
   return txObj.getThisHash();
}

////
bool ValidationAddressManager::hasSpendableOutputs(const bs::Address& addr) const
{
   auto& maStruct = getValidationAddress(addr);

   for (auto& outpointSet : maStruct->outpoints_) {
      for (auto& outpoint : outpointSet.second) {
         //ZC outputs are not eligible to vet with
         if (!outpoint.second->isSpent() && !outpoint.second->isZc()) {
            //nor is the first outpoint
            if (maStruct->isFirstOutpoint(
               outpointSet.first, outpoint.second->txOutIndex())) {
               continue;
            }
            return true;
         }
      }
   }
   return false;
}

////
bool ValidationAddressManager::hasZCOutputs(const bs::Address& addr) const
{
   auto iter = validationAddresses_.find(addr.prefixed());
   if (iter == validationAddresses_.end()) {
      throw std::runtime_error("unknown validation address");
   }
   for (auto& outpointSet : iter->second->outpoints_) {
      for (auto& outpoint : outpointSet.second) {
         if (outpoint.second->isZc()) {
            return true;
         }
      }
   }
   return false;
}

////
const bs::Address& ValidationAddressManager::findValidationAddressForUTXO(
   const UTXO& utxo) const
{
   return findValidationAddressForTxHash(utxo.getTxHash());
}

////
const bs::Address& ValidationAddressManager::findValidationAddressForTxHash(
   const BinaryData& txHash) const
{
   for (auto& maPair : validationAddresses_) {
      auto iter = maPair.second->spenderHashes_.find(txHash);
      if (iter == maPair.second->spenderHashes_.end()) {
         continue;
      }
      return maPair.first;
   }
   throw std::runtime_error("no validation address spends to that hash");
}

///////////////////////////////////////////////////////////////////////////////
std::vector<OutpointData> AuthAddressLogic::getValidPaths(
   const ValidationAddressManager& vam, const bs::Address& addr)
{
   std::vector<OutpointData> validPaths;

   //get txout history for address
   auto promPtr = std::make_shared<std::promise<
      std::map<BinaryData, std::vector<OutpointData>>>>();
   auto futPtr = promPtr->get_future();
   auto opLbd = [promPtr](ReturnMessage<OutpointBatch> outpointBatch)->void
   {
      auto&& outpoints = outpointBatch.get();
      promPtr->set_value(outpoints.outpoints_);
   };

   std::vector<BinaryData> addrVec;
   addrVec.push_back(addr.prefixed());

   vam.connPtr()->bdv()->getOutpointsForAddresses(
      addrVec, 0, 0, opLbd);

   //sanity check on the address history
   auto&& opMap = futPtr.get();
   if (opMap.size() != 1) {
      throw AuthLogicException(
         "unexpected result from getOutpointsForAddresses");
   }

   auto& opVec = opMap.begin()->second;

   //check all spent outputs vs ValidationAddressManager
   for (auto& outpoint : opVec) {
      try {
         /*
         Does this txHash spend from a validation address output? It will
         throw if not.
         */
         auto& validationAddr = 
            vam.findValidationAddressForTxHash(outpoint.txHash_);

         /*
         If relevant validation address is invalid, this address is invalid,
         regardless of any other path states.
         */
         if (!vam.isValid(validationAddr)) {
            throw AuthLogicException(
               "Address is vetted by invalid validation address");
         }

         /*
         Is the validation output spent? Spending it revokes the
         address.
         */
         if (outpoint.isSpent_) {
            throw AuthLogicException(
               "Address has been revoked");
         }

         validPaths.push_back(outpoint);
      }
      catch (const std::exception &) {
         continue;
      }
   }

   return validPaths;
}

////
bool AuthAddressLogic::isValid(
   const ValidationAddressManager& vam, const bs::Address& addr)
{
   /***
   Validity is unique. This means there should be only one output chain
   defining validity. Any concurent path, whether partial or full,
   invalidates the user address.
   ***/

   auto currentTop = vam.connPtr()->topBlock();
   if (currentTop == UINT32_MAX) {
      throw std::runtime_error("invalid top height");
   }

   try {
      auto&& validPaths = getValidPaths(vam, addr);

      //is there only 1 valid path?
      if (validPaths.size() != 1) {
         return false;
      }
      auto& outpoint = validPaths[0];

      //does this path have enough confirmations?
      auto opHeight = outpoint.txHeight_;
      if (currentTop >= opHeight &&
         (1 + currentTop - opHeight) >= VALIDATION_CONF_COUNT) {
         return true;
      }
   }
   catch (AuthLogicException&) { }

   return false;
}

////
BinaryData AuthAddressLogic::revoke(const ValidationAddressManager& vam,
   const bs::Address& addr, std::shared_ptr<ResolverFeed> feedPtr)
{
   //get valid paths for address

   auto&& validPaths = getValidPaths(vam, addr);

   //is there only 1 valid path?
   if (validPaths.size() != 1) {
      throw AuthLogicException("address has no valid paths");
   }
   auto& outpoint = validPaths[0];

   /*
   We do not check auth output maturation when revoking.
   A yet to be confirmed valid path can be revoked.
   */

   //grab UTXOs for address
   auto promPtr = std::make_shared<std::promise<UTXO>>();
   auto fut = promPtr->get_future();
   auto utxosLbd = [&outpoint, promPtr]
   (ReturnMessage<std::vector<UTXO>> utxoVec)->void
   {
      try {
         auto&& utxos = utxoVec.get();
         for (auto& utxo : utxos) {
            if (utxo.getTxHash() == outpoint.txHash_ &&
               utxo.getTxOutIndex() == outpoint.txOutIndex_) {
               promPtr->set_value(utxo);
               return;
            }
         }

         /*
         Throw if we can't find the outpoint to revoke within the
         address' utxos, as this indicates our auth state is
         corrupt.
         */
         throw std::runtime_error("could not find utxo to revoke");
      }
      catch (const std::exception_ptr &e) {
         promPtr->set_exception(e);
      }
   };

   vam.connPtr()->bdv()->getUTXOsForAddress(
      addr, true, utxosLbd);
   auto&& revokeUtxo = fut.get();

   //construct revocation utxo
   Signer signer;

   /*
   We should have passed the feed that can resolve private keys
   for this auth address, i.e. the auth wallet's HD leaf resolver.
   Obviously, the leaf should also be locked for decryption.
   */
   signer.setFeed(feedPtr);

   /*
   We're only spending from the revoke utxo in this scenario. A more
   realistic case is where another utxo is provided to cover for an
   ample fee, as you want revocations to take places quickly. This
   edge case needs to be addressed.
   */
   signer.addSpender(std::make_shared<ScriptSpender>(revokeUtxo));

   //we're sending the coins back to the relevant validation address
   auto& validationAddr = vam.findValidationAddressForUTXO(revokeUtxo);

   signer.addRecipient(validationAddr.getRecipient(revokeUtxo.getValue()));

   //sign and broadcast, return the txHash
   signer.sign();
   auto signedTx = signer.serialize();

   Tx txObj(signedTx);
   vam.connPtr()->pushZC(signedTx);

   return txObj.getThisHash();
}

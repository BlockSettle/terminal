#include "ColoredCoinLogic.h"

/***

#1: Add CC origin address

#2: Find all spends, check with CC value structure, save all valid cc outputs

#3: Same with all valid cc outputs until there are only unspent outputs left.
    This is the valid CC utxo set for this height.

#4: Merklerize. Pass the signed Merkle to light clients, generate proofs for 
    them to verify CC utxo validity at settlement time.

--
Parsing CC tx:

1) Input order doesn't matter. If an input is a valid CC, its value is added 
   to the total expended CC balance.

2) You can have any amount of other inputs that are not CC added to the 
   transaction. Those values will not be counted towards the total CC balance.

3) Valid CC outputs have to have a value that is a multiple of the 
   instruments coinsPerShare.

4) Output ordering is strict: 
   Only the first outputs that have proper the CC value (rule 3) and which 
   cumulated value is less or equal to the total CC balance (rule 2) are 
   considered value new CC outputs. 
   
   New CC output creation ends when either rule is broken. All outputs 
   past the one that breaks the rule (included) are ignored and will not 
   count towards new CC output creation.

   It is therefor possible to destroy CC value.

5) Exception: All UTXOs on origin addresses are valid to create CC from, 
   regardless of their value or ordering within the tx. 
   
   However, these are only relevant for issuing CC to users. They do not 
   count towards the total outstanding CC balance. Therefor, an instrument's
   origin addresses never actually hold any valid CC.

6) A CC can be invalidated if the address holding the output(s) receives an 
   output from the instrument's revocation address. In this case, all 
   outstanding valid CC outputs on this address are invalidated.

   This operation is not retroactive, it only applies to the current and 
   further CC UTXO sets.

7) There is no need to keep track of spent CC outputs. Only the utxo set 
   matters once parenthood has been established. A snapshot for an instrument
   is the list of UTXOs at any given time. Any system can bootstrap from 
   either the CC origin address or a valid snapshot.

8) Snapshots should have some sort of signature to assert their validity.

9) Establishing CC output is expensive. Therefor, clients operate only on a 
   the merkle root of the current CC UTXO set for the relevant instrument. 
   
   Clients who run against a remote service will receive signed merkle root 
   updates and can request proofs to validate CC outputs.

   Clients who run a local service can run their own CC parsing serivce and 
   grab merkle roots and proofs from their own stack instead.

   Clients always operate on merkle roots and proof. The only variation is 
   whether they run the parser locally or trust a remote one.



-- conf count and merkleisation

-- add rules to check for mix and match of multiple CC
   this shouldnt be tolerated
***/

////////////////////////////////////////////////////////////////////////////////
void CcOutpoint::setTxHash(const BinaryData& hash)
{
   if (txHash_ != nullptr) {
      throw ColoredCoinException("cc outpoint already has hash ptr");
   }
   txHash_ = std::make_shared<BinaryData>(hash);
}

////
void CcOutpoint::setTxHash(const std::shared_ptr<BinaryData>& hash)
{
   if (txHash_ != nullptr) {
      throw ColoredCoinException("cc outpoint already has hash ptr");
   }
   txHash_ = hash;
}

////
void CcOutpoint::setScrAddr(const std::shared_ptr<BinaryData>& scrAddr)
{
   if (scrAddr_ != nullptr) {
      throw ColoredCoinException("cc outpoint already has hash ptr");
   }
   scrAddr_ = scrAddr;
}


////////////////////////////////////////////////////////////////////////////////
void ColoredCoinTracker::addOriginAddress(const bs::Address& addr)
{
   originAddresses_.insert(addr.prefixed());
}

////
void ColoredCoinTracker::addRevocationAddress(const bs::Address& addr)
{
   revocationAddresses_.insert(addr.prefixed());
}

////
std::shared_ptr<ColoredCoinSnapshot> ColoredCoinTracker::snapshot() const
{
   return std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
}

std::shared_ptr<ColoredCoinZCSnapshot> ColoredCoinTracker::zcSnapshot() const
{
   return std::atomic_load_explicit(&zcSnapshot_, std::memory_order_acquire);
}

////
const std::shared_ptr<BinaryData> ColoredCoinTracker::getScrAddrPtr(
   const std::map<BinaryData, OpPtrSet>& addrMap
   , const BinaryData& scrAddr) const
{
   auto scrAddrIter = addrMap.find(scrAddr.getRef());
   if (scrAddrIter == addrMap.end()) {
      return nullptr;
   }
   if (scrAddrIter->second.size() == 0) {
      return nullptr;
   }
   auto& obj = *scrAddrIter->second.begin();
   if (obj == nullptr) {
      return nullptr;
   }
   return obj->getScrAddr();
}

////
uint64_t ColoredCoinTracker::getCcOutputValue(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr
   , const std::shared_ptr<ColoredCoinZCSnapshot>& zcPtr
   , const BinaryData& hash, unsigned txOutIndex, unsigned height) const
{
   /*
   Error returns:
      0 if the output is not a valid CC.
      UINT64_MAX if it was revoked.
   */

   auto getOpPtr = [&ssPtr, &hash, &txOutIndex](void)->
      std::shared_ptr<CcOutpoint>
   {
      if (ssPtr == nullptr) {
         return nullptr;
      }
      auto hashIter = ssPtr->utxoSet_.find(hash);
      if (hashIter == ssPtr->utxoSet_.end()) {
         return nullptr;
      }
      auto idIter = hashIter->second.find(txOutIndex);
      if (idIter == hashIter->second.end()) {
         return nullptr;
      }
      return idIter->second;
   };

   //try to grab the cc output
   auto ccPtr = getOpPtr();

   if (ccPtr != nullptr) {
      //check this cc addr isnt revoked
      if (ssPtr != nullptr) {
         auto revokedIter = ssPtr->revokedAddresses_.find(*ccPtr->getScrAddr());
         if (revokedIter != ssPtr->revokedAddresses_.end() &&
            height > revokedIter->second) {
            return UINT64_MAX;
         }
      }

      //check the cc isn't spent by a zc
      if (zcPtr == nullptr) {
         return ccPtr->value();
      }
      auto spentIter = zcPtr->spentOutputs_.find(hash);
      if (spentIter == zcPtr->spentOutputs_.end()) {
         return ccPtr->value();
      }
      auto indexIter = spentIter->second.find(txOutIndex);
      if (indexIter == spentIter->second.end()) {
         return ccPtr->value();
      }
      return 0;
   }

   //no output was found in utxo set, check zc snapshot
   if (zcPtr == nullptr) {
      return 0;
   }
   auto spentIter = zcPtr->utxoSet_.find(hash);
   if (spentIter == zcPtr->utxoSet_.end()) {
      return 0;
   }
   auto indexIter = spentIter->second.find(txOutIndex);
   if (indexIter == spentIter->second.end()) {
      return 0;
   }
   return indexIter->second->value();
}

////
uint64_t ColoredCoinTracker::getCcOutputValue(
   const BinaryData& hash, unsigned int txOutIndex
   , unsigned int height) const
{
   auto ssPtr = snapshot();
   auto zcPtr = zcSnapshot();
   return getCcOutputValue(ssPtr, zcPtr, hash, txOutIndex, height);
}
   
////
ParsedCcTx ColoredCoinTracker::processTx(
   const std::shared_ptr<ColoredCoinSnapshot> &ssPtr
   , const std::shared_ptr<ColoredCoinZCSnapshot>& zcPtr
   , const Tx& tx) const
{
   ParsedCcTx result;

   //how many inputs are CC
   uint64_t ccValue = 0;
   for (unsigned i = 0; i < tx.getNumTxIn(); i++) {
      auto&& input = tx.getTxInCopy(i); //TODO: work on refs instead of copies
      auto&& outpoint = input.getOutPoint();

      auto val = getCcOutputValue(
         ssPtr, zcPtr,
         outpoint.getTxHash(), outpoint.getTxOutIndex(),
         tx.getTxHeight());

      if (val == UINT64_MAX || val == 0) {
         continue;
      }
      //keep track of CC outpoints
      result.outpoints_.push_back(
         std::make_pair(outpoint.getTxHash(), outpoint.getTxOutIndex()));

      //tally CC value
      ccValue += val;
   }

   if (ccValue == 0) {
      //not a CC tx
      return result;
   }

   //this tx consumes CC outputs, let's check the new outputs
   uint64_t outputValue = 0;
   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      auto&& output = tx.getTxOutCopy(i); //TODO: work on refs instead of copies
      auto&& val = output.getValue();

      //is the value a multiple of the CC coins per share?
      if (val % coinsPerShare_ != 0) {
         break;
      }
      //is the value non zero?
      if (val == 0) {
         break;
      }
      //if cumulated output value exceeds total ccValue, break
      if (outputValue + val > ccValue) {
         break;
      }
      //tally output value
      outputValue += val;

      //add to the result's list of CC outputs
      result.outputs_.push_back(std::make_pair(val, output.getScrAddressStr()));
   }

   //return as is if no new CC output was detected
   if (result.outputs_.size() == 0) {
      return result;
   }
   //total output CC value should be inferior or equal to redeemed CC value
   if (outputValue > ccValue) {
      return result;
   }
   //we got this far, this is a good CC tx, set the txhash in the result
   //struct to flag it as valid and return
   result.txHash_ = tx.getThisHash();
   return result;
}

////
std::vector<Tx> ColoredCoinTracker::grabTxBatch(
   const std::set<BinaryData>& hashes)
{
   if (hashes.empty()) {
      return {};
   }
   auto txProm = std::make_shared<std::promise<std::vector<Tx>>>();
   auto txFut = txProm->get_future();
   auto txLbd = [txProm, hashes](const std::vector<Tx> &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         txProm->set_exception(exPtr);
      }
      else {
         auto sortedBatch = batch;
         std::sort(sortedBatch.begin(), sortedBatch.end(), TxComparator());
         txProm->set_value(sortedBatch);
      }
   };
   if (!connPtr_->getTXsByHash(hashes, txLbd)) {
      throw ColoredCoinException("invalid DB state/connection");
   }
   return txFut.get();
}

////
std::set<BinaryData> ColoredCoinTracker::processTxBatch(
   std::shared_ptr<ColoredCoinSnapshot>& ssPtr,
   const std::set<BinaryData>& hashes)
{
   //grab listed tx
   auto&& txBatch = grabTxBatch(hashes);

   std::shared_ptr<ColoredCoinZCSnapshot> zcPtr = nullptr;
   std::map<BinaryData, std::set<unsigned>> spentnessToTrack;

   //process them
   for (auto& tx : txBatch) {
      //parse the tx
      auto&& parsedTx = processTx(ssPtr, zcPtr, tx);

      //purge utxo set of all spent CC outputs
      for (auto& input : parsedTx.outpoints_) {
         auto hashIter = ssPtr->utxoSet_.find(input.first);
         if (hashIter == ssPtr->utxoSet_.end()) {
            throw ColoredCoinException("missing outpoint hash");
         }
         auto idIter = hashIter->second.find(input.second);
         if (idIter == hashIter->second.end()) {
            throw ColoredCoinException("missing outpoint index");
         }
         //remove from scrAddr to utxo map
         eraseScrAddrOp(ssPtr, idIter->second);

         //remove from utxo set
         hashIter->second.erase(idIter);
         if (hashIter->second.size() == 0) {
            ssPtr->utxoSet_.erase(hashIter);
         }
      }

      if (parsedTx.isInitialized()) {
         //This tx creates valid CC utxos, add them to the map and 
         //track the spender hashes if any

         auto spentnessIter = spentnessToTrack.insert(
            std::make_pair(parsedTx.txHash_, std::set<unsigned>())).first;

         for (unsigned i = 0; i < parsedTx.outputs_.size(); i++) {
            //add the utxo
            auto& output = parsedTx.outputs_[i];
            addUtxo(ssPtr, parsedTx.txHash_, i, output.first, output.second);

            //add the index to the spentness fetch packet
            spentnessIter->second.insert(i);
         }
      }
   }

   //check new utxo list
   auto spentnessProm = std::make_shared<
      std::promise<std::map<BinaryData, std::map<unsigned, BinaryData>>>>();
   auto spentnessFut = spentnessProm->get_future();
   auto spentnessLbd = [spentnessProm](
      const std::map<BinaryData, std::map<unsigned, BinaryData>> &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         spentnessProm->set_exception(exPtr);
      }
      else {
         spentnessProm->set_value(batch);
      }
   };
   if (!connPtr_->getSpentnessForOutputs(spentnessToTrack, spentnessLbd)) {
      throw ColoredCoinException("invalid DB state/connection");
   }
   auto&& spentnessBatch = spentnessFut.get();

   //aggregate spender hashes
   std::set<BinaryData> spenderHashes;
   for (auto& spentness : spentnessBatch) {
      auto& spentnessMap = spentness.second;
      for (auto& hashPair : spentnessMap) {
         if (hashPair.second.getSize() == 32) {
            spenderHashes.insert(hashPair.second);
         }
      }
   }

   return spenderHashes;
}

////
void ColoredCoinTracker::processZcBatch(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr,
   const std::shared_ptr<ColoredCoinZCSnapshot>& zcPtr,
   const std::set<BinaryData>& hashes)
{
   if (hashes.size() == 0)
      return;

   //grab listed tx
   auto&& txBatch = grabTxBatch(hashes);

   //process the zc transactions
   for (auto& tx : txBatch) {
      //parse the tx
      auto&& parsedTx = processTx(ssPtr, zcPtr, tx);
      
      //purge utxo set of all spent CC outputs
      for (auto& input : parsedTx.outpoints_) {
         auto hashIter = ssPtr->utxoSet_.find(input.first);
         if (hashIter != ssPtr->utxoSet_.end()) {
            auto idIter = hashIter->second.find(input.second);
            if (idIter != hashIter->second.end()) {
               //spent confirmed output, mark it in zc snapshot
               auto spentIter = zcPtr->spentOutputs_.find(input.first);
               if (spentIter == zcPtr->spentOutputs_.end()) {
                  spentIter = zcPtr->spentOutputs_.insert(
                     std::make_pair(input.first, std::set<unsigned>())).first;
               }

               spentIter->second.insert(input.second);
               continue;
            }
         }

         //not a confirmed output, remove from zc utxo set instead
         auto zcHashIter = zcPtr->utxoSet_.find(input.first);
         if (zcHashIter == zcPtr->utxoSet_.end()) {
            continue;
         }
         zcHashIter->second.erase(input.second);
         if (zcHashIter->second.size() == 0) {
            zcPtr->utxoSet_.erase(zcHashIter);
         }
      }

      if (parsedTx.isInitialized()) {
         //This tx creates valid CC utxos, add them to the map and 
         //track the spender hashes if any

         for (unsigned i = 0; i < parsedTx.outputs_.size(); i++) {
            //add the utxo
            auto& output = parsedTx.outputs_[i];
            addZcUtxo(ssPtr, zcPtr, parsedTx.txHash_, i, output.first, output.second);
         }
      }
   }
}

////
void ColoredCoinTracker::processRevocationBatch(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr,
   const std::set<BinaryData>& hashes)
{
   if (hashes.size() == 0) {
      return;
   }
   //grab listed tx
   auto txProm = std::make_shared<std::promise<std::vector<Tx>>>();
   auto txFut = txProm->get_future();
   auto txLbd = [txProm](const std::vector<Tx> &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         txProm->set_exception(exPtr);
      }
      else {
         txProm->set_value(batch);
      }
   };
   if (!connPtr_->getTXsByHash(hashes, txLbd)) {
      throw ColoredCoinException("invalid DB state/connection");
   }
   const auto &txBatch = txFut.get();

   //mark all output scrAddr as revoked
   for (auto& tx : txBatch) {
      for (unsigned i = 0; i < tx.getNumTxOut(); i++) {
         auto&& txOut = tx.getTxOutCopy(i); //TODO: work with ref instead of copy
         auto&& scrAddr = txOut.getScrAddressRef();

         auto iter = revocationAddresses_.find(scrAddr);
         if (iter != revocationAddresses_.end()) {
            continue;
         }
         ssPtr->revokedAddresses_.insert(std::make_pair(scrAddr, tx.getTxHeight()));
      }
   }
}

std::set<BinaryData> ColoredCoinTracker::collectOriginAddresses() const
{
   std::set<BinaryData> addrSet;
   for (const auto &origAddr : originAddresses_) {
      addrSet.insert(origAddr);
   }
   return addrSet;
}

std::set<BinaryData> ColoredCoinTracker::collectRevokeAddresses() const
{
   std::set<BinaryData> addrSet;
   for (const auto &revokeAddr : revocationAddresses_) {
      addrSet.insert(revokeAddr);
   }
   return addrSet;
}

////
std::set<BinaryData> ColoredCoinTracker::update()
{
   //create new snapshot
   auto ssPtr = std::make_shared<ColoredCoinSnapshot>();

   {
      /*
      We need to copy the current snapshot into the new one to then
      update it. We do not need the current snapshot past that point
      and the new one is meant to replace it once it's ready. Therefor
      we will perform the copy in a dedicated scope.
      */
      auto currentSs = snapshot();
      if (currentSs != nullptr) {
         *ssPtr = *currentSs;
      }
   }

   //track changeset for relevant addresses
   auto &&addrSet = collectOriginAddresses();

   const auto &revokeAddrs = collectRevokeAddresses();
   addrSet.insert(revokeAddrs.cbegin(), revokeAddrs.cend());

   //current set of live user addresses
   for (auto& addrRef : ssPtr->scrAddrCcSet_) {
      addrSet.insert(addrRef.first);
   }

   auto promPtr = std::make_shared<std::promise<OutpointBatch>>();
   auto fut = promPtr->get_future();
   auto lbd = [promPtr](const OutpointBatch &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         promPtr->set_exception(exPtr);
      }
      else {
         promPtr->set_value(batch);
      }
   };

   /*
   We don't want any zc data for this call so pass UINT32_MAX
   as the zc cutoff.
   */
   if (!connPtr_->getOutpointsForAddresses(addrSet, lbd, startHeight_, UINT32_MAX)) {
      throw ColoredCoinException("invalid DB state/connection");
   }

   auto&& outpointData = fut.get();
   std::set<BinaryData> txsToCheck;
   std::set<BinaryData> revokesToCheck;

   /*
   All outputs that hit origin addresses become valid CC UTXOs, even 
   though they do not count towards actual CC balance. The tracker
   operates on UTXOs, so it needs to know of all origin address UTXOs, 
   otherwise it will fail to tag user funding operations.
   */

   for (auto& scrAddr : originAddresses_) {
      auto iter = outpointData.outpoints_.find(scrAddr);
      if (iter == outpointData.outpoints_.end()) {
         continue;
      }
      for (auto& op : iter->second) {
         addUtxo(ssPtr, op.txHash_, op.txOutIndex_, op.value_, scrAddr);
      }
   }

   /*
   Users cannot create new CC, only the origin address holder can. 
   Therefor all CC operations performed by users have to consume
   existing CC UTXOs.

   Also, any address that receives an outpoint originating from our
   set of revocation addresses is revoked from that point on. 
   Revocation is not retroactive.
   */

   for (auto& addrPair : outpointData.outpoints_) {
      for (auto& op : addrPair.second) {
         if (op.isSpent_) {
            /*
            An output from our list of tracked addresses has been
            spent. Does it affect this instrument?
            */

            //sanity check
            if (op.spenderHash_.getSize() != 32) {
               throw ColoredCoinException("missing spender hash");
            }
            //was the output from a revocation address?
            auto revokeIter = revocationAddresses_.find(addrPair.first);
            if (revokeIter != revocationAddresses_.end()) {
               //check the spender for addresses to revoke
               revokesToCheck.insert(op.spenderHash_);
               continue;
            }
            
            //or was it a valid CC?
            auto hashIter = ssPtr->utxoSet_.find(op.txHash_);
            if (hashIter == ssPtr->utxoSet_.end()) {
               continue;
            }
            auto idIter = hashIter->second.find(op.txOutIndex_);
            if (idIter == hashIter->second.end()) {
               continue;
            }
            //mark the spender for CC settlement
            txsToCheck.insert(op.spenderHash_);
         }
      }
   }

   //process revokes
   processRevocationBatch(ssPtr, revokesToCheck);

   std::set<BinaryData> combinedTxHashes = txsToCheck;
   //process settlements
   while (true) {
      if (txsToCheck.empty()) {
         break;
      }
      txsToCheck = std::move(processTxBatch(ssPtr, txsToCheck));
      combinedTxHashes.insert(txsToCheck.cbegin(), txsToCheck.cend());
   }

   /*
   A kind of workaround below. This helps to resolve "loops" when current batch
   processing attempts to retrieve valid inputs that will be added in the next
   batch[es]. Example TX for this scenario (testnet):
      fa6fc18e49ecdbd68ebd38b6146a3060157d3743e6908e8a6e7457b5bf35162e
   It denies the second output because 6 of 9 CC inputs are not added to utxoSet_
   at the moment of this TX processing in "traditional" loop above.
   This solution just collects the TX hashes from the previous loop, sorts them by height
   and processes further using the same loop until txsToCheck becomes empty.
   */
   for (auto& scrAddr : originAddresses_) {  // This is just to re-seed genesis TXs
      auto iter = outpointData.outpoints_.find(scrAddr);
      if (iter == outpointData.outpoints_.end()) {
         continue;
      }
      for (auto& op : iter->second) {
         addUtxo(ssPtr, op.txHash_, op.txOutIndex_, op.value_, scrAddr);
      }
   }
   txsToCheck = processTxBatch(ssPtr, combinedTxHashes);
   while (true) {
      if (txsToCheck.empty()) {
         break;
      }
      txsToCheck = std::move(processTxBatch(ssPtr, txsToCheck));
   }

   //update cutoff
   startHeight_ = outpointData.heightCutoff_ + 1;

   //track new addresses
   std::set<BinaryData> toReg;
   for (auto& addr : ssPtr->scrAddrCcSet_) {
      if (addrSet.find(addr.first) == addrSet.end()) {
         toReg.insert(addr.first);
      }
   }

   //swap new snapshot in
   std::atomic_store_explicit(&snapshot_, ssPtr, std::memory_order_release);

   //purge zc container
   purgeZc();

   //register new addresses
   return toReg;
}

////
std::set<BinaryData> ColoredCoinTracker::zcUpdate()
{
   //create new snapshot
   auto ssPtr = std::make_shared<ColoredCoinZCSnapshot>();
   auto currentSs = snapshot();

   {
      auto currentZcSs = zcSnapshot();
      if (currentZcSs != nullptr) {
         *ssPtr = *currentZcSs;
      }
   }

   //track changeset for relevant addresses
   auto &&addrSet = collectOriginAddresses();

   //current set of live user addresses
   if (currentSs != nullptr) {
      for (auto& addrRef : currentSs->scrAddrCcSet_)
         addrSet.insert(addrRef.first);
   }

   for (auto& addrRef : ssPtr->scrAddrCcSet_) {
      addrSet.insert(addrRef.first);
   }
   //note: we dont deal with unconfirmed revocations
   auto promPtr = std::make_shared<std::promise<OutpointBatch>>();
   auto fut = promPtr->get_future();
   auto lbd = [promPtr](const OutpointBatch &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         promPtr->set_exception(exPtr);
      }
      else {
         promPtr->set_value(batch);
      }
   };

   /*
   We don't want any confirmed data for this call so pass UINT32_MAX
   as the height cutoff.
   */
   if (!connPtr_->getOutpointsForAddresses(addrSet, lbd, UINT32_MAX, zcCutOff_)) {
      throw ColoredCoinException("invalid DB state/connection");
   }

   auto&& outpointData = fut.get();
   std::set<BinaryData> txsToCheck;

   //parse new outputs for origin addresses
   for (auto& scrAddr : originAddresses_) {
      auto iter = outpointData.outpoints_.find(scrAddr);
      if (iter == outpointData.outpoints_.end()) {
         continue;
      }
      for (auto& op : iter->second) {
         addZcUtxo(currentSs, ssPtr, op.txHash_, op.txOutIndex_, op.value_, scrAddr);
      }
   }

   //parse new spenders
   for (auto& addrPair : outpointData.outpoints_) {
      for (auto& op : addrPair.second) {
         if (op.isSpent_) {
            /*
            An output from our list of tracked addresses has been
            spent. Does it affect this instrument?
            */

            //sanity check
            if (op.spenderHash_.getSize() != 32) {
               throw ColoredCoinException("missing spender hash");
            }
            //was it a valid CC?
            auto ccVal = getCcOutputValue(
               currentSs, ssPtr, op.txHash_, op.txOutIndex_, op.txHeight_);
            if (ccVal == 0 || ccVal == UINT64_MAX) {
               continue;
            }
            //mark the spender for CC settlement check
            txsToCheck.insert(op.spenderHash_);
         }
      }
   }

   //process unconfirmed settlements
   processZcBatch(currentSs, ssPtr, txsToCheck);

   //update zc cutoff
   zcCutOff_ = outpointData.zcIndexCutoff_;

   //track new addresses
   std::set<BinaryData> toReg;
   for (auto& addr : ssPtr->scrAddrCcSet_) {
      if (addrSet.find(addr.first) == addrSet.end()) {
         toReg.insert(addr.first);
      }
   }

   //swap the new snapshot in
   std::atomic_store_explicit(&zcSnapshot_, ssPtr, std::memory_order_release);

   //register new addresses
   return toReg;
}

////
void ColoredCoinTracker::purgeZc()
{
   auto currentSs = snapshot();

   //grab height for all our active zc
   std::set<BinaryData> txHashes;

   {
      //only need the zc snapshot to gather zc tx hashes
      auto currentZcSs = zcSnapshot();
      if (currentZcSs == nullptr)
         return;

      for (auto& hashPair : currentZcSs->utxoSet_)
         txHashes.insert(hashPair.first);
   }

   auto promPtr = std::make_shared<std::promise<std::vector<Tx>>>();
   auto fut = promPtr->get_future();
   const auto &getTxBatchLbd = [promPtr]
      (const std::vector<Tx> &batch, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         promPtr->set_exception(exPtr);
      }
      else {
         promPtr->set_value(batch);
      }
   };

   if (txHashes.size() == 0)
      return;

   if (!connPtr_->getTXsByHash(txHashes, getTxBatchLbd)) {
      throw ColoredCoinException("invalid DB state/connection");
   }
   const auto &txBatch = fut.get();

   auto zcPtr = std::make_shared<ColoredCoinZCSnapshot>();
   std::set<BinaryData> txsToCheck;
   for (auto& tx : txBatch) {
      if (tx.getTxHeight() != UINT32_MAX) {
         continue;
      }
      txsToCheck.insert(tx.getThisHash());

      //parse tx for origin address outputs
      for (unsigned i = 0; i < tx.getNumTxOut(); i++) {
         auto&& txOut = tx.getTxOutCopy(i);
         auto& scrAddr = txOut.getScrAddressStr();
         
         if (originAddresses_.find(scrAddr) == originAddresses_.end()) {
            continue;
         }
         addZcUtxo(currentSs, zcPtr, 
            tx.getThisHash(), i, txOut.getValue(), scrAddr);
      }
   }

   if (txsToCheck.size() > 0) {
      //process unconfirmed settlements
      processZcBatch(currentSs, zcPtr, txsToCheck);
   }

   //swap the new snapshot in
   std::atomic_store_explicit(&zcSnapshot_, zcPtr, std::memory_order_release);
}

////
uint64_t ColoredCoinTracker::getCcValueForAddress(const BinaryData& scrAddr) const
{
   /*takes prefixed scrAddr*/

   uint64_t tally = 0;
   auto&& addrOp = getSpendableOutpointsForAddress(scrAddr);
   for (auto& op : addrOp) {
      tally += op->value();
   }
   return tally;
}

////
std::vector<std::shared_ptr<CcOutpoint>> 
ColoredCoinTracker::getSpendableOutpointsForAddress(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr,
   const std::shared_ptr<ColoredCoinZCSnapshot>& zcPtr,
   const BinaryData& scrAddr, bool confirmedOnly) const
{
   /*takes prefixed scrAddr*/
   std::vector<std::shared_ptr<CcOutpoint>> result;
   const auto addr = bs::Address::fromHash(scrAddr).display();

   if (ssPtr != nullptr) {
      auto iter = ssPtr->scrAddrCcSet_.find(scrAddr.getRef());
      if (iter != ssPtr->scrAddrCcSet_.end()) {
         auto revokeIter = ssPtr->revokedAddresses_.find(scrAddr);
         if (revokeIter != ssPtr->revokedAddresses_.end()) {
            return {};
         }

         if (zcPtr != nullptr) {
            for (auto& ccOp : iter->second) {
               //is this outpoint spent by a zc?
               auto zcSpentIter = zcPtr->spentOutputs_.find(ccOp->getTxHash()->getRef());
               if (zcSpentIter != zcPtr->spentOutputs_.end()) {
                  auto idIter = zcSpentIter->second.find(ccOp->index());
                  if (idIter != zcSpentIter->second.end()) {
                     continue;
                  }
               }

               result.push_back(ccOp);
            }
         }
         else {
            for (auto& ccOp : iter->second) {
               result.push_back(ccOp);
            }
         }
      }
   }

   if (zcPtr == nullptr || confirmedOnly) {
      return result;
   }
   auto zcIter = zcPtr->scrAddrCcSet_.find(scrAddr.getRef());
   if (zcIter != zcPtr->scrAddrCcSet_.end()) {
      for (auto& ccOp : zcIter->second) {
         result.push_back(ccOp);
      }
   }

   return result;
}

////
std::vector<std::shared_ptr<CcOutpoint>> ColoredCoinTracker::getSpendableOutpointsForAddress(
   const BinaryData& scrAddr) const
{
   /*takes prefixed scrAddr*/
   auto ssPtr = snapshot();
   auto zcPtr = zcSnapshot();

   return getSpendableOutpointsForAddress(ssPtr, zcPtr, scrAddr, false);
}

bool ColoredCoinTracker::isTxHashValid(const BinaryData &txHash) const
{
   const auto ssPtr = snapshot();
   if (ssPtr && (ssPtr->utxoSet_.find(txHash) != ssPtr->utxoSet_.end())) {
      return true;
   }

   const auto zcPtr = zcSnapshot();
   if (zcPtr && (zcPtr->utxoSet_.find(txHash) != zcPtr->utxoSet_.end())) {
      return true;
   }
   return false;
}

////
void ColoredCoinTracker::eraseScrAddrOp(
   const std::shared_ptr<ColoredCoinSnapshot> &ssPtr
   , const std::shared_ptr<CcOutpoint>& opPtr)
{
   if (ssPtr == nullptr) {
      return;
   }
   auto scrAddrIter = ssPtr->scrAddrCcSet_.find(opPtr->getScrAddr()->getRef());
   if (scrAddrIter == ssPtr->scrAddrCcSet_.end()) {
      return;
   }
   scrAddrIter->second.erase(opPtr);
   if (scrAddrIter->second.size() == 0) {
      ssPtr->scrAddrCcSet_.erase(scrAddrIter);
   }
}

////
void ColoredCoinTracker::addScrAddrOp(
   std::map<BinaryData, OpPtrSet>& addrMap
   , const std::shared_ptr<CcOutpoint>& opPtr)
{
   auto scrAddrIter = addrMap.find(opPtr->getScrAddr()->getRef());
   if (scrAddrIter == addrMap.end()) {
      scrAddrIter = addrMap.insert(std::make_pair(
         opPtr->getScrAddr()->getRef(), OpPtrSet())).first;
   }

   scrAddrIter->second.insert(opPtr);
}

////
void ColoredCoinTracker::addUtxo(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr
   , const BinaryData& txHash, unsigned txOutIndex
   , uint64_t value, const BinaryData& scrAddr)
{
   std::shared_ptr<BinaryData> hashPtr;
   auto hashIter = ssPtr->utxoSet_.find(txHash.getRef());
   if (hashIter == ssPtr->utxoSet_.end()) {
      //create hash shared_ptr and map entry
      hashPtr = std::make_shared<BinaryData>(txHash);
      hashIter = ssPtr->utxoSet_.insert(std::make_pair(
         hashPtr->getRef(), std::map<unsigned, std::shared_ptr<CcOutpoint>>())).first;
   }
   else {
      //already have this hash entry, recover the hash shared_ptr
      if (hashIter->second.size() == 0)
         throw ColoredCoinException("empty utxo hash map");

      auto opPtr = hashIter->second.begin()->second;
      if (opPtr == nullptr) {
         throw ColoredCoinException("null utxo ptr");
      }
      hashPtr = opPtr->getTxHash();
   }

   //create output ptr
   auto opPtr = std::make_shared<CcOutpoint>(value, txOutIndex);
   opPtr->setTxHash(hashPtr);

   auto scrAddrPtr = getScrAddrPtr(ssPtr->scrAddrCcSet_, scrAddr);
   if (scrAddrPtr == nullptr) {
      scrAddrPtr = std::make_shared<BinaryData>(scrAddr);
   }
   opPtr->setScrAddr(scrAddrPtr);

   //add to utxo set
   hashIter->second.insert(std::make_pair(txOutIndex, opPtr));

   //add to scrAddr to utxo map
   addScrAddrOp(ssPtr->scrAddrCcSet_, opPtr);
}

////
void ColoredCoinTracker::addZcUtxo(
   const std::shared_ptr<ColoredCoinSnapshot>& ssPtr
   , const std::shared_ptr<ColoredCoinZCSnapshot>& zcPtr
   , const BinaryData& txHash, unsigned txOutIndex
   , uint64_t value, const BinaryData& scrAddr)
{
   std::shared_ptr<BinaryData> hashPtr;
   auto hashIter = zcPtr->utxoSet_.find(txHash.getRef());
   if (hashIter == zcPtr->utxoSet_.end()) {
      //dont have this entry, does the snapshot carry the hash shared_ptr?
      auto ssIter = ssPtr->utxoSet_.find(txHash.getRef());
      if (ssIter != ssPtr->utxoSet_.end() && ssIter->second.size() != 0) {
         hashPtr = ssIter->second.begin()->second->getTxHash();
      }
      else {
         //otherwise created the hash shared_ptr
         hashPtr = std::make_shared<BinaryData>(txHash);
      }

      //add the hash entry to the zc snapshot utxo map
      hashIter = zcPtr->utxoSet_.insert(std::make_pair(
         hashPtr->getRef(), std::map<unsigned, std::shared_ptr<CcOutpoint>>())).first;
   }
   else {
      //already have this hash entry, recover the hash shared_ptr
      if (hashIter->second.size() == 0) {
         throw ColoredCoinException("empty utxo hash map");
      }
      auto opPtr = hashIter->second.begin()->second;
      if (opPtr == nullptr) {
         throw ColoredCoinException("null utxo ptr");
      }
      hashPtr = opPtr->getTxHash();
   }

   //create output ptr
   auto opPtr = std::make_shared<CcOutpoint>(value, txOutIndex);
   opPtr->setTxHash(hashPtr);

   //look for ths scrAddr shared_ptr in the confirmed snapshot 
   auto scrAddrPtr = getScrAddrPtr(ssPtr->scrAddrCcSet_, scrAddr);
   if (scrAddrPtr == nullptr) {
      //missing from that snapshot, is it in the zc snapshot instead?
      scrAddrPtr = getScrAddrPtr(zcPtr->scrAddrCcSet_, scrAddr);

      //otherwise create it
      if (scrAddrPtr == nullptr) {
         scrAddrPtr = std::make_shared<BinaryData>(scrAddr);
      }
   }

   opPtr->setScrAddr(scrAddrPtr);

   //add to utxo set
   hashIter->second.insert(std::make_pair(txOutIndex, opPtr));

   //add to scrAddr to utxo map
   addScrAddrOp(zcPtr->scrAddrCcSet_, opPtr);
}

////
void ColoredCoinTracker::reorg(bool hard)
{
   if (!hard) {
      throw std::runtime_error("not implemented yet");
   }
   std::shared_ptr<ColoredCoinSnapshot> snapshot = nullptr;
   std::shared_ptr<ColoredCoinZCSnapshot> zcSnapshot = nullptr;

   std::atomic_store_explicit(
      &snapshot_, snapshot, std::memory_order_release);
   std::atomic_store_explicit(
      &zcSnapshot_, zcSnapshot, std::memory_order_release);

   startHeight_ = 0;
   zcCutOff_ = 0;
}

////
bool ColoredCoinTracker::goOnline()
{
   if (ready_.load(std::memory_order_relaxed)) {
      return false;
   }
   //TODO: load from snapshot

   //use default ACT if none is set
   if (actPtr_ == nullptr) {
      actPtr_ = std::make_shared<ColoredCoinACT>(connPtr_.get());
   }

   //register CC addresses
   std::vector<BinaryData> addrVec;

   for (auto& addr : originAddresses_) {
      addrVec.push_back(addr);
   }
   for (auto& addr : revocationAddresses_) {
      addrVec.push_back(addr);
   }
   auto &&regID = walletObj_->registerAddresses(addrVec, false);
   while (true) {
      /*
      Wait on regID. We have to do this because we can't start
      the ACT notification thread yet.
      */
      auto&& notif = actPtr_->popNotification();
      if ((notif->type_ == DBNS_Offline) && !notif->online_) {
         return false;
      }
      if (notif->type_ == DBNS_Refresh) {
         if (notif->ids_.size() == 1 &&
            notif->ids_[0] == regID) {
            break;
         }
      }
   }

   //update state
   auto&& addrSet = update();
   auto&& zcAddrSet = zcUpdate();

   //register set of addresses to track as a result of update routines
   addrVec.clear();
   addrSet.insert(zcAddrSet.begin(), zcAddrSet.end());
   for (auto& addr : addrSet) {
      addrVec.emplace_back(addr);
   }
   regID = walletObj_->registerAddresses(addrVec, true);

   while (true) {
      auto&& notif = actPtr_->popNotification();
      if ((notif->type_ == DBNS_Offline) && !notif->online_) {
         return false;
      }
      if (notif->type_ == DBNS_Refresh) {
         if (notif->ids_.size() == 1 &&
            notif->ids_[0] == regID) {
            break;
         }
      }
   }

   //set the act manager ptr to process notifications
   actPtr_->setCCManager(this);

   //start notification handler
   actPtr_->start();

   //flag ready
   ready_.store(true, std::memory_order_relaxed);

   return true;
}

////
void ColoredCoinTracker::shutdown()
{
   ready_.store(false, std::memory_order_relaxed);
   if (actPtr_) {
      actPtr_->stop();
   }
}

////
void ColoredCoinTracker::pushRefreshID(std::vector<BinaryData>& idVec)
{
   for (auto& id : idVec) {
      refreshQueue_.push_back(std::move(id));
   }
}

////
void ColoredCoinTracker::waitOnRefresh(const std::string& id)
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
uint64_t ColoredCoinTracker::getUnconfirmedCcValueForAddresses(
   const std::set<BinaryData>& scrAddrSet) const
{
   uint64_t total = 0;
   auto zcPtr = zcSnapshot();

   for (auto& scrAddr : scrAddrSet)
   {
      auto opVec = getSpendableOutpointsForAddress(nullptr, zcPtr, scrAddr, false);

      uint64_t scrAddrTotal = 0;
      for (auto op : opVec)
         scrAddrTotal += op->value();

      total += scrAddrTotal;
   }

   return total;
}

////
uint64_t ColoredCoinTracker::getConfirmedCcValueForAddresses(
   const std::set<BinaryData>& scrAddrSet) const
{
   /*
   This code has to grab ZC outpoints as well, as it substract ZC spends
   from the total of confirmed CC balance.
   */

   uint64_t total = 0;
   auto ssPtr = snapshot();
   auto zcPtr = zcSnapshot();

   for (auto& scrAddr : scrAddrSet)
   {
      auto opVec = getSpendableOutpointsForAddress(ssPtr, zcPtr, scrAddr, true);

      uint64_t scrAddrTotal = 0;
      for (auto op : opVec)
         scrAddrTotal += op->value();
      
      total += scrAddrTotal;
   }

   return total;
}

////
bool ColoredCoinTracker::getCCUtxoForAddresses(
   const std::set<BinaryData>& scrAddrSet, bool withZc,
   const std::function<void(std::vector<UTXO>, std::exception_ptr)>& cb) const
{
   auto ssPtr = snapshot();
   auto zcPtr = zcSnapshot();

   std::map<BinaryData, std::set<unsigned>> outpointMap;
   for (auto& scrAddr : scrAddrSet)
   {
      auto opVec = getSpendableOutpointsForAddress(ssPtr, zcPtr, scrAddr, withZc);
      for (auto& op : opVec)
      {
         auto iter = outpointMap.find(*op->getTxHash());
         if (iter == outpointMap.end())
         {
            iter = outpointMap.insert(std::make_pair(
               *op->getTxHash(), std::set<unsigned>())).first;
         }

         iter->second.insert(op->index());
      }
   }

   return connPtr_->getOutputsForOutpoints(outpointMap, withZc, cb);
}

////////////////////////////////////////////////////////////////////////////////
void ColoredCoinACT::onZCReceived(const std::vector<bs::TXEntry> &zcs)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
   dbns->zc_ = zcs;

   notifQueue_.push_back(std::move(dbns));
}

////
void ColoredCoinACT::onNewBlock(unsigned int height, unsigned int branchHeight)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_NewBlock);
   dbns->block_ = height;
   dbns->branchHeight_ = branchHeight;

   notifQueue_.push_back(std::move(dbns));
}

////
void ColoredCoinACT::onRefresh(const std::vector<BinaryData>& ids, bool online)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
   dbns->ids_ = ids;
   dbns->online_ = online;

   notifQueue_.push_back(std::move(dbns));
}

void ColoredCoinACT::onStateChanged(ArmoryState state)
{
   if (state == ArmoryState::Offline) {
      auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Offline);
      dbns->online_ = false;

      notifQueue_.push_back(std::move(dbns));
   }
}

////
void ColoredCoinACT::start()
{
   if (ccPtr_ == nullptr) {
      throw std::runtime_error("null cc manager ptr");
   }
   auto thrLbd = [this](void)->void
   {
      processNotification();
   };

   processThr_ = std::thread(thrLbd);
}

////
void ColoredCoinACT::stop()
{
   notifQueue_.terminate();

   if (processThr_.joinable()) {
      processThr_.join();
   }
}

////
void ColoredCoinACT::processNotification()
{
   ColoredCoinACT::RegistrationStruct regStruct;
   std::list<std::shared_ptr<DBNotificationStruct>> notifList;

   ////
   auto regLbd = [&regStruct](std::shared_ptr<DBNotificationStruct>& ptr)->bool
   {
      if (ptr->type_ == DBNS_Refresh) {
         if (ptr->ids_.size() == 1 &&
            ptr->ids_[0] == regStruct.regID_) {
            return true;
         }
      }

      return false;
   };

   ////
   auto processNotificationList = [this, &notifList, &regStruct](void)->void
   {
      while (notifList.size() > 0) {
         auto notifPtr = notifList.front();
         notifList.pop_front();

         switch (notifPtr->type_)
         {
         case DBNS_NewBlock:
         {
            /*
            reorg() if the branch height is set, will reset the state
            to either the branch point or entirely clear it. Regardless
            of resulting effect, we know the next call to update will
            yield a valid state.
            */
            if (notifPtr->branchHeight_ != UINT32_MAX) {
               ccPtr_->reorg(true);
            }
            auto&& addrSet = ccPtr_->update();

            //reorg() nuked the ZC snapshot, have to run zcUpdate anew
            if (notifPtr->branchHeight_ != UINT32_MAX) {
               auto&& zcAddrSet = ccPtr_->zcUpdate();

               //add the zcAddr to register to the update() address set
               addrSet.insert(zcAddrSet.begin(), zcAddrSet.end());
            }

            //now register the update() address set
            if (addrSet.size() > 0) {
               std::vector<BinaryData> addrVec;
               for (auto& addr : addrSet)
                  addrVec.emplace_back(addr);
               auto&& regID = ccPtr_->walletObj_->registerAddresses(addrVec, true);

               /*
               We have to wait on the refresh event for the registration
               before processing new notifications. Flag the registration ID
               and proceed.
               */
               regStruct.set(notifPtr, regID);
               return;
            }

            break;
         }

         case DBNS_ZC:
         {
            auto&& addrSet = ccPtr_->zcUpdate();

            //same as with DBNS_NewBlock address registration
            if (addrSet.size() > 0) {
               std::vector<BinaryData> addrVec;
               for (auto& addr : addrSet) {
                  addrVec.emplace_back(addr);
               }
               auto&& regID = ccPtr_->walletObj_->registerAddresses(addrVec, true);

               regStruct.set(notifPtr, regID);
            }

            break;
         }

         case DBNS_Refresh:
         {
            ccPtr_->pushRefreshID(notifPtr->ids_);
            break;
         }

         case DBNS_Offline:
            //TODO: put some disconnection processing here
            break;

         default:
            throw std::runtime_error("unexpected notification type");
         }

         onUpdate(notifPtr);
      }
   };

   ////
   while (true) {
      std::shared_ptr<DBNotificationStruct> dbNotifPtr;
      try {
         dbNotifPtr = notifQueue_.pop_front();
      }
      catch (StopBlockingLoop&) {
         break;
      }

      //is there a refresh ID to wait on?
      if (regStruct.isValid()) {
         if (!regLbd(dbNotifPtr)) {
            //didn't get the refresh id, stash this notif for later and 
            //wait on the queue
            notifList.push_back(dbNotifPtr);
            continue;
         }

         /*
         If we got this far we found the reg id. We can clear regStruct and
         go back to processing the notif vector.

         Note that we do not allow processing of this specific refresh ID, as it
         is for internal handling only and we don't want it reported further.
         However, we do need to report the parent notification that led to this
         registration event.
         */

         onUpdate(regStruct.notifPtr_);
         regStruct.clear();
      }
      else {
         /*
         This isn't a refresh notification, stash it for processing after
         we get the id we're looking for.
         */
         notifList.push_back(dbNotifPtr);
      }

      processNotificationList();
   }
}

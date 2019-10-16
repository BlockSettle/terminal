////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
//                                                                            //
//  Copyright (C) 2016-2018, goatpig                                          //            
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                   
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "ZeroConf.h"
#include "BlockDataMap.h"

using namespace std;

///////////////////////////////////////////////////////////////////////////////
//ZeroConfContainer Methods
///////////////////////////////////////////////////////////////////////////////

BinaryData ZeroConfContainer::getNewZCkey()
{
   uint32_t newId = topId_.fetch_add(1, memory_order_relaxed);
   BinaryData newKey = READHEX("ffff");
   newKey.append(WRITE_UINT32_BE(newId));

   return move(newKey);
}

///////////////////////////////////////////////////////////////////////////////
Tx ZeroConfContainer::getTxByHash(const BinaryData& txHash) const
{
   auto ss = getSnapshot();
   if (ss == nullptr)
      return Tx();

   auto& txhashmap = ss->txHashToDBKey_;
   const auto keyIter = txhashmap.find(txHash);

   if (keyIter == txhashmap.end())
      return Tx();

   auto& txmap = ss->txMap_;
   auto txiter = txmap.find(keyIter->second);

   if (txiter == txmap.end())
      return Tx();

   return txiter->second->tx_;
}
///////////////////////////////////////////////////////////////////////////////
bool ZeroConfContainer::hasTxByHash(const BinaryData& txHash) const
{
   auto ss = getSnapshot();
   auto& txhashmap = ss->txHashToDBKey_;
   return (txhashmap.find(txHash) != txhashmap.end());
}

///////////////////////////////////////////////////////////////////////////////
bool ZeroConfContainer::purge(
   const Blockchain::ReorganizationState& reorgState,
   shared_ptr<ZeroConfSharedStateSnapshot> ss,
   map<BinaryData, BinaryData>& minedKeys)
{
   if (db_ == nullptr || ss->txMap_.size() == 0)
      return true;

   set<BinaryData> keysToDelete;
   auto& zcMap = ss->txMap_;
   auto& txoutspentbyzc = ss->txOutsSpentByZC_;

   auto updateChildren = [&zcMap, &minedKeys, &txoutspentbyzc, this](
      BinaryDataRef& txHash, const BinaryData& blockKey,
      map<BinaryData, unsigned> minedHashes)->void
   {
      auto spentIter = outPointsSpentByKey_.find(txHash);
      if (spentIter == outPointsSpentByKey_.end())
         return;

      //is this zc mined or just invalidated?
      auto minedIter = minedHashes.find(txHash);
      if (minedIter == minedHashes.end())
         return;
      auto txid = minedIter->second;

      //list children by key
      set<BinaryDataRef> keysToClear;
      for (auto& op_pair : spentIter->second)
         keysToClear.insert(op_pair.second);

      //run through children, replace key
      for (auto& zckey : keysToClear)
      {
         auto zcIter = zcMap.find(zckey);
         if (zcIter == zcMap.end())
            continue;

         for (auto& input : zcIter->second->inputs_)
         {
            if (input.opRef_.getTxHashRef() != txHash)
               continue;

            auto prevKey = input.opRef_.getDbKey();
            txoutspentbyzc.erase(prevKey);
            input.opRef_.reset();

            BinaryWriter bw_key(8);
            bw_key.put_BinaryData(blockKey);
            bw_key.put_uint16_t(txid, BE);
            bw_key.put_uint16_t(input.opRef_.getIndex(), BE);

            minedKeys.insert(make_pair(prevKey, bw_key.getData()));
            input.opRef_.getDbKey() = bw_key.getData();

            zcIter->second->isChainedZc_ = false;
            zcIter->second->needsReparsed_ = true;
         }
      }
   };

   //lambda to purge zc map per block
   auto purgeZcMap =
      [&zcMap, &keysToDelete, &reorgState, &minedKeys, this, updateChildren](
         map<BinaryDataRef, set<unsigned>>& spentOutpoints,
         map<BinaryData, unsigned> minedHashes,
         const BinaryData& blockKey)->void
   {
      auto zcMapIter = zcMap.begin();
      while (zcMapIter != zcMap.end())
      {
         auto& zc = zcMapIter->second;
         bool invalidated = false;
         for (auto& input : zc->inputs_)
         {
            auto opIter = spentOutpoints.find(input.opRef_.getTxHashRef());
            if (opIter == spentOutpoints.end())
               continue;

            auto indexIter = opIter->second.find(input.opRef_.getIndex());
            if (indexIter == opIter->second.end())
               continue;

            //the outpoint for this zc is spent, invalidate the zc
            invalidated = true;
            break;
         }

         if (invalidated)
         {
            //mark for deletion
            keysToDelete.insert(zcMapIter->first);

            //this zc is now invalid, process its children
            auto&& zchash = zcMapIter->second->getTxHash().getRef();
            updateChildren(zchash, blockKey, minedHashes);
         }

         ++zcMapIter;
      }
   };

   if (!reorgState.prevTopStillValid_)
   {
      //reset resolved outpoints cause of reorg
      for (auto& zc_pair : zcMap)
         zc_pair.second->reset();
   }

   auto getIdSpoofLbd = [](const BinaryData&)->unsigned
   {
      return 0;
   };

   //get all txhashes for the new blocks
   ZcUpdateBatch batch;
   auto bcPtr = db_->blockchain();
   try
   {
      auto currentHeader = reorgState.prevTop_;
      if (!reorgState.prevTopStillValid_)
         currentHeader = reorgState.reorgBranchPoint_;

      //get the next header
      currentHeader = bcPtr->getHeaderByHash(currentHeader->getNextHash());

      //loop over headers
      while (currentHeader != nullptr)
      {
         //grab block
         auto&& rawBlock = db_->getRawBlock(
            currentHeader->getBlockHeight(),
            currentHeader->getDuplicateID());

         BlockData block;
         block.deserialize(
            rawBlock.getPtr(), rawBlock.getSize(),
            currentHeader, getIdSpoofLbd,
            false, false);

         //build up hash set
         map<BinaryDataRef, set<unsigned>> spentOutpoints;
         map<BinaryData, unsigned> minedHashes;
         auto txns = block.getTxns();
         for (unsigned txid = 0; txid < txns.size(); txid++)
         {
            auto& txn = txns[txid];
            for (unsigned iin = 0; iin < txn->txins_.size(); iin++)
            {
               auto txInRef = txn->getTxInRef(iin);
               BinaryRefReader brr(txInRef);
               auto hash = brr.get_BinaryDataRef(32);
               auto index = brr.get_uint32_t();

               auto& indexSet = spentOutpoints[hash];
               indexSet.insert(index);
            }

            minedHashes.insert(make_pair(txn->getHash(), txid));
         }

         purgeZcMap(spentOutpoints, minedHashes,
            currentHeader->getBlockDataKey());

         if (BlockDataManagerConfig::getDbType() != ARMORY_DB_SUPER)
         {
            //purge mined hashes
            for (auto& minedHash : minedHashes)
            {
               allZcTxHashes_.erase(minedHash.first);
               batch.txHashesToDelete_.insert(minedHash.first);
            }
         }

         //next block
         if (currentHeader->getThisHash() == reorgState.newTop_->getThisHash())
            break;

         auto& bhash = currentHeader->getNextHash();
         currentHeader = bcPtr->getHeaderByHash(bhash);
      }
   }
   catch (...)
   {
   }

   if (reorgState.prevTopStillValid_)
   {
      dropZC(ss, keysToDelete);
      return true;
   }
   else
   {
      //reset containers and resolve outpoints anew after a reorg
      reset();
      preprocessZcMap(zcMap);

      //delete keys from DB
      batch.keysToDelete_ = move(keysToDelete);
      auto fut = batch.getCompletedFuture();
      updateBatch_.push_back(move(batch));
      fut.wait();

      return false;
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::preprocessZcMap(
   map<BinaryDataRef, shared_ptr<ParsedTx>>& zcMap)
{
   //run threads to preprocess the zcMap
   auto counter = make_shared<atomic<unsigned>>();
   counter->store(0, memory_order_relaxed);

   vector<shared_ptr<ParsedTx>> txVec;
   txVec.reserve(zcMap.size());
   for (auto& txPair : zcMap)
      txVec.push_back(txPair.second);

   auto parserLdb = [this, &txVec, counter](void)->void
   {
      while (1)
      {
         auto id = counter->fetch_add(1, memory_order_relaxed);
         if (id >= txVec.size())
            return;

         auto txIter = txVec.begin() + id;
         this->preprocessTx(*(*txIter));
      }
   };

   vector<thread> parserThreads;
   for (unsigned i = 1; i < thread::hardware_concurrency(); i++)
      parserThreads.push_back(thread(parserLdb));
   parserLdb();

   for (auto& thr : parserThreads)
   {
      if (thr.joinable())
         thr.join();
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::reset()
{
   keyToSpentScrAddr_.clear();
   outPointsSpentByKey_.clear();
   keyToFundedScrAddr_.clear();
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::dropZC(
   shared_ptr<ZeroConfSharedStateSnapshot> ss, const BinaryDataRef& key)
{
   auto& txMap = ss->txMap_;
   auto& txioMap = ss->txioMap_;
   auto& txOuts = ss->txOutsSpentByZC_;

   auto iter = txMap.find(key);
   if (iter == txMap.end())
      return;

   /*** lambas ***/
   auto dropTxios = [&txioMap](
      const BinaryDataRef zcKey,
      const BinaryDataRef scrAddr)->void
   {
      auto mapIter = txioMap.find(scrAddr);
      if (mapIter == txioMap.end())
         return;

      map<BinaryData, shared_ptr<TxIOPair>> revisedTxioMap;
      auto& txios = mapIter->second;
      for (auto& txio_pair : *txios)
      {
         //if the txio is keyed by our zc, do not keep it
         if (txio_pair.first.startsWith(zcKey))
            continue;

         //otherwise, it should have a txin
         if (!txio_pair.second->hasTxIn())
            continue;

         //at this point the txin is ours. if the txout is not zc, drop the txio
         if (!txio_pair.second->hasTxOutZC())
            continue;

         //wipe our txin from the txio, keep the txout as it belongs to another zc
         auto txio = make_shared<TxIOPair>(*txio_pair.second);
         txio->setTxIn(BinaryData());
         revisedTxioMap.insert(move(make_pair(txio_pair.first, move(txio))));
      }

      if (revisedTxioMap.size() == 0)
      {
         txioMap.erase(mapIter);
         return;
      }

      auto txioMapPtr = make_shared<
         map<BinaryData, shared_ptr<TxIOPair>>>(move(revisedTxioMap));
      mapIter->second = txioMapPtr;
   };

   /*** drop tx from snapshot ***/
   auto&& hashToDelete = iter->second->getTxHash().getRef();
   ss->txHashToDBKey_.erase(hashToDelete);

   //drop from outPointsSpentByKey_
   outPointsSpentByKey_.erase(hashToDelete);
   for (auto& input : iter->second->inputs_)
   {
      auto opIter = 
         outPointsSpentByKey_.find(input.opRef_.getTxHashRef());
      if (opIter == outPointsSpentByKey_.end())
         continue;

      //erase the index
      opIter->second.erase(input.opRef_.getIndex());

      //erase the txhash if the index map is empty
      if (opIter->second.size() == 0)
      {
         outPointsSpentByKey_.erase(opIter);
      }
      else if (opIter->first.getPtr() == input.opRef_.getTxHashRef().getPtr())
      {
         //outpoint hash reference is owned by this tx object, rekey it
         
         //1. save the idmap
         auto indexMap = move(opIter->second);

         //2. erase current entry
         outPointsSpentByKey_.erase(opIter);

         //3. look for another zc among the referenced spenders
         for (auto& id : indexMap)
         {
            auto& tx_key = id.second;
            if (tx_key == key)
               continue;

            //4. we have a different zc, grab it
            auto replaceIter = txMap.find(tx_key);
            
            //sanity checks
            if (replaceIter == txMap.end() || 
               id.first >= replaceIter->second->inputs_.size())
               continue;

            //5. grab hash reference and key by it
            auto replaceHash = 
               replaceIter->second->inputs_[id.first].opRef_.getTxHashRef();

            pair<BinaryDataRef, map<unsigned, BinaryDataRef>> new_pair;
            new_pair.first = replaceHash;
            new_pair.second = move(indexMap);
            outPointsSpentByKey_.insert(new_pair);
            break;
         }
      }

   }

   //drop from keyToSpendScrAddr_
   auto saSetIter = keyToSpentScrAddr_.find(key);
   if (saSetIter != keyToSpentScrAddr_.end())
   {
      for (auto& sa : *saSetIter->second)
      {
         BinaryData sabd(sa);
         dropTxios(key, sa);
      }
      keyToSpentScrAddr_.erase(saSetIter);
   }

   //drop from keyToFundedScrAddr_
   auto fundedIter = keyToFundedScrAddr_.find(key);
   if (fundedIter != keyToFundedScrAddr_.end())
   {
      for (auto& sa : fundedIter->second)
      {
         BinaryData sabd(sa);
         dropTxios(key, sa);
      }

      keyToFundedScrAddr_.erase(fundedIter);
   }

   //drop from txOutsSpentByZC_
   for (auto& input : iter->second->inputs_)
   {
      if (!input.isResolved())
         continue;
      txOuts.erase(input.opRef_.getDbKey());
   }

   //delete tx
   txMap.erase(iter);
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::dropZC(
   shared_ptr<ZeroConfSharedStateSnapshot> ss, const set<BinaryData>& zcKeys)
{
   if (zcKeys.size() == 0)
      return;

   for (auto& key : zcKeys)
      dropZC(ss, key);

   ZcUpdateBatch batch;
   batch.keysToDelete_ = zcKeys;
   updateBatch_.push_back(move(batch));
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::parseNewZC(void)
{
   while (1)
   {
      ZcActionStruct zcAction;
      map<BinaryDataRef, shared_ptr<ParsedTx>> zcMap;
      try
      {
         zcAction = move(newZcStack_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         break;
      }

      bool notify = true;
      map<BinaryData, BinaryData> previouslyValidKeys;
      map<BinaryData, BinaryData> minedKeys;
      auto ss = ZeroConfSharedStateSnapshot::copy(snapshot_);

      switch (zcAction.action_)
      {
      case Zc_Purge:
      {
         //build set of currently valid keys
         for (auto& txpair : ss->txMap_)
         {
            previouslyValidKeys.emplace(
               make_pair(txpair.first, txpair.second->getTxHash()));
         }

         //purge mined zc
         auto result = purge(zcAction.reorgState_, ss, minedKeys);
         notify = false;

         //setup batch with all tracked zc
         if (zcAction.batch_ == nullptr)
            zcAction.batch_ = make_shared<ZeroConfBatch>();
         zcAction.batch_->txMap_ = ss->txMap_;
         zcAction.batch_->isReadyPromise_.set_value(true);

         if (!result)
         {
            reset();
            ss = nullptr;
         }
      }

      case Zc_NewTx:
      {
         if (zcAction.batch_ == nullptr)
            continue;

         auto fut = zcAction.batch_->isReadyPromise_.get_future();
         fut.wait();
         zcMap = move(zcAction.batch_->txMap_);
         break;
      }

      case Zc_Shutdown:
         reset();
         return;

      default:
         continue;
      }

      parseNewZC(move(zcMap), ss, true, notify);
      if (zcAction.resultPromise_ != nullptr)
      {
         auto purgePacket = make_shared<ZcPurgePacket>();
         purgePacket->minedTxioKeys_ = move(minedKeys);

         for (auto& wasValid : previouslyValidKeys)
         {
            auto keyIter = snapshot_->txMap_.find(wasValid.first);
            if (keyIter != snapshot_->txMap_.end())
               continue;

            purgePacket->invalidatedZcKeys_.insert(wasValid);
         }

         zcAction.resultPromise_->set_value(purgePacket);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::parseNewZC(
   map<BinaryDataRef, shared_ptr<ParsedTx>> zcMap,
   shared_ptr<ZeroConfSharedStateSnapshot> ss,
   bool updateDB, bool notify)
{
   unique_lock<mutex> lock(parserMutex_);
   ZcUpdateBatch batch;

   auto iter = zcMap.begin();
   while (iter != zcMap.end())
   {
      if (iter->second->status() == Tx_Mined ||
         iter->second->status() == Tx_Invalid)
         zcMap.erase(iter++);
      else
         ++iter;
   }

   if (ss == nullptr)
      ss = make_shared<ZeroConfSharedStateSnapshot>();

   auto& txmap = ss->txMap_;
   auto& txhashmap = ss->txHashToDBKey_;
   auto& txoutsspentbyzc = ss->txOutsSpentByZC_;
   auto& txiomap = ss->txioMap_;

   for (auto& newZCPair : zcMap)
   {
      if (BlockDataManagerConfig::getDbType() != ARMORY_DB_SUPER)
      {
         auto& txHash = newZCPair.second->getTxHash();
         auto insertIter = allZcTxHashes_.insert(txHash);
         if (!insertIter.second)
            continue;
      }
      else
      {
         if (txmap.find(newZCPair.first) != txmap.end())
            continue;
      }

      batch.zcToWrite_.insert(newZCPair);
   }

   map<BinaryDataRef, shared_ptr<ParsedTx>> invalidatedTx;

   bool hasChanges = false;

   map<string, pair<bool, ParsedZCData>> flaggedBDVs;

   //zckey fetch lambda
   auto getzckeyfortxhash = [&txhashmap]
   (const BinaryData& txhash, BinaryData& zckey_output)->bool
   {
      auto global_iter = txhashmap.find(txhash);
      if (global_iter == txhashmap.end())
         return false;

      zckey_output = global_iter->second;
      return true;
   };

   //zc tx fetch lambda
   auto getzctxforkey = [&txmap]
   (const BinaryData& zc_key)->const ParsedTx&
   {
      auto global_iter = txmap.find(zc_key);
      if (global_iter == txmap.end())
         throw runtime_error("no zc tx for this key");

      return *global_iter->second;
   };

   function<set<BinaryData>(const BinaryData&)> getTxChildren =
      [&](const BinaryData& zcKey)->set<BinaryData>
   {
      set<BinaryData> childKeys;
      BinaryDataRef txhash;

      try
      {
         auto& parsedTx = getzctxforkey(zcKey);
         txhash = parsedTx.getTxHash().getRef();
      }
      catch (exception&)
      {
         return childKeys;
      }

      auto spentOP_iter = outPointsSpentByKey_.find(txhash);
      if (spentOP_iter != outPointsSpentByKey_.end())
      {
         auto& keymap = spentOP_iter->second;

         for (auto& keypair : keymap)
         {
            auto&& childrenKeys = getTxChildren(keypair.second);
            childKeys.insert(move(keypair.second));
            for (auto& c_key : childrenKeys)
               childKeys.insert(move(c_key));
         }
      }

      return childKeys;
   };

   //zc logic
   set<BinaryDataRef> addedZcKeys;
   for (auto& newZCPair : zcMap)
   {
      auto&& txHash = newZCPair.second->getTxHash().getRef();
      if (txhashmap.find(txHash) != txhashmap.end())
      {
         /*
         Already have this ZC, why is it passed for parsing again?
         Most common reason for reparsing is a child zc which parent's
         was mined (outpoint now resolves to a dbkey instead of 
         the previous zckey)
         */

         if (!newZCPair.second->needsReparsed_)
         {
            //tx wasn't flagged as needing processed again, skip it
            continue;
         }

         //turn of reparse flag
         newZCPair.second->needsReparsed_ = false;
      }

      {
         auto&& bulkData = ZCisMineBulkFilter(
            *newZCPair.second, newZCPair.first,
            getzckeyfortxhash, getzctxforkey);

         //check for replacement
         {
            //loop through all outpoints consumed by this ZC
            for (auto& idSet : bulkData.outPointsSpentByKey_)
            {
               set<BinaryData> childKeysToDrop;

               //compare them to the list of currently spent outpoints
               auto hashIter = outPointsSpentByKey_.find(idSet.first);
               if (hashIter == outPointsSpentByKey_.end())
                  continue;

               for (auto opId : idSet.second)
               {
                  auto idIter = hashIter->second.find(opId.first);
                  if (idIter != hashIter->second.end())
                  {
                     try
                     {
                        //gather replaced tx children
                        auto&& keySet = getTxChildren(idIter->second);
                        keySet.insert(idIter->second);
                        hasChanges = true;

                        //drop the replaced transactions
                        for (auto& key : keySet)
                        {
                           auto txiter = txmap.find(key);
                           if (txiter != txmap.end())
                              invalidatedTx.insert(*txiter);
                           childKeysToDrop.insert(key);
                           //dropZC(ss, key);
                        }
                     }
                     catch (exception&)
                     {
                        continue;
                     }
                  }
               }

               for (auto& childKey : childKeysToDrop)
                  dropZC(ss, childKey);
            }
         }

         //add ZC if its relevant
         if (newZCPair.second->status() != Tx_Invalid &&
            newZCPair.second->status() != Tx_Uninitialized &&
            !bulkData.isEmpty())
         {
            addedZcKeys.insert(newZCPair.first);
            hasChanges = true;

            //merge spent outpoints
            txoutsspentbyzc.insert(
               bulkData.txOutsSpentByZC_.begin(),
               bulkData.txOutsSpentByZC_.end());

            for (auto& idmap : bulkData.outPointsSpentByKey_)
            {
               //Since the outpoint is being replaced, the tx holding the 
               //reference to the txhash this pair is keyed by will expire.
               //Re-key the pair by the replacing hash

               pair<BinaryDataRef, map<unsigned, BinaryDataRef>> outpoints;
               outpoints.first = idmap.first;
               auto opIter = outPointsSpentByKey_.find(idmap.first);
               if (opIter != outPointsSpentByKey_.end())
               {
                  outpoints.second = move(opIter->second);
                  outPointsSpentByKey_.erase(opIter);
               }

               for (auto& idpair : idmap.second)
                  outpoints.second[idpair.first] = idpair.second;

               outPointsSpentByKey_.insert(move(outpoints));
            }

            //merge scrAddr spent by key
            for (auto& sa_pair : bulkData.keyToSpentScrAddr_)
            {
               auto insertResult = keyToSpentScrAddr_.insert(sa_pair);
               if (insertResult.second == false)
                  insertResult.first->second = move(sa_pair.second);
            }

            //merge scrAddr funded by key
            typedef map<BinaryDataRef, set<BinaryDataRef>>::iterator mapbd_setbd_iter;
            keyToFundedScrAddr_.insert(
               move_iterator<mapbd_setbd_iter>(bulkData.keyToFundedScrAddr_.begin()),
               move_iterator<mapbd_setbd_iter>(bulkData.keyToFundedScrAddr_.end()));

            //merge new txios
            txhashmap[txHash] = newZCPair.first;
            txmap[newZCPair.first] = newZCPair.second;

            for (auto& saTxio : bulkData.scrAddrTxioMap_)
            {
               auto saIter = txiomap.find(saTxio.first);
               if (saIter != txiomap.end())
               {
                  for (auto& newTxio : *saTxio.second)
                  {
                     auto insertIter = saIter->second->insert(newTxio);
                     if (insertIter.second == false)
                        insertIter.first->second = newTxio.second;
                  }
               }
               else
               {
                  txiomap.insert(move(saTxio));
               }
            }

            //notify BDVs
            for (auto& bdvMap : bulkData.flaggedBDVs_)
            {
               auto& parserResult = flaggedBDVs[bdvMap.first];
               parserResult.second.mergeTxios(bdvMap.second);
               parserResult.first = true;
            }
         }
      }
   }

   if (updateDB && batch.hasData())
   {
      //post new zc for writing to db, no need to wait on it
      updateBatch_.push_back(move(batch));
   }

   //find BDVs affected by invalidated keys
   if (invalidatedTx.size() > 0)
   {
      //TODO: multi thread this at some point

      for (auto& tx_pair : invalidatedTx)
      {
         //gather all scrAddr from invalidated tx
         set<BinaryDataRef> addrRefs;

         for (auto& input : tx_pair.second->inputs_)
         {
            if (!input.isResolved())
               continue;

            addrRefs.insert(input.scrAddr_.getRef());
         }

         for (auto& output : tx_pair.second->outputs_)
            addrRefs.insert(output.scrAddr_.getRef());

         //flag relevant BDVs
         for (auto& addrRef : addrRefs)
         {
            auto&& bdvid_set = bdvCallbacks_->hasScrAddr(addrRef);
            for (auto& bdvid : bdvid_set)
            {
               auto& bdv = flaggedBDVs[bdvid];
               bdv.second.invalidatedKeys_.insert(
                  make_pair(tx_pair.first, tx_pair.second->getTxHash()));
               bdv.first = true;
               hasChanges = true;
            }
         }
      }
   }

   //swap in new state
   atomic_store_explicit(&snapshot_, ss, memory_order_release);

   //notify bdvs
   if (!hasChanges)
      return;

   if (!notify)
      return;

   //prepare notifications
   auto newZcKeys =
      make_shared<map<BinaryData, shared_ptr<set<BinaryDataRef>>>>();
   auto newTxPtrMap =
      make_shared<map<BinaryDataRef, shared_ptr<ParsedTx>>>();
   for (auto& newKey : addedZcKeys)
   {
      //fill key to spent scrAddr map
      shared_ptr<set<BinaryDataRef>> spentScrAddr = nullptr;
      auto iter = keyToSpentScrAddr_.find(newKey);
      if (iter != keyToSpentScrAddr_.end())
         spentScrAddr = iter->second;

      auto addr_pair = make_pair(newKey, move(spentScrAddr));
      newZcKeys->insert(move(addr_pair));

      //fill new zc map
      auto zcIter = txmap.find(newKey);
      if (zcIter == txmap.end())
      {
         LOGWARN << "this should not happen!";
         continue;
      }

      newTxPtrMap->insert(*zcIter);
   }

   for (auto& bdvMap : flaggedBDVs)
   {
      if (!bdvMap.second.first)
         continue;

      NotificationPacket notificationPacket(bdvMap.first);
      notificationPacket.txMap_ = newTxPtrMap;

      for (auto& sa : bdvMap.second.second.txioKeys_)
      {
         auto saIter = txiomap.find(sa);
         if (saIter == txiomap.end())
            continue;

         notificationPacket.txioMap_.insert(*saIter);
      }

      if (bdvMap.second.second.invalidatedKeys_.size() != 0)
      {
         notificationPacket.purgePacket_ = make_shared<ZcPurgePacket>();
         notificationPacket.purgePacket_->invalidatedZcKeys_ =
            move(bdvMap.second.second.invalidatedKeys_);
      }

      notificationPacket.newKeysAndScrAddr_ = newZcKeys;
      bdvCallbacks_->pushZcNotification(notificationPacket);
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::preprocessTx(ParsedTx& tx) const
{
   auto& txHash = tx.getTxHash();
   auto&& txref = db_->getTxRef(txHash);

   if (txref.isInitialized())
   {
      tx.state_ = Tx_Mined;
      return;
   }

   uint8_t const * txStartPtr = tx.tx_.getPtr();
   unsigned len = tx.tx_.getSize();

   auto nTxIn = tx.tx_.getNumTxIn();
   auto nTxOut = tx.tx_.getNumTxOut();

   //try to resolve as many outpoints as we can. unresolved outpoints are 
   //either invalid or (most likely) children of unconfirmed transactions
   if (nTxIn != tx.inputs_.size())
   {
      tx.inputs_.clear();
      tx.inputs_.resize(nTxIn);
   }

   if (nTxOut != tx.outputs_.size())
   {
      tx.outputs_.clear();
      tx.outputs_.resize(nTxOut);
   }

   for (uint32_t iin = 0; iin < nTxIn; iin++)
   {
      auto& txIn = tx.inputs_[iin];
      if (txIn.isResolved())
         continue;

      auto& opRef = txIn.opRef_;

      if (!opRef.isInitialized())
      {
         auto offset = tx.tx_.getTxInOffset(iin);
         if (offset > len)
            throw runtime_error("invalid txin offset");
         BinaryDataRef inputDataRef(txStartPtr + offset, len - offset);
         opRef.unserialize(inputDataRef);
      }

      if (!opRef.isResolved())
      {
         //resolve outpoint to dbkey
         txIn.opRef_.resolveDbKey(db_);
         if (!opRef.isResolved())
            continue;
      }

      //grab txout
      StoredTxOut stxOut;
      if (!db_->getStoredTxOut(stxOut, opRef.getDbKey()))
         continue;

      if (db_->armoryDbType() == ARMORY_DB_SUPER)
         opRef.getDbKey() = stxOut.getDBKey(false);

      if (stxOut.isSpent())
      {
         tx.state_ = Tx_Invalid;
         return;
      }

      //set txin address and value
      txIn.scrAddr_ = stxOut.getScrAddress();
      txIn.value_ = stxOut.getValue();
   }

   for (uint32_t iout = 0; iout < nTxOut; iout++)
   {
      auto& txOut = tx.outputs_[iout];
      if (txOut.isInitialized())
         continue;

      auto offset = tx.tx_.getTxOutOffset(iout);
      auto len = tx.tx_.getTxOutOffset(iout + 1) - offset;

      BinaryRefReader brr(txStartPtr + offset, len);
      txOut.value_ = brr.get_uint64_t();

      auto scriptLen = brr.get_var_int();
      auto scriptRef = brr.get_BinaryDataRef(scriptLen);
      txOut.scrAddr_ = move(BtcUtils::getTxOutScrAddr(scriptRef));
   }

   tx.isRBF_ = tx.tx_.isRBF();


   bool txInResolved = true;
   for (auto& txin : tx.inputs_)
   {
      if (txin.isResolved())
         continue;

      txInResolved = false;
      break;
   }

   if (!txInResolved)
      tx.state_ = Tx_Unresolved;
   else
      tx.state_ = Tx_Resolved;
}

///////////////////////////////////////////////////////////////////////////////
ZeroConfContainer::BulkFilterData ZeroConfContainer::ZCisMineBulkFilter(
   ParsedTx& parsedTx, const BinaryDataRef& ZCkey,
   function<bool(const BinaryData&, BinaryData&)> getzckeyfortxhash,
   function<const ParsedTx&(const BinaryData&)> getzctxforkey)
{
   BulkFilterData bulkData;
   if (parsedTx.status() == Tx_Mined || parsedTx.status() == Tx_Invalid)
      return bulkData;

   auto mainAddressSet = scrAddrMap_->get();

   auto filter = [mainAddressSet, this]
      (const BinaryData& addr)->pair<bool, set<string>>
   {
      pair<bool, set<string>> flaggedBDVs;
      flaggedBDVs.first = false;

      auto addrIter = mainAddressSet->find(addr.getRef());
      if (addrIter == mainAddressSet->end())
      {
         if (BlockDataManagerConfig::getDbType() == ARMORY_DB_SUPER)
            flaggedBDVs.first = true;

         return flaggedBDVs;
      }

      flaggedBDVs.first = true;
      flaggedBDVs.second = move(bdvCallbacks_->hasScrAddr(addr.getRef()));
      return flaggedBDVs;
   };

   auto insertNewZc = [&bulkData](const BinaryData& sa,
      BinaryData txiokey, shared_ptr<TxIOPair> txio,
      set<string> flaggedBDVs, bool consumesTxOut)->void
   {
      if (consumesTxOut)
         bulkData.txOutsSpentByZC_.insert(txiokey);

      auto& key_txioPair = bulkData.scrAddrTxioMap_[sa];

      if (key_txioPair == nullptr)
         key_txioPair = make_shared<map<BinaryData, shared_ptr<TxIOPair>>>();

      (*key_txioPair)[txiokey] = move(txio);

      for (auto& bdvId : flaggedBDVs)
         bulkData.flaggedBDVs_[bdvId].txioKeys_.insert(sa);
   };

   if (parsedTx.status() == Tx_Uninitialized ||
      parsedTx.status() == Tx_ResolveAgain)
      preprocessTx(parsedTx);

   auto& txHash = parsedTx.getTxHash();
   bool isRBF = parsedTx.isRBF_;
   bool isChained = parsedTx.isChainedZc_;

   //if parsedTx has unresolved outpoint, they are most likely ZC
   for (auto& input : parsedTx.inputs_)
   {
      if (input.isResolved())
      {
         //check resolved key is valid
         if (input.opRef_.isZc())
         {
            try
            {
               isChained = true;
               auto& chainedZC = getzctxforkey(input.opRef_.getDbTxKeyRef());
               if (chainedZC.status() == Tx_Invalid)
                  throw runtime_error("invalid parent zc");
            }
            catch (exception&)
            {
               parsedTx.state_ = Tx_Invalid;
               return bulkData;
            }
         }
         else
         {
            auto&& keyRef = input.opRef_.getDbKey().getSliceRef(0, 4);
            auto height = DBUtils::hgtxToHeight(keyRef);
            auto dupId = DBUtils::hgtxToDupID(keyRef);

            if (db_->getValidDupIDForHeight(height) != dupId)
            {
               parsedTx.state_ = Tx_Invalid;
               return bulkData;
            }
         }

         continue;
      }

      auto& opZcKey = input.opRef_.getDbKey();
      if (!getzckeyfortxhash(input.opRef_.getTxHashRef(), opZcKey))
      {
         if (BlockDataManagerConfig::getDbType() == ARMORY_DB_SUPER ||
            allZcTxHashes_.find(input.opRef_.getTxHashRef()) == allZcTxHashes_.end())
            continue;
      }

      isChained = true;

      try
      {
         auto& chainedZC = getzctxforkey(opZcKey);
         auto&& chainedTxOut = chainedZC.tx_.getTxOutCopy(input.opRef_.getIndex());

         input.value_ = chainedTxOut.getValue();
         input.scrAddr_ = chainedTxOut.getScrAddressStr();
         isRBF |= chainedZC.tx_.isRBF();
         input.opRef_.setTime(chainedZC.tx_.getTxTime());

         opZcKey.append(WRITE_UINT16_BE(input.opRef_.getIndex()));
      }
      catch (runtime_error&)
      {
         continue;
      }
   }

   parsedTx.isRBF_ = isRBF;
   parsedTx.isChainedZc_ = isChained;

   //spent txios
   unsigned iin = 0;
   for (auto& input : parsedTx.inputs_)
   {
      auto inputId = iin++;
      if (!input.isResolved())
      {
         if (db_->armoryDbType() == ARMORY_DB_SUPER)
         {
            parsedTx.state_ = Tx_Invalid;
            return bulkData;
         }
         else
         {
            parsedTx.state_ = Tx_ResolveAgain;
         }

         continue;
      }

      //keep track of all outputs this ZC consumes
      auto& id_map = bulkData.outPointsSpentByKey_[input.opRef_.getTxHashRef()];
      id_map.insert(make_pair(input.opRef_.getIndex(), ZCkey));

      auto&& flaggedBDVs = filter(input.scrAddr_);
      if (!isChained && !flaggedBDVs.first)
         continue;

      auto txio = make_shared<TxIOPair>(
         TxRef(input.opRef_.getDbTxKeyRef()), input.opRef_.getIndex(),
         TxRef(ZCkey), inputId);

      txio->setTxHashOfOutput(input.opRef_.getTxHashRef());
      txio->setTxHashOfInput(txHash);
      txio->setValue(input.value_);
      auto tx_time = input.opRef_.getTime();
      if (tx_time == UINT64_MAX)
         tx_time = parsedTx.tx_.getTxTime();
      txio->setTxTime(tx_time);
      txio->setRBF(isRBF);
      txio->setChained(isChained);

      auto&& txioKey = txio->getDBKeyOfOutput();
      insertNewZc(input.scrAddr_, move(txioKey), move(txio),
         move(flaggedBDVs.second), true);

      auto& updateSet = bulkData.keyToSpentScrAddr_[ZCkey];
      if (updateSet == nullptr)
         updateSet = make_shared<set<BinaryDataRef>>();
      updateSet->insert(input.scrAddr_.getRef());
   }

   //funded txios
   unsigned iout = 0;
   for (auto& output : parsedTx.outputs_)
   {
      auto outputId = iout++;

      auto&& flaggedBDVs = filter(output.scrAddr_);
      if (flaggedBDVs.first)
      {
         auto txio = make_shared<TxIOPair>(TxRef(ZCkey), outputId);

         txio->setValue(output.value_);
         txio->setTxHashOfOutput(txHash);
         txio->setTxTime(parsedTx.tx_.getTxTime());
         txio->setUTXO(true);
         txio->setRBF(isRBF);
         txio->setChained(isChained);

         auto& fundedScrAddr = bulkData.keyToFundedScrAddr_[ZCkey];
         fundedScrAddr.insert(output.scrAddr_.getRef());

         auto&& txioKey = txio->getDBKeyOfOutput();
         insertNewZc(output.scrAddr_, move(txioKey),
            move(txio), move(flaggedBDVs.second), false);
      }
   }

   return bulkData;
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::clear()
{
   snapshot_.reset();
}

///////////////////////////////////////////////////////////////////////////////
bool ZeroConfContainer::isTxOutSpentByZC(const BinaryData& dbkey) const
{
   auto ss = getSnapshot();
   if (ss == nullptr)
      return false;

   auto& txoutset = ss->txOutsSpentByZC_;
   if (txoutset.find(dbkey) != txoutset.end())
      return true;

   return false;
}

///////////////////////////////////////////////////////////////////////////////
map<BinaryData, shared_ptr<TxIOPair>> ZeroConfContainer::getUnspentZCforScrAddr(
   BinaryData scrAddr) const
{
   auto ss = getSnapshot();
   if (ss == nullptr)
      return map<BinaryData, shared_ptr<TxIOPair>>();

   auto& txiomapptr = ss->txioMap_;
   auto saIter = txiomapptr.find(scrAddr);

   if (saIter != txiomapptr.end())
   {
      auto& zcMap = saIter->second;
      map<BinaryData, shared_ptr<TxIOPair>> returnMap;

      for (auto& zcPair : *zcMap)
      {
         if (zcPair.second->hasTxIn())
            continue;

         returnMap.insert(zcPair);
      }

      return returnMap;
   }

   return map<BinaryData, shared_ptr<TxIOPair>>();
}

///////////////////////////////////////////////////////////////////////////////
map<BinaryData, shared_ptr<TxIOPair>> ZeroConfContainer::getRBFTxIOsforScrAddr(
   BinaryData scrAddr) const
{
   auto ss = getSnapshot();
   auto& txiomapptr = ss->txioMap_;
   auto saIter = txiomapptr.find(scrAddr);

   if (saIter != txiomapptr.end())
   {
      auto& zcMap = saIter->second;
      map<BinaryData, shared_ptr<TxIOPair>> returnMap;

      for (auto& zcPair : *zcMap)
      {
         if (!zcPair.second->hasTxIn())
            continue;

         if (!zcPair.second->isRBF())
            continue;

         returnMap.insert(zcPair);
      }

      return returnMap;
   }

   return map<BinaryData, shared_ptr<TxIOPair>>();
}

///////////////////////////////////////////////////////////////////////////////
vector<TxOut> ZeroConfContainer::getZcTxOutsForKey(
   const set<BinaryData>& keys) const
{
   vector<TxOut> result;
   auto ss = getSnapshot();
   auto& txmap = ss->txMap_;

   for (auto& key : keys)
   {
      auto zcKey = key.getSliceRef(0, 6);

      auto txIter = txmap.find(zcKey);
      if (txIter == txmap.end())
         continue;

      auto& theTx = *txIter->second;

      auto outIdRef = key.getSliceRef(6, 2);
      auto outId = READ_UINT16_BE(outIdRef);

      auto&& txout = theTx.tx_.getTxOutCopy(outId);
      result.push_back(move(txout));
   }

   return result;
}

///////////////////////////////////////////////////////////////////////////////
vector<UnspentTxOut> ZeroConfContainer::getZcUTXOsForKey(
   const set<BinaryData>& keys) const
{
   vector<UnspentTxOut> result;
   auto ss = getSnapshot();
   auto& txmap = ss->txMap_;

   for (auto& key : keys)
   {
      auto zcKey = key.getSliceRef(0, 6);

      auto txIter = txmap.find(zcKey);
      if (txIter == txmap.end())
         continue;

      auto& theTx = *txIter->second;

      auto outIdRef = key.getSliceRef(6, 2);
      auto outId = READ_UINT16_BE(outIdRef);

      auto&& txout = theTx.tx_.getTxOutCopy(outId);
      UnspentTxOut utxo(
         theTx.getTxHash(), outId, UINT32_MAX,
         txout.getValue(), txout.getScript());

      result.push_back(move(utxo));
   }

   return result;
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::updateZCinDB()
{
   while (true)
   {
      ZcUpdateBatch batch;
      try
      {
         batch = move(updateBatch_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         break;
      }

      if (!batch.hasData())
         continue;

      auto&& tx = db_->beginTransaction(ZERO_CONF, LMDB::ReadWrite);
      for (auto& zc_pair : batch.zcToWrite_)
      {
            /*TODO: speed this up*/
            StoredTx zcTx;
            zcTx.createFromTx(zc_pair.second->tx_, true, true);
            db_->putStoredZC(zcTx, zc_pair.first);
      }

      for (auto& txhash : batch.txHashes_)
      {
         //if the key is not to be found in the txMap_, this is a ZC txhash
         db_->putValue(ZERO_CONF, txhash, BinaryData());
      }

      for (auto& key : batch.keysToDelete_)
      {
         BinaryData keyWithPrefix;
         if (key.getSize() == 6)
         {
            keyWithPrefix.resize(7);
            uint8_t* keyptr = keyWithPrefix.getPtr();
            keyptr[0] = DB_PREFIX_ZCDATA;
            memcpy(keyptr + 1, key.getPtr(), 6);
         }
         else
         {
            keyWithPrefix = key;
         }

         auto dbIter = db_->getIterator(ZERO_CONF);

         if (!dbIter->seekTo(keyWithPrefix))
            continue;

         vector<BinaryData> ktd;
         ktd.push_back(keyWithPrefix);

         do
         {
            BinaryDataRef thisKey = dbIter->getKeyRef();
            if (!thisKey.startsWith(keyWithPrefix))
               break;

            ktd.push_back(thisKey);
         } while (dbIter->advanceAndRead(DB_PREFIX_ZCDATA));

         for (auto _key : ktd)
            db_->deleteValue(ZERO_CONF, _key);
      }

      for (auto& key : batch.txHashesToDelete_)
         db_->deleteValue(ZERO_CONF, key);

      batch.setCompleted(true);
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::loadZeroConfMempool(bool clearMempool)
{
   map<BinaryDataRef, shared_ptr<ParsedTx>> zcMap;

   {
      auto&& tx = db_->beginTransaction(ZERO_CONF, LMDB::ReadOnly);
      auto dbIter = db_->getIterator(ZERO_CONF);

      if (!dbIter->seekToStartsWith(DB_PREFIX_ZCDATA))
         return;

      do
      {
         BinaryDataRef zcKey = dbIter->getKeyRef();

         if (zcKey.getSize() == 7)
         {
            //Tx, grab it from DB
            StoredTx zcStx;
            db_->getStoredZcTx(zcStx, zcKey);

            //add to newZCMap_
            auto&& zckey = zcKey.getSliceCopy(1, 6);
            Tx zctx(zcStx.getSerializedTx());
            zctx.setTxTime(zcStx.unixTime_);

            auto parsedTx = make_shared<ParsedTx>(zckey);
            parsedTx->tx_ = move(zctx);

            zcMap.insert(move(make_pair(
               parsedTx->getKeyRef(), move(parsedTx))));
         }
         else if (zcKey.getSize() == 9)
         {
            //TxOut, ignore it
            continue;
         }
         else if (zcKey.getSize() == 32)
         {
            //tx hash
            allZcTxHashes_.insert(zcKey);
         }
         else
         {
            //shouldn't hit this
            LOGERR << "Unknown key found in ZC mempool";
            break;
         }
      } while (dbIter->advanceAndRead(DB_PREFIX_ZCDATA));
   }

   if (clearMempool == true)
   {
      LOGWARN << "Mempool was flagged for deletion!";
      ZcUpdateBatch batch;
      auto fut = batch.getCompletedFuture();

      for (const auto& zcTx : zcMap)
         batch.keysToDelete_.insert(zcTx.first);

      updateBatch_.push_back(move(batch));
      fut.wait();
   }
   else if (zcMap.size())
   {
      preprocessZcMap(zcMap);

      //set highest used index
      auto lastEntry = zcMap.rbegin();
      auto& topZcKey = lastEntry->first;
      topId_.store(READ_UINT32_BE(topZcKey.getSliceCopy(2, 4)) + 1);

      //no need to update the db nor notify bdvs on init
      parseNewZC(move(zcMap), nullptr, false, false);
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::init(shared_ptr<ScrAddrFilter> saf, bool clearMempool)
{
   LOGINFO << "Enabling zero-conf tracking";

   scrAddrMap_ = saf->getScrAddrMapPtr();
   loadZeroConfMempool(clearMempool);

   auto processZcThread = [this](void)->void
   {
      parseNewZC();
   };

   auto updateZcThread = [this](void)->void
   {
      updateZCinDB();
   };

   parserThreads_.push_back(thread(processZcThread));
   parserThreads_.push_back(thread(updateZcThread));

   increaseParserThreadPool(1);

   zcEnabled_.store(true, memory_order_relaxed);
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::processInvTxVec(
   vector<InvEntry> invVec, bool extend, unsigned timeout)
{
   if (!isEnabled())
      return;

   //skip this entirely if there are no addresses to scan the ZCs against
   if (scrAddrMap_->size() == 0 && extend &&
      BlockDataManagerConfig::getDbType() != ARMORY_DB_SUPER)
      return;

   if (extend &&
      parserThreadCount_ < invVec.size() &&
      parserThreadCount_ < maxZcThreadCount_)
      increaseParserThreadPool(invVec.size());

   //setup batch
   auto batch = make_shared<ZeroConfBatch>();
   batch->timeout_ = timeout;
   for (unsigned i = 0; i < invVec.size(); i++)
   {
      pair<BinaryDataRef, shared_ptr<ParsedTx>> txPair;
      auto&& key = getNewZCkey();
      auto ptx = make_shared<ParsedTx>(key);
      txPair.first = ptx->getKeyRef();
      txPair.second = move(ptx);

      batch->txMap_.insert(move(txPair));
   }

   //pass individual packets to parser threads
   auto mapIter = batch->txMap_.begin();
   for (auto& entry : invVec)
   {
      ZeroConfInvPacket packet;
      packet.batchPtr_ = batch;
      packet.zcKey_ = mapIter->first;
      packet.invEntry_ = move(entry);

      newInvTxStack_.push_back(move(packet));
      ++mapIter;
   }

   //register batch with main zc processing thread
   ZcActionStruct zac;
   zac.batch_ = batch;
   zac.action_ = Zc_NewTx;
   newZcStack_.push_back(move(zac));
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::processInvTxThread(void)
{
   while (1)
   {
      ZeroConfInvPacket packet;
      try
      {
         packet = move(newInvTxStack_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         break;
      }

      try
      {
         processInvTxThread(packet);
      }
      catch (BitcoinP2P_Exception&)
      {
         //ignore any p2p connection related exceptions
         packet.batchPtr_->incrementCounter();
         continue;
      }
      catch (runtime_error &e)
      {
         LOGERR << "zc parser error: " << e.what();
         packet.batchPtr_->incrementCounter();
      }
      catch (exception&)
      {
         LOGERR << "unhandled exception in ZcParser thread!";
         packet.batchPtr_->incrementCounter();
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::processInvTxThread(ZeroConfInvPacket& packet)
{
   packet.invEntry_.invtype_ = Inv_Msg_Witness_Tx;
   auto payload =
      networkNode_->getTx(packet.invEntry_, packet.batchPtr_->timeout_);

   auto txIter = packet.batchPtr_->txMap_.find(packet.zcKey_);
   if (txIter == packet.batchPtr_->txMap_.end())
      throw runtime_error("batch does not have zckey");

   auto& txObj = txIter->second;

   auto payloadtx = dynamic_pointer_cast<Payload_Tx>(payload);
   if (payloadtx == nullptr)
   {
      txObj->state_ = Tx_Invalid;
      packet.batchPtr_->incrementCounter();
      return;
   }

   //push raw tx with current time
   auto& rawTx = payloadtx->getRawTx();
   txObj->tx_.unserialize(&rawTx[0], rawTx.size());
   txObj->tx_.setTxTime(time(0));

   preprocessTx(*txObj);
   packet.batchPtr_->incrementCounter();
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::pushZcToParser(const BinaryDataRef& rawTx)
{
   pair<BinaryDataRef, shared_ptr<ParsedTx>> zcpair;
   auto&& key = getNewZCkey();
   auto ptx = make_shared<ParsedTx>(key);

   ptx->tx_.unserialize(rawTx.getPtr(), rawTx.getSize());
   ptx->tx_.setTxTime(time(0));

   zcpair.first = ptx->getKeyRef();
   zcpair.second = move(ptx);

   ZcActionStruct actionstruct;
   actionstruct.batch_ = make_shared<ZeroConfBatch>();
   actionstruct.batch_->txMap_.insert(move(zcpair));
   actionstruct.action_ = Zc_NewTx;
   newZcStack_.push_back(move(actionstruct));
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::broadcastZC(const BinaryData& rawzc,
   const string& bdvId, uint32_t timeout_ms)
{
   Tx zcTx(rawzc);

   //get tx hash
   auto&& txHash = zcTx.getThisHash();
   auto&& txHashStr = txHash.toHexStr();

   if (!networkNode_->connected())
   {
      string errorMsg("node is offline, cannot broadcast");
      LOGWARN << errorMsg;
      bdvCallbacks_->errorCallback(bdvId, errorMsg, txHashStr);
      return;
   }

   //create inv payload
   InvEntry entry;
   entry.invtype_ = Inv_Msg_Tx;
   memcpy(entry.hash, txHash.getPtr(), 32);

   vector<InvEntry> invVec;
   invVec.push_back(entry);

   Payload_Inv payload_inv;
   payload_inv.setInvVector(invVec);

   //create getData payload packet
   auto&& payload = make_unique<Payload_Tx>();
   vector<uint8_t> rawtx;
   rawtx.resize(rawzc.getSize());
   memcpy(&rawtx[0], rawzc.getPtr(), rawzc.getSize());

   payload->setRawTx(move(rawtx));
   auto getDataProm = make_shared<promise<bool>>();
   auto getDataFut = getDataProm->get_future();

   BitcoinP2P::getDataPayload getDataPayload;
   getDataPayload.payload_ = move(payload);
   getDataPayload.promise_ = getDataProm;

   pair<BinaryData, BitcoinP2P::getDataPayload> getDataPair;
   getDataPair.first = txHash;
   getDataPair.second = move(getDataPayload);

   //Register tx hash for watching before broadcasting the inv. This guarantees we will
   //catch any reject packet before trying to fetch the tx back for confirmation.
   auto gds = make_shared<GetDataStatus>();
   networkNode_->registerGetTxCallback(txHash, gds);

   //register getData payload
   networkNode_->getDataPayloadMap_.insert(move(getDataPair));

   //send inv packet
   networkNode_->sendMessage(move(payload_inv));
   LOGINFO << "sent inv packet";

   //wait on getData future
   bool sent = false;
   if (timeout_ms == 0)
   {
      getDataFut.wait();
   }
   else
   {
      auto getDataFutStatus = getDataFut.wait_for(chrono::milliseconds(timeout_ms));
      if (getDataFutStatus != future_status::ready)
      {
         gds->setStatus(false);
         LOGERR << "tx broadcast timed out (send)";
         gds->setMessage("tx broadcast timed out (send)");
      }
      else
      {
         LOGINFO << "got getData packet";
         sent = true;
      }
   }

   networkNode_->getDataPayloadMap_.erase(txHash);

   if (!sent)
   {
      auto&& errorMsg = gds->getMessage();
      networkNode_->unregisterGetTxCallback(txHash);
      bdvCallbacks_->errorCallback(bdvId, errorMsg, txHashStr);
      return;
   }

   auto watchTxFuture = gds->getFuture();

   //try to fetch tx by hash from node
   if (PEER_USES_WITNESS)
      entry.invtype_ = Inv_Msg_Witness_Tx;

   auto grabtxlambda = [this, timeout_ms](InvEntry inventry)->void
   {
      vector<InvEntry> invVec;
      invVec.push_back(move(inventry));

      processInvTxVec(move(invVec), false, timeout_ms);
   };

   thread grabtxthread(grabtxlambda, move(entry));
   if (grabtxthread.joinable())
      grabtxthread.detach();

   LOGINFO << "grabbing tx from node";

   if (timeout_ms == 0)
   {
      watchTxFuture.wait();
   }
   else
   {
      auto status = watchTxFuture.wait_for(chrono::milliseconds(timeout_ms));
      if (status != future_status::ready)
      {
         gds->setStatus(false);
         LOGERR << "tx broadcast timed out (get)";
         gds->setMessage("tx broadcast timed out (get)");
      }
   }

   networkNode_->unregisterGetTxCallback(txHash);

   if (!gds->status())
   {
      auto&& errorMsg = gds->getMessage();
      bdvCallbacks_->errorCallback(bdvId, errorMsg, txHashStr);
   }
   else
   {
      LOGINFO << "tx broadcast successfully";
   }
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::shutdown()
{
   newInvTxStack_.terminate();
   newZcStack_.terminate();
   updateBatch_.terminate();

   vector<thread::id> idVec;
   for (auto& parser : parserThreads_)
   {
      idVec.push_back(parser.get_id());
      if (parser.joinable())
         parser.join();
   }

   DatabaseContainer_Sharded::clearThreadShardTx(idVec);
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfContainer::increaseParserThreadPool(unsigned count)
{
   unique_lock<mutex> lock(parserThreadMutex_);

   //start Zc parser thread
   auto processZcThread = [this](void)->void
   {
      processInvTxThread();
   };

   for (unsigned i = parserThreadCount_; i < count; i++)
      parserThreads_.push_back(thread(processZcThread));

   parserThreadCount_ = parserThreads_.size();
   LOGINFO << "now running " << parserThreadCount_ << " zc parser threads";
}

///////////////////////////////////////////////////////////////////////////////
shared_ptr<map<BinaryData, shared_ptr<TxIOPair>>>
   ZeroConfContainer::getTxioMapForScrAddr(const BinaryData& scrAddr) const
{
   auto ss = getSnapshot();
   auto& txiomap = ss->txioMap_;

   auto iter = txiomap.find(scrAddr);
   if (iter == txiomap.end())
      return nullptr;

   return iter->second;
}

///////////////////////////////////////////////////////////////////////////////
shared_ptr<ParsedTx> ZeroConfContainer::getTxByKey(const BinaryData& key) const
{
   auto ss = getSnapshot();
   auto iter = ss->txMap_.find(key.getRef());
   if (iter == ss->txMap_.end())
      return nullptr;

   return iter->second;
}

///////////////////////////////////////////////////////////////////////////////
BinaryDataRef ZeroConfContainer::getKeyForHash(const BinaryDataRef& hash) const
{
   auto ss = getSnapshot();
   auto iter = ss->txHashToDBKey_.find(hash);
   if (iter == ss->txHashToDBKey_.end())
      return BinaryDataRef();

   return iter->second;
}

///////////////////////////////////////////////////////////////////////////////
BinaryDataRef ZeroConfContainer::getHashForKey(const BinaryDataRef& key) const
{
   auto ss = getSnapshot();
   auto iter = ss->txMap_.find(key);
   if (iter == ss->txMap_.end())
      return BinaryDataRef();

   return iter->second->getTxHash().getRef();
}

///////////////////////////////////////////////////////////////////////////////
TxOut ZeroConfContainer::getTxOutCopy(
   const BinaryDataRef key, unsigned index) const
{
   auto&& tx = getTxByKey(key);
   if (tx == nullptr)
      return TxOut();

   return tx->tx_.getTxOutCopy(index);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// OutPointRef
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void OutPointRef::unserialize(uint8_t const * ptr, uint32_t remaining)
{
   if (remaining < 36)
      throw runtime_error("ptr is too short to be an outpoint");

   BinaryDataRef bdr(ptr, remaining);
   BinaryRefReader brr(bdr);

   txHash_ = brr.get_BinaryDataRef(32);
   txOutIndex_ = brr.get_uint32_t();
}

////////////////////////////////////////////////////////////////////////////////
void OutPointRef::unserialize(BinaryDataRef bdr)
{
   unserialize(bdr.getPtr(), bdr.getSize());
}

////////////////////////////////////////////////////////////////////////////////
void OutPointRef::resolveDbKey(LMDBBlockDatabase *dbPtr)
{
   if (txHash_.getSize() == 0 || txOutIndex_ == UINT16_MAX)
      throw runtime_error("empty outpoint hash");

   auto&& key = dbPtr->getDBKeyForHash(txHash_);
   if (key.getSize() != 6)
      return;

   BinaryWriter bw;
   bw.put_BinaryData(key);
   bw.put_uint16_t(txOutIndex_, BE);

   dbKey_ = bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
BinaryDataRef OutPointRef::getDbTxKeyRef() const
{
   if (!isResolved())
      throw runtime_error("unresolved outpoint key");

   return dbKey_.getSliceRef(0, 6);
}

////////////////////////////////////////////////////////////////////////////////
bool OutPointRef::isInitialized() const
{
   return txHash_.getSize() == 32 && txOutIndex_ != UINT16_MAX;
}

////////////////////////////////////////////////////////////////////////////////
bool OutPointRef::isZc() const
{
   if (!isResolved())
      return false;

   auto ptr = dbKey_.getPtr();
   auto val = (uint16_t*)ptr;
   return *val == 0xFFFF;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// ParsedTxIn
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool ParsedTxIn::isResolved() const
{
   if (!opRef_.isResolved())
      return false;

   if (scrAddr_.getSize() == 0 || value_ == UINT64_MAX)
      return false;

   return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// ParsedTx
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool ParsedTx::isResolved() const
{
   if (state_ == Tx_Uninitialized)
      return false;

   if (!tx_.isInitialized())
      return false;

   if (inputs_.size() != tx_.getNumTxIn() ||
      outputs_.size() != tx_.getNumTxOut())
      return false;

   for (auto& input : inputs_)
   {
      if (!input.isResolved())
         return false;
   }

   return true;
}

////////////////////////////////////////////////////////////////////////////////
void ParsedTx::reset()
{
   for (auto& input : inputs_)
      input.opRef_.reset();
   tx_.setChainedZC(false);

   state_ = Tx_Uninitialized;
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& ParsedTx::getTxHash(void) const
{
   if (txHash_.getSize() == 0)
      txHash_ = move(tx_.getThisHash());
   return txHash_;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// ZeroConfCallbacks
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ZeroConfCallbacks::~ZeroConfCallbacks() 
{}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// ZcUpdateBatch
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
shared_future<bool> ZcUpdateBatch::getCompletedFuture()
{
   if (completed_ == nullptr)
      completed_ = make_unique<promise<bool>>();
   return completed_->get_future();
}

////////////////////////////////////////////////////////////////////////////////
void ZcUpdateBatch::setCompleted(bool val)
{
   if (completed_ == nullptr)
      return;

   completed_->set_value(val);
}

////////////////////////////////////////////////////////////////////////////////
bool ZcUpdateBatch::hasData() const
{
   if (zcToWrite_.size() > 0 ||
      txHashes_.size() > 0 ||
      keysToDelete_.size() > 0)
      return true;
   
   return false;
}
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "NodeUnitTest.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
NodeUnitTest::NodeUnitTest(uint32_t magic_word) :
   BitcoinP2P("", "", magic_word)
{
   counter_.store(1, memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
void NodeUnitTest::notifyNewBlock(void)
{
   InvEntry ie;
   ie.invtype_ = Inv_Msg_Block;

   vector<InvEntry> vecIE;
   vecIE.push_back(ie);

   processInvBlock(move(vecIE));
}

////////////////////////////////////////////////////////////////////////////////
void NodeUnitTest::mineNewBlock(unsigned count, const BinaryData& h160)
{
   Recipient_P2PKH recipient(h160, 50 * COIN);
   mineNewBlock(count, &recipient);
}

////////////////////////////////////////////////////////////////////////////////
std::map<unsigned, BinaryData> NodeUnitTest::mineNewBlock(
   unsigned count, ScriptRecipient* recipient)
{
   BinaryData prevHash;
   uint32_t timestamp;
   BinaryData diffBits;
   unsigned blockHeight;
   
   {
      auto top = blockchain_->top();
      prevHash = top->getThisHash();
      timestamp = top->getTimestamp();
      diffBits  = top->getDiffBits();
      blockHeight = top->getBlockHeight() + 1;
   }

   std::map<unsigned, BinaryData> result;

   for (unsigned i = 0; i < count; i++)
   {
      //create coinbase tx
      BinaryWriter bwCoinbase;
      {
         //version
         bwCoinbase.put_uint32_t(1);

         //input count
         bwCoinbase.put_var_int(1);

         //outpoint
         BinaryData outpoint(36);
         memset(outpoint.getPtr(), 0, outpoint.getSize());
         bwCoinbase.put_BinaryData(outpoint);

         //txin script
         bwCoinbase.put_var_int(4);
         bwCoinbase.put_uint32_t(counter_.fetch_add(1, memory_order_relaxed));

         //sequence
         bwCoinbase.put_uint32_t(UINT32_MAX);

         //output count
         bwCoinbase.put_var_int(1);

         //output script
         auto& outputScript = recipient->getSerializedScript();
         bwCoinbase.put_BinaryData(outputScript);

         //locktime
         bwCoinbase.put_uint32_t(0);
      }

      auto coinbaseObj = make_shared<MempoolObject>();
      coinbaseObj->rawTx_ = bwCoinbase.getData();
      coinbaseObj->hash_ = BtcUtils::getHash256(coinbaseObj->rawTx_);
      coinbaseObj->order_ = 0;

      result.insert(make_pair(blockHeight++, coinbaseObj->hash_));

      //grab all tx in the mempool, respect ordering
      vector<shared_ptr<MempoolObject>> mempoolV;
      map<BinaryDataRef, shared_ptr<MempoolObject>> purgedMempool;
      mempoolV.push_back(coinbaseObj);
      for (auto& obj : mempool_)
      {
         if (obj.second->blocksUntilMined_ == 0)
         {
            mempoolV.push_back(obj.second);
         }
         else
         {
            --obj.second->blocksUntilMined_;
            auto objPair = make_pair(obj.second->hash_.getRef(), move(obj.second));
            purgedMempool.emplace(objPair);
         }
      }

      sort(mempoolV.begin(), mempoolV.end());

      //compute merkle
      vector<BinaryData> txHashes;
      for (auto& obj : mempoolV)
         txHashes.push_back(obj->hash_);
      auto merkleRoot = BtcUtils::calculateMerkleRoot(txHashes);

      //clear mempool
      mempool_ = move(purgedMempool);

      //build block
      BinaryWriter bwBlock;

      {
         /* build header */

         //version
         bwBlock.put_uint32_t(1);

         //previous hash
         bwBlock.put_BinaryData(prevHash);

         //merkle root
         bwBlock.put_BinaryData(merkleRoot);

         //timestamp
         bwBlock.put_uint32_t(timestamp + 600);

         //diff bits
         bwBlock.put_BinaryData(diffBits);

         //nonce
         bwBlock.put_uint32_t(0);

         //update prev hash and timestamp for the next block
         prevHash = BtcUtils::getHash256(bwBlock.getDataRef());
         timestamp += 600;
      }

      {
         /* serialize block */

         //tx count
         bwBlock.put_var_int(mempoolV.size());

         //tx
         for (auto& txObj : mempoolV)
            bwBlock.put_BinaryData(txObj->rawTx_);
      }

      {
         /* append to blocks data file */

         //get file stream
         auto lastFileName = filesPtr_->getLastFileName();
         auto fStream = ofstream(lastFileName, ios::binary | ios::app);

         BinaryWriter bwHeader;

         //magic byte
         bwHeader.put_uint32_t(getMagicWord());

         //block size
         bwHeader.put_uint32_t(bwBlock.getSize());

         fStream.write(
            (const char*)bwHeader.getDataRef().getPtr(), bwHeader.getSize());

         //block data
         fStream.write(
            (const char*)bwBlock.getDataRef().getPtr(), bwBlock.getSize());

         fStream.close();
      }
   }

   //push notification
   notifyNewBlock();

   return result;
}

////////////////////////////////////////////////////////////////////////////////
void NodeUnitTest::pushZC(const vector<pair<BinaryData, unsigned>>& txVec)
{
   vector<InvEntry> invVec;

   //save tx to fake mempool
   for (auto& tx : txVec)
   {
      auto obj = make_shared<MempoolObject>();
      Tx txNew(tx.first);
      obj->rawTx_ = tx.first;
      obj->hash_ = txNew.getThisHash();
      obj->order_ = counter_.fetch_add(1, memory_order_relaxed);
      obj->blocksUntilMined_ = tx.second;

      /***
      cheap zc replacement code: check for outpoint reuse, assume unit
      tests will not push conflicting transactions that aren't legit RBF
      ***/

      auto poolIter = mempool_.begin();
      while(poolIter != mempool_.end())
      {
         Tx txMempool(poolIter->second->rawTx_);
         if (txNew.getThisHash() == txMempool.getThisHash())
            return;

         bool hasCollision = false;
         for (unsigned i = 0; i < txMempool.getNumTxIn(); i++)
         {
            auto txinMempool = txMempool.getTxInCopy(i);

            for (unsigned y = 0; y < txNew.getNumTxIn(); y++)
            {
               auto txinNew = txNew.getTxInCopy(y);

               if (txinMempool.getOutPoint() == txinNew.getOutPoint())
               {
                  hasCollision = true;
                  break;
               }
            }
            
            if (hasCollision)
               break;
         }

         if (hasCollision)
         {
            mempool_.erase(poolIter++);
            continue;
         }

         ++poolIter;
      }

      auto objPair = make_pair(obj->hash_.getRef(), move(obj));
      auto insertIter = mempool_.insert(move(objPair));

      //notify the zc parser
      InvEntry ie;
      ie.invtype_ = Inv_Msg_Witness_Tx;
      memcpy(ie.hash, insertIter.first->second->hash_.getPtr(), 32);
      invVec.emplace_back(ie);
   }

   processInvTx(invVec);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<Payload> NodeUnitTest::getTx(const InvEntry& ie, uint32_t timeout)
{
   //find tx in mempool
   BinaryDataRef hashRef(ie.hash, 32);
   auto iter = mempool_.find(hashRef);
   if (iter == mempool_.end())
      return nullptr;

   //create payload and return
   auto payload = make_shared<Payload_Tx>(
      iter->second->rawTx_.getPtr(), iter->second->rawTx_.getSize());
   return payload;
}

////////////////////////////////////////////////////////////////////////////////
void NodeUnitTest::setBlockchain(std::shared_ptr<Blockchain> bcPtr)
{
   blockchain_ = bcPtr;
}

////////////////////////////////////////////////////////////////////////////////
void NodeUnitTest::setBlockFiles(std::shared_ptr<BlockFiles> filesPtr)
{
   filesPtr_ = filesPtr;
}
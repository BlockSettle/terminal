////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_NODEUNITTEST
#define _H_NODEUNITTEST

#include <memory>
#include <vector>
#include <map>

#include "../BinaryData.h"
#include "../BtcUtils.h"

#include "../BlockUtils.h"
#include "../BitcoinP2p.h"
#include "../Blockchain.h"
#include "../ScriptRecipient.h"
#include "../BlockDataMap.h"

struct UnitTestBlock
{
   BinaryData rawHeader_;
   BinaryData headerHash_;

   Tx coinbase_;
   std::vector<Tx> transactions_;

   unsigned height_;
   unsigned timestamp_;
   BinaryData diffBits_;
};

class NodeUnitTest : public BitcoinP2P
{
private:
   struct MinedHeader
   {
      BinaryData prevHash_;
      unsigned blockHeight_;

      unsigned timestamp_;
      BinaryData diffBits_;
   };

   struct MempoolObject
   {
      BinaryData rawTx_;
      BinaryData hash_;
      unsigned order_;
      unsigned blocksUntilMined_ = 0;
      bool staged_;

      bool operator<(const MempoolObject& rhs) const { return order_ < rhs.order_; }
   };

   std::map<BinaryDataRef, std::shared_ptr<MempoolObject>> mempool_;
   std::vector<UnitTestBlock> blocks_;
   std::atomic<unsigned> counter_;
   
   std::shared_ptr<Blockchain> blockchain_ = nullptr;
   std::shared_ptr<BlockFiles> filesPtr_ = nullptr;

   MinedHeader header_;

public:
   NodeUnitTest(uint32_t magic_word);

   //virtuals
   void connectToNode(bool async) {}

   void shutdown(void)
   {
      //clean up remaining lambdas
      BitcoinP2P::shutdown();
   }

   //locals
   void notifyNewBlock(void);

   std::map<unsigned, BinaryData> mineNewBlock(
      BlockDataManager* bdm, unsigned count, const BinaryData& h160);
   std::map<unsigned, BinaryData> mineNewBlock(
      BlockDataManager* bdm, unsigned, ScriptRecipient*);

   std::shared_ptr<Payload> getTx(const InvEntry& ie, uint32_t timeout);

   std::vector<UnitTestBlock> getMinedBlocks(void) const { return blocks_; }
   void setReorgBranchPoint(std::shared_ptr<BlockHeader>);

   //<raw tx, blocks to wait until mining>
   void pushZC(const std::vector<std::pair<BinaryData, unsigned>>&, bool);

   //set
   void setBlockchain(std::shared_ptr<Blockchain>);
   void setBlockFiles(std::shared_ptr<BlockFiles>);
};

#endif
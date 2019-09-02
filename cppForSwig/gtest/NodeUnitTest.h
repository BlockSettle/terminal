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

#include "../BitcoinP2p.h"
#include "../Blockchain.h"
#include "../ScriptRecipient.h"
#include "../BlockDataMap.h"

class NodeUnitTest : public BitcoinP2P
{
   struct MempoolObject
   {
      BinaryData rawTx_;
      BinaryData hash_;
      unsigned order_;
      unsigned blocksUntilMined_ = 0;

      bool operator<(const MempoolObject& rhs) const { return order_ < rhs.order_; }
   };

   std::map<BinaryDataRef, std::shared_ptr<MempoolObject>> mempool_;
   std::atomic<unsigned> counter_;
   
   std::shared_ptr<Blockchain> blockchain_ = nullptr;
   std::shared_ptr<BlockFiles> filesPtr_ = nullptr;

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
   void mineNewBlock(unsigned count, const BinaryData& h160);
   std::map<unsigned, BinaryData> mineNewBlock(unsigned, ScriptRecipient*);
   std::shared_ptr<Payload> getTx(const InvEntry& ie, uint32_t timeout);
 
   //<raw tx, blocks to wait until mining>
   void pushZC(const std::vector<std::pair<BinaryData, unsigned>>&);

   //set
   void setBlockchain(std::shared_ptr<Blockchain>);
   void setBlockFiles(std::shared_ptr<BlockFiles>);
};

#endif
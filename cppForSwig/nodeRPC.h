////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_NODERPC_
#define _H_NODERPC_

#include <mutex>
#include <memory>
#include <string>
#include <functional>

#include "SocketObject.h"
#include "StringSockets.h"
#include "BtcUtils.h"

#include "JSON_codec.h"

#include "ReentrantLock.h"
#include "BlockDataManagerConfig.h"

////
enum NodeStatus
{
   NodeStatus_Offline,
   NodeStatus_Online,
   NodeStatus_OffSync
};

////
enum RpcStatus
{
   RpcStatus_Disabled,
   RpcStatus_BadAuth,
   RpcStatus_Online,
   RpcStatus_Error_28
};

////
enum ChainStatus
{
   ChainStatus_Unknown,
   ChainStatus_Syncing,
   ChainStatus_Ready
};

////
class RpcError : public std::runtime_error
{
public:
   RpcError(void) : std::runtime_error("")
   {}
};

class ConfMismatch
{
   const unsigned expected_;
   const unsigned actual_;

public:
   ConfMismatch(unsigned expected, unsigned actual) :
      expected_(expected), actual_(actual)
   {}

   unsigned expected(void) const { return expected_; }
   unsigned actual(void) const { return actual_; }
};

////////////////////////////////////////////////////////////////////////////////
struct FeeEstimateResult
{
   bool smartFee_ = false;
   float feeByte_ = 0;

   std::string error_;
};

////////////////////////////////////////////////////////////////////////////////
class NodeChainState
{
   friend class CallbackReturn_NodeStatusStruct;
   friend class NodeRPC;

private:
   std::list<std::tuple<unsigned, uint64_t, uint64_t> > heightTimeVec_;
   ChainStatus state_ = ChainStatus_Unknown;
   float blockSpeed_ = 0.0f;
   uint64_t eta_ = 0;
   float pct_ = 0.0f;
   unsigned blocksLeft_ = 0;

   unsigned prev_pct_int_ = 0;

private:
   bool processState(std::shared_ptr<JSON_object> const getblockchaininfo_obj);

public:
   void appendHeightAndTime(unsigned, uint64_t);
   unsigned getTopBlock(void) const;
   ChainStatus state(void) const { return state_; }
   float getBlockSpeed(void) const { return blockSpeed_; }

   void reset();

   float getProgressPct(void) const { return pct_; }
   uint64_t getETA(void) const { return eta_; }
   unsigned getBlocksLeft(void) const { return blocksLeft_; }
};

////////////////////////////////////////////////////////////////////////////////
struct NodeStatusStruct
{
   NodeStatus status_ = NodeStatus_Offline;
   bool SegWitEnabled_ = false;
   RpcStatus rpcStatus_ = RpcStatus_Disabled;
   ::NodeChainState chainState_;
};

////////////////////////////////////////////////////////////////////////////////
class NodeRPC : protected Lockable
{
private:
   const BlockDataManagerConfig& bdmConfig_;
   std::string basicAuthString64_;

   ::NodeChainState nodeChainState_;
   std::function<void(void)> nodeStatusLambda_;

   RpcStatus previousState_ = RpcStatus_Disabled;
   std::condition_variable pollCondVar_;

   typedef std::map<std::string, std::map<unsigned, FeeEstimateResult>> EstimateCache;
   std::shared_ptr<EstimateCache> currentEstimateCache_ = nullptr;

   std::vector<std::thread> thrVec_;
   std::atomic<bool> run_ = { true };

private:
   std::string getAuthString(void);
   std::string getDatadir(void);

   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}
   void callback(void)
   {
      if (nodeStatusLambda_)
         nodeStatusLambda_();
   }

   std::string queryRPC(JSON_object&);
   std::string queryRPC(HttpSocket&, JSON_object&);
   void pollThread(void);
   
   float queryFeeByte(HttpSocket&, unsigned);
   FeeEstimateResult queryFeeByteSmart(HttpSocket&,
      unsigned& confTarget, std::string& strategy);
   void aggregateFeeEstimates(void);
   void resetAuthString(void);

public:
   NodeRPC(BlockDataManagerConfig&);
   ~NodeRPC(void);
   
   RpcStatus testConnection();
   bool setupConnection(HttpSocket&);
   void shutdown(void);

   bool updateChainStatus(void);
   const ::NodeChainState& getChainStatus(void) const;   
   void waitOnChainSync(std::function<void(void)>);
   std::string broadcastTx(const BinaryDataRef&);

   void registerNodeStatusLambda(std::function<void(void)> lbd) { nodeStatusLambda_ = lbd; }

   virtual bool canPool(void) const { return true; }
   FeeEstimateResult getFeeByte(unsigned confTarget, const std::string& strategy);
   std::map<unsigned, FeeEstimateResult> getFeeSchedule(const std::string& strategy);
};

////////////////////////////////////////////////////////////////////////////////
class NodeRPC_UnitTest : public NodeRPC
{
public:

   NodeRPC_UnitTest(BlockDataManagerConfig& bdmc) :
      NodeRPC(bdmc)
   {}

   bool canPool(void) const { return false; }
};

#endif
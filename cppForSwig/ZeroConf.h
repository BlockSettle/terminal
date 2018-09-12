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

#ifndef _ZEROCONF_H_
#define _ZEROCONF_H_

#include <vector>
#include <atomic>
#include <functional>
#include <memory>

#include "ThreadSafeClasses.h"
#include "BitcoinP2p.h"
#include "lmdb_wrapper.h"
#include "Blockchain.h"
#include "ScrAddrFilter.h"

#define GETZC_THREADCOUNT 5
#define TXGETDATA_TIMEOUT_MS 3000

enum ZcAction
{
   Zc_NewTx,
   Zc_Purge,
   Zc_Shutdown
};

enum ParsedTxStatus
{
   Tx_Uninitialized,
   Tx_Resolved,
   Tx_ResolveAgain,
   Tx_Unresolved,
   Tx_Mined,
   Tx_Invalid
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfData
{
   Tx            txobj_;
   uint32_t      txtime_;

   bool operator==(const ZeroConfData& rhs) const
   {
      return (this->txobj_ == rhs.txobj_) && (this->txtime_ == rhs.txtime_);
   }
};

////////////////////////////////////////////////////////////////////////////////
class OutPointRef
{
private:
   BinaryData txHash_;
   unsigned txOutIndex_ = UINT16_MAX;
   BinaryData dbKey_;
   uint64_t time_ = UINT64_MAX;

public:
   void unserialize(uint8_t const * ptr, uint32_t remaining);
   void unserialize(BinaryDataRef bdr);

   void resolveDbKey(LMDBBlockDatabase* db);
   const BinaryData& getDbKey(void) const { return dbKey_; }

   bool isResolved(void) const { return dbKey_.getSize() == 8; }
   bool isInitialized(void) const;

   BinaryDataRef getTxHashRef(void) const { return txHash_.getRef(); }
   unsigned getIndex(void) const { return txOutIndex_; }

   BinaryData& getDbKey(void) { return dbKey_; }
   BinaryDataRef getDbTxKeyRef(void) const;

   void reset(void)
   {
      dbKey_.clear();
      time_ = UINT64_MAX;
   }

   bool isZc(void) const;

   void setTime(uint64_t t) { time_ = t; }
   uint64_t getTime(void) const { return time_; }
};

////////////////////////////////////////////////////////////////////////////////
struct ParsedTxIn
{
   OutPointRef opRef_;
   BinaryData scrAddr_;
   uint64_t value_ = UINT64_MAX;

public:
   bool isResolved(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct ParsedTxOut
{
   BinaryData scrAddr_;
   uint64_t value_ = UINT64_MAX;

   bool isInitialized(void) const
   {
      return scrAddr_.getSize() != 0 && value_ != UINT64_MAX; \
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ParsedTx
{
private:
   mutable BinaryData txHash_;
   const BinaryData zcKey_;

public:
   Tx tx_;
   vector<ParsedTxIn> inputs_;
   vector<ParsedTxOut> outputs_;
   ParsedTxStatus state_ = Tx_Uninitialized;
   bool isRBF_ = false;
   bool isChainedZc_ = false;


public:
   ParsedTx(BinaryData& key) :
      zcKey_(move(key))
   {}

   ParsedTxStatus status(void) const { return state_; }
   bool isResolved(void) const;
   void reset(void);

   const BinaryData& getTxHash(void) const;
   BinaryDataRef getKeyRef(void) const { return zcKey_.getRef(); }
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfBatch
{
   map<BinaryDataRef, shared_ptr<ParsedTx>> txMap_;
   atomic<unsigned> counter_;
   promise<bool> isReadyPromise_;
   unsigned timeout_ = TXGETDATA_TIMEOUT_MS;

public:
   ZeroConfBatch(void)
   {
      counter_.store(0, memory_order_relaxed);
   }

   void incrementCounter(void)
   {
      auto val = counter_.fetch_add(1, memory_order_relaxed);
      if (val + 1 == txMap_.size())
         isReadyPromise_.set_value(true);
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfInvPacket
{
   BinaryData zcKey_;
   shared_ptr<ZeroConfBatch> batchPtr_;
   InvEntry invEntry_;
};

////////////////////////////////////////////////////////////////////////////////
struct ParsedZCData
{
   set<BinaryData> txioKeys_;
   set<BinaryData> invalidatedKeys_;

   void mergeTxios(ParsedZCData& pzd)
   {
      txioKeys_.insert(pzd.txioKeys_.begin(), pzd.txioKeys_.end());
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ZcPurgePacket
{
   set<BinaryData> invalidatedZcKeys_;
   map<BinaryData, BinaryData> minedTxioKeys_;
};

////////////////////////////////////////////////////////////////////////////////
struct ZcUpdateBatch
{
private:
   unique_ptr<promise<bool>> completed_;

public:
   map<BinaryDataRef, shared_ptr<ParsedTx>> zcToWrite_;
   set<BinaryData> txHashes_;
   set<BinaryData> keysToDelete_;
   set<BinaryData> txHashesToDelete_;

   shared_future<bool> getCompletedFuture(void);
   void setCompleted(bool);
   bool hasData(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfSharedStateSnapshot
{
   map<BinaryDataRef, BinaryDataRef> txHashToDBKey_; //<txHash, dbKey>
   map<BinaryDataRef, shared_ptr<ParsedTx>> txMap_; //<zcKey, zcTx>
   set<BinaryData> txOutsSpentByZC_; //<txOutDbKeys>

   //TODO: rethink this map, slow to purge
   //<scrAddr,  <dbKeyOfOutput, TxIOPair>> 
   map<BinaryData, shared_ptr<map<BinaryData, shared_ptr<TxIOPair>>>>  txioMap_;

   static shared_ptr<ZeroConfSharedStateSnapshot> copy(
      shared_ptr<ZeroConfSharedStateSnapshot> obj)
   {
      auto ss = make_shared<ZeroConfSharedStateSnapshot>();
      if (obj != nullptr)
      {
         ss->txHashToDBKey_ = obj->txHashToDBKey_;
         ss->txMap_ = obj->txMap_;
         ss->txOutsSpentByZC_ = obj->txOutsSpentByZC_;
         ss->txioMap_ = obj->txioMap_;
      }

      return ss;
   }
};

class ZeroConfCallbacks;

////////////////////////////////////////////////////////////////////////////////
class ZeroConfContainer
{
private:
   struct BulkFilterData
   {
      map<BinaryData, shared_ptr<map<BinaryData, shared_ptr<TxIOPair>>>> 
         scrAddrTxioMap_;
      map<BinaryDataRef, map<unsigned, BinaryDataRef>> outPointsSpentByKey_;
      set<BinaryData> txOutsSpentByZC_;
      map<BinaryDataRef, shared_ptr<set<BinaryDataRef>>> keyToSpentScrAddr_;
      map<BinaryDataRef, set<BinaryDataRef>> keyToFundedScrAddr_;

      map<string, ParsedZCData> flaggedBDVs_;

      bool isEmpty(void) { return scrAddrTxioMap_.size() == 0; }
   };

public:
   struct NotificationPacket
   {
      string bdvID_;
      map<BinaryData, shared_ptr<map<BinaryData, shared_ptr<TxIOPair>>>> txioMap_;
      shared_ptr<ZcPurgePacket> purgePacket_;
      shared_ptr<map<BinaryData, shared_ptr<set<BinaryDataRef>>>>
         newKeysAndScrAddr_;
      shared_ptr<map<BinaryDataRef, shared_ptr<ParsedTx>>> txMap_;

      NotificationPacket(const string& bdvID) :
         bdvID_(bdvID)
      {}
   };

   struct ZcActionStruct
   {
      ZcAction action_;
      shared_ptr<ZeroConfBatch> batch_;
      unique_ptr<promise<shared_ptr<ZcPurgePacket>>> resultPromise_ = nullptr;
      Blockchain::ReorganizationState reorgState_;
   };

private:
   shared_ptr<ZeroConfSharedStateSnapshot> snapshot_;
   
   //<txHash, map<opId, ZcKeys>>
   map<BinaryDataRef, map<unsigned, BinaryDataRef>> outPointsSpentByKey_;

   //<zcKey, set<ScrAddr>>
   map<BinaryDataRef, shared_ptr<set<BinaryDataRef>>> keyToSpentScrAddr_;
   
   set<BinaryData> allZcTxHashes_;
   map<BinaryDataRef, set<BinaryDataRef>> keyToFundedScrAddr_;

   std::atomic<uint32_t> topId_;
   LMDBBlockDatabase* db_;

   set<BinaryData> emptySetBinData_;

   //stacks inv tx packets from node
   shared_ptr<BitcoinP2P> networkNode_;
   BlockingQueue<ZeroConfInvPacket> newInvTxStack_;

   mutex parserMutex_;
   mutex parserThreadMutex_;

   vector<thread> parserThreads_;
   atomic<bool> zcEnabled_;
   const unsigned maxZcThreadCount_;

   shared_ptr<TransactionalMap<BinaryDataRef, shared_ptr<AddrAndHash>>> scrAddrMap_;

   unsigned parserThreadCount_ = 0;
   unique_ptr<ZeroConfCallbacks> bdvCallbacks_;
   BlockingQueue<ZcUpdateBatch> updateBatch_;

private:
   BulkFilterData ZCisMineBulkFilter(ParsedTx & tx, const BinaryDataRef& ZCkey,
      function<bool(const BinaryData&, BinaryData&)> getzckeyfortxhash,
      function<const ParsedTx&(const BinaryData&)> getzctxbykey);

   void preprocessTx(ParsedTx&) const;

   void loadZeroConfMempool(bool clearMempool);
   bool purge(
      const Blockchain::ReorganizationState&, 
      shared_ptr<ZeroConfSharedStateSnapshot>,
      map<BinaryData, BinaryData>&);
   void reset(void);

   void processInvTxThread(void);
   void processInvTxThread(ZeroConfInvPacket&);

   void increaseParserThreadPool(unsigned);
   void preprocessZcMap(map<BinaryDataRef, shared_ptr<ParsedTx>>&);

public:
   //stacks new zc Tx objects from node
   BinaryData getNewZCkey(void);
   BlockingQueue<ZcActionStruct> newZcStack_;

public:
   ZeroConfContainer(LMDBBlockDatabase* db,
      shared_ptr<BitcoinP2P> node, unsigned maxZcThread) :
      topId_(0), db_(db), maxZcThreadCount_(maxZcThread), networkNode_(node)
   {
      zcEnabled_.store(false, memory_order_relaxed);

      //register ZC callback
      auto processInvTx = [this](vector<InvEntry> entryVec)->void
      {
         this->processInvTxVec(entryVec, true);
      };

      networkNode_->registerInvTxLambda(processInvTx);
   }

   bool hasTxByHash(const BinaryData& txHash) const;
   Tx getTxByHash(const BinaryData& txHash) const;

   void dropZC(shared_ptr<ZeroConfSharedStateSnapshot>, const BinaryDataRef&);
   void dropZC(shared_ptr<ZeroConfSharedStateSnapshot>, const set<BinaryData>&);

   void parseNewZC(void);
   void parseNewZC(
      map<BinaryDataRef, shared_ptr<ParsedTx>> zcMap, 
      shared_ptr<ZeroConfSharedStateSnapshot>,
      bool updateDB, bool notify);
   bool isTxOutSpentByZC(const BinaryData& dbKey) const;

   void clear(void);

   map<BinaryData, shared_ptr<TxIOPair>> 
      getUnspentZCforScrAddr(BinaryData scrAddr) const;
   map<BinaryData, shared_ptr<TxIOPair>> 
      getRBFTxIOsforScrAddr(BinaryData scrAddr) const;

   vector<TxOut> getZcTxOutsForKey(const set<BinaryData>&) const;

   void updateZCinDB(void);

   void processInvTxVec(vector<InvEntry>, bool extend,
      unsigned timeout = TXGETDATA_TIMEOUT_MS);

   void init(shared_ptr<ScrAddrFilter>, bool clearMempool);
   void shutdown();

   void setZeroConfCallbacks(unique_ptr<ZeroConfCallbacks> ptr)
   {
      bdvCallbacks_ = move(ptr);
   }

   void broadcastZC(const BinaryData& rawzc,
      const string& bdvId, uint32_t timeout_ms);

   bool isEnabled(void) const { return zcEnabled_.load(memory_order_relaxed); }
   void pushZcToParser(const BinaryDataRef& rawTx);

   shared_ptr<map<BinaryData, shared_ptr<TxIOPair>> >
      getTxioMapForScrAddr(const BinaryData&) const;

   shared_ptr<ZeroConfSharedStateSnapshot> getSnapshot(void) const
   {
      auto ss = atomic_load_explicit(&snapshot_, memory_order_acquire);
      return ss;
   }

   shared_ptr<ParsedTx> getTxByKey(const BinaryData&) const;
   TxOut getTxOutCopy(const BinaryDataRef, unsigned) const;
   BinaryDataRef getKeyForHash(const BinaryData&) const;
   BinaryDataRef getHashForKey(const BinaryData&) const;
};

////////////////////////////////////////////////////////////////////////////////
class ZeroConfCallbacks
{
public:
   virtual ~ZeroConfCallbacks(void) = 0;

   virtual set<string> hasScrAddr(const BinaryDataRef&) const = 0;
   virtual void pushZcNotification(
      ZeroConfContainer::NotificationPacket& packet) = 0;
   virtual void errorCallback(
      const string& bdvId, string& errorStr, const string& txHash) = 0;
};

#endif
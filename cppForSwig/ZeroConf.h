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
   std::vector<ParsedTxIn> inputs_;
   std::vector<ParsedTxOut> outputs_;
   ParsedTxStatus state_ = Tx_Uninitialized;
   bool isRBF_ = false;
   bool isChainedZc_ = false;
   bool needsReparsed_ = false;

public:
   ParsedTx(BinaryData& key) :
      zcKey_(std::move(key))
   {
      //set zc index in Tx object
      BinaryRefReader brr(zcKey_.getRef());
      brr.advance(2);
      tx_.setTxIndex(brr.get_uint32_t(BE));
   }

   ParsedTxStatus status(void) const { return state_; }
   bool isResolved(void) const;
   void reset(void);

   const BinaryData& getTxHash(void) const;
   BinaryDataRef getKeyRef(void) const { return zcKey_.getRef(); }
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfBatch
{
   std::map<BinaryDataRef, std::shared_ptr<ParsedTx>> txMap_;
   std::atomic<unsigned> counter_;
   std::promise<bool> isReadyPromise_;
   unsigned timeout_ = TXGETDATA_TIMEOUT_MS;

public:
   ZeroConfBatch(void)
   {
      counter_.store(0, std::memory_order_relaxed);
   }

   void incrementCounter(void)
   {
      auto val = counter_.fetch_add(1, std::memory_order_relaxed);
      if (val + 1 == txMap_.size())
         isReadyPromise_.set_value(true);
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfInvPacket
{
   BinaryData zcKey_;
   std::shared_ptr<ZeroConfBatch> batchPtr_;
   InvEntry invEntry_;
};

////////////////////////////////////////////////////////////////////////////////
struct ParsedZCData
{
   std::set<BinaryData> txioKeys_;
   std::map<BinaryData, BinaryData> invalidatedKeys_;

   void mergeTxios(ParsedZCData& pzd)
   {
      txioKeys_.insert(pzd.txioKeys_.begin(), pzd.txioKeys_.end());
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ZcPurgePacket
{
   std::map<BinaryData, BinaryData> invalidatedZcKeys_;
   std::map<BinaryData, BinaryData> minedTxioKeys_;
};

////////////////////////////////////////////////////////////////////////////////
struct ZcUpdateBatch
{
private:
   std::unique_ptr<std::promise<bool>> completed_;

public:
   std::map<BinaryDataRef, std::shared_ptr<ParsedTx>> zcToWrite_;
   std::set<BinaryData> txHashes_;
   std::set<BinaryData> keysToDelete_;
   std::set<BinaryData> txHashesToDelete_;

   std::shared_future<bool> getCompletedFuture(void);
   void setCompleted(bool);
   bool hasData(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct ZeroConfSharedStateSnapshot
{
   std::map<BinaryDataRef, BinaryDataRef> txHashToDBKey_; //<txHash, dbKey>
   std::map<BinaryDataRef, std::shared_ptr<ParsedTx>> txMap_; //<zcKey, zcTx>
   std::set<BinaryData> txOutsSpentByZC_; //<txOutDbKeys>

   //TODO: rethink this map, slow to purge
   //<scrAddr,  <dbKeyOfOutput, TxIOPair>> 
   std::map<BinaryData, std::shared_ptr<std::map<BinaryData, std::shared_ptr<TxIOPair>>>>  txioMap_;

   static std::shared_ptr<ZeroConfSharedStateSnapshot> copy(
      std::shared_ptr<ZeroConfSharedStateSnapshot> obj)
   {
      auto ss = std::make_shared<ZeroConfSharedStateSnapshot>();
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
      std::map<BinaryData, std::shared_ptr<std::map<BinaryData, std::shared_ptr<TxIOPair>>>>
         scrAddrTxioMap_;
      std::map<BinaryDataRef, std::map<unsigned, BinaryDataRef>> outPointsSpentByKey_;
      std::set<BinaryData> txOutsSpentByZC_;
      std::map<BinaryDataRef, std::shared_ptr<std::set<BinaryDataRef>>> keyToSpentScrAddr_;
      std::map<BinaryDataRef, std::set<BinaryDataRef>> keyToFundedScrAddr_;

      std::map<std::string, ParsedZCData> flaggedBDVs_;

      bool isEmpty(void) { return scrAddrTxioMap_.size() == 0; }
   };

public:
   struct NotificationPacket
   {
      std::string bdvID_;
      std::map<BinaryData, std::shared_ptr<std::map<BinaryData, std::shared_ptr<TxIOPair>>>> txioMap_;
      std::shared_ptr<ZcPurgePacket> purgePacket_;
      std::shared_ptr<std::map<BinaryData, std::shared_ptr<std::set<BinaryDataRef>>>>
         newKeysAndScrAddr_;
      std::shared_ptr<std::map<BinaryDataRef, std::shared_ptr<ParsedTx>>> txMap_;

      NotificationPacket(const std::string& bdvID) :
         bdvID_(bdvID)
      {}
   };

   struct ZcActionStruct
   {
      ZcAction action_;
      std::shared_ptr<ZeroConfBatch> batch_;
      std::unique_ptr<std::promise<std::shared_ptr<ZcPurgePacket>>> resultPromise_ = nullptr;
      Blockchain::ReorganizationState reorgState_;
   };

private:
   std::shared_ptr<ZeroConfSharedStateSnapshot> snapshot_;
   
   //<txHash, map<opId, ZcKeys>>
   std::map<BinaryDataRef, std::map<unsigned, BinaryDataRef>> outPointsSpentByKey_;

   //<zcKey, set<ScrAddr>>
   std::map<BinaryDataRef, std::shared_ptr<std::set<BinaryDataRef>>> keyToSpentScrAddr_;
   
   std::set<BinaryData> allZcTxHashes_;
   std::map<BinaryDataRef, std::set<BinaryDataRef>> keyToFundedScrAddr_;

   std::atomic<uint32_t> topId_;
   LMDBBlockDatabase* db_;

   std::set<BinaryData> emptySetBinData_;

   //stacks inv tx packets from node
   std::shared_ptr<BitcoinP2P> networkNode_;
   BlockingQueue<ZeroConfInvPacket> newInvTxStack_;

   std::mutex parserMutex_;
   std::mutex parserThreadMutex_;

   std::vector<std::thread> parserThreads_;
   std::atomic<bool> zcEnabled_;
   const unsigned maxZcThreadCount_;

   std::shared_ptr<TransactionalMap<BinaryDataRef, std::shared_ptr<AddrAndHash>>> scrAddrMap_;

   unsigned parserThreadCount_ = 0;
   std::unique_ptr<ZeroConfCallbacks> bdvCallbacks_;
   BlockingQueue<ZcUpdateBatch> updateBatch_;

private:
   BulkFilterData ZCisMineBulkFilter(ParsedTx & tx, const BinaryDataRef& ZCkey,
      std::function<bool(const BinaryData&, BinaryData&)> getzckeyfortxhash,
      std::function<const ParsedTx&(const BinaryData&)> getzctxbykey);

   void preprocessTx(ParsedTx&) const;

   void loadZeroConfMempool(bool clearMempool);
   bool purge(
      const Blockchain::ReorganizationState&, 
      std::shared_ptr<ZeroConfSharedStateSnapshot>,
      std::map<BinaryData, BinaryData>&);
   void reset(void);

   void processInvTxThread(void);
   void processInvTxThread(ZeroConfInvPacket&);

   void increaseParserThreadPool(unsigned);
   void preprocessZcMap(std::map<BinaryDataRef, std::shared_ptr<ParsedTx>>&);

public:
   //stacks new zc Tx objects from node
   BinaryData getNewZCkey(void);
   BlockingQueue<ZcActionStruct> newZcStack_;

public:
   ZeroConfContainer(LMDBBlockDatabase* db,
      std::shared_ptr<BitcoinP2P> node, unsigned maxZcThread) :
      topId_(0), db_(db), networkNode_(node), maxZcThreadCount_(maxZcThread)
   {
      zcEnabled_.store(false, std::memory_order_relaxed);

      //register ZC callback
      auto processInvTx = [this](std::vector<InvEntry> entryVec)->void
      {
         this->processInvTxVec(entryVec, true);
      };

      networkNode_->registerInvTxLambda(processInvTx);
   }

   bool hasTxByHash(const BinaryData& txHash) const;
   Tx getTxByHash(const BinaryData& txHash) const;

   void dropZC(std::shared_ptr<ZeroConfSharedStateSnapshot>, const BinaryDataRef&);
   void dropZC(std::shared_ptr<ZeroConfSharedStateSnapshot>, const std::set<BinaryData>&);

   void parseNewZC(void);
   void parseNewZC(
      std::map<BinaryDataRef, std::shared_ptr<ParsedTx>> zcMap,
      std::shared_ptr<ZeroConfSharedStateSnapshot>,
      bool updateDB, bool notify);
   bool isTxOutSpentByZC(const BinaryData& dbKey) const;

   void clear(void);

   std::map<BinaryData, std::shared_ptr<TxIOPair>>
      getUnspentZCforScrAddr(BinaryData scrAddr) const;
   std::map<BinaryData, std::shared_ptr<TxIOPair>>
      getRBFTxIOsforScrAddr(BinaryData scrAddr) const;

   std::vector<TxOut> getZcTxOutsForKey(const std::set<BinaryData>&) const;
   std::vector<UnspentTxOut> getZcUTXOsForKey(const std::set<BinaryData>&) const;

   void updateZCinDB(void);

   void processInvTxVec(std::vector<InvEntry>, bool extend,
      unsigned timeout = TXGETDATA_TIMEOUT_MS);

   void init(std::shared_ptr<ScrAddrFilter>, bool clearMempool);
   void shutdown();

   void setZeroConfCallbacks(std::unique_ptr<ZeroConfCallbacks> ptr)
   {
      bdvCallbacks_ = std::move(ptr);
   }

   void broadcastZC(const BinaryData& rawzc,
      const std::string& bdvId, uint32_t timeout_ms);

   bool isEnabled(void) const { return zcEnabled_.load(std::memory_order_relaxed); }
   void pushZcToParser(const BinaryDataRef& rawTx);

   std::shared_ptr<std::map<BinaryData, std::shared_ptr<TxIOPair>> >
      getTxioMapForScrAddr(const BinaryData&) const;

   std::shared_ptr<ZeroConfSharedStateSnapshot> getSnapshot(void) const
   {
      auto ss = std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
      return ss;
   }

   std::shared_ptr<ParsedTx> getTxByKey(const BinaryData&) const;
   TxOut getTxOutCopy(const BinaryDataRef, unsigned) const;
   BinaryDataRef getKeyForHash(const BinaryDataRef&) const;
   BinaryDataRef getHashForKey(const BinaryDataRef&) const;
};

////////////////////////////////////////////////////////////////////////////////
class ZeroConfCallbacks
{
public:
   virtual ~ZeroConfCallbacks(void) = 0;

   virtual std::set<std::string> hasScrAddr(const BinaryDataRef&) const = 0;
   virtual void pushZcNotification(
      ZeroConfContainer::NotificationPacket& packet) = 0;
   virtual void errorCallback(
      const std::string& bdvId, std::string& errorStr, const std::string& txHash) = 0;
};

#endif
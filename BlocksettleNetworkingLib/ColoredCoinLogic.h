/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef _H_COLOREDCOINLOGIC
#define _H_COLOREDCOINLOGIC

#include <vector>
#include <set>
#include <map>
#include <string>

#include "Address.h"
#include "ArmoryConnection.h"

////
class ColoredCoinException : public std::runtime_error
{
public:
   ColoredCoinException(const std::string& err) :
      std::runtime_error(err)
   {}
};

////
struct ParsedCcTx
{
   BinaryData txHash_;

   std::vector<std::pair<BinaryData, unsigned>> outpoints_;
   std::vector<std::pair<uint64_t, BinaryData>> outputs_;

   bool isInitialized(void) const { return txHash_.getSize() == 32; }
};

////
struct CcOutpoint
{
private:
   const uint64_t value_;
   const unsigned index_;

   std::shared_ptr<BinaryData> txHash_;
   std::shared_ptr<BinaryData> scrAddr_;

public:
   CcOutpoint(uint64_t value, unsigned index) :
      value_(value), index_(index)
   {}

   ////
   void setTxHash(const BinaryData&);
   void setTxHash(const std::shared_ptr<BinaryData>&);
   void setScrAddr(const std::shared_ptr<BinaryData>&);

   ////
   const std::shared_ptr<BinaryData> getTxHash(void) const { return txHash_; }
   const std::shared_ptr<BinaryData> getScrAddr(void) const { return scrAddr_; }

   ////
   uint64_t value(void) const { return value_; }
   unsigned index(void) const { return index_; }
   
   ////
   bool operator<(const CcOutpoint& rhs) const
   {
      if (txHash_ != rhs.txHash_)
         return txHash_ < rhs.txHash_;

      return index_ < rhs.index_;
   }
};

////
struct CcOutpointCompare
{
   bool operator() (
      const std::shared_ptr<CcOutpoint>& lhs,
      const std::shared_ptr<CcOutpoint>& rhs) const
   {
      if (lhs == nullptr || rhs == nullptr)
         throw std::runtime_error("empty outpoints");

      return *lhs < *rhs;
   }
};

////
struct BinaryDataCompare
{
   bool operator() (
      const std::shared_ptr<BinaryData>& lhs,
      const std::shared_ptr<BinaryData>& rhs) const
   {
      return *lhs < *rhs;
   }
};
   
typedef std::set<std::shared_ptr<CcOutpoint>, CcOutpointCompare> OpPtrSet;

////
struct ColoredCoinSnapshot
{
public:
   //<txHash, <txOutId, outpoint>>
   std::map<BinaryData, std::map<unsigned, std::shared_ptr<CcOutpoint>>> utxoSet_;

   //<prefixed scrAddr, <outpoints>>
   std::map<BinaryData, OpPtrSet> scrAddrCcSet_;

   //<prefixed scrAddr, height of revoke tx>
   std::map<BinaryData, unsigned> revokedAddresses_;
};

////
struct ColoredCoinZCSnapshot
{
public:
   //<txHash, <txOutId, outpoint>>
   std::map<BinaryData, std::map<unsigned, std::shared_ptr<CcOutpoint>>> utxoSet_;

   //<prefixed scrAddr, <outpoints>>
   std::map<BinaryData, OpPtrSet> scrAddrCcSet_;

   //<hash, <txOutIds>>
   std::map<BinaryData, std::set<unsigned>> spentOutputs_;
};

////
class ColoredCoinTracker;

////
class ColoredCoinACT : public ArmoryCallbackTarget
{
   friend class ColoredCoinTracker;

private:
   struct RegistrationStruct
   {
      std::string regID_;
      std::shared_ptr<DBNotificationStruct> notifPtr_ = nullptr;

      ////
      bool isValid(void) const { return notifPtr_ != nullptr; }
      void clear(void)
      {
         notifPtr_ = nullptr;
         regID_.clear();
      }

      void set(std::shared_ptr<DBNotificationStruct> ptr, std::string id)
      {
         if (isValid())
            throw ColoredCoinException("registration struct is already set");

         if(id.size() == 0)
            throw ColoredCoinException("empty registration id");

         notifPtr_ = ptr;
         regID_ = id;
      }
   };

private:
   BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;
   std::thread processThr_;

   ColoredCoinTracker* ccPtr_ = nullptr;

private:
   std::shared_ptr<DBNotificationStruct> popNotification(void)
   {
      return notifQueue_.pop_front();
   }

protected:
   virtual void onUpdate(std::shared_ptr<DBNotificationStruct>) {}

public:
   ColoredCoinACT(ArmoryConnection *armory)
      : ArmoryCallbackTarget()
   {
      init(armory);
   }
   ColoredCoinACT() : ArmoryCallbackTarget() {}

   ~ColoredCoinACT() override
   {
      cleanup();
   }

   ////
   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override;
   void onNewBlock(unsigned int, unsigned int) override;
   void onRefresh(const std::vector<BinaryData> &, bool) override;
   void onStateChanged(ArmoryState) override;

   ////
   virtual void start();
   virtual void stop();
   virtual void setCCManager(ColoredCoinTracker* ccPtr) { ccPtr_ = ccPtr; }

private:
   virtual void processNotification(void);
};

////
class ColoredCoinTracker
{
   /*
   This class tracks the UTXO set for a single colored coin.
   */
   
   friend class ColoredCoinACT;
   
private:
   std::set<BinaryData> originAddresses_;
   std::set<BinaryData> revocationAddresses_;

   std::shared_ptr<ArmoryConnection> connPtr_;
   BlockingQueue<BinaryData> refreshQueue_;

   std::shared_ptr<ColoredCoinSnapshot> snapshot_ = nullptr;
   std::shared_ptr<ColoredCoinZCSnapshot> zcSnapshot_ = nullptr;

   unsigned startHeight_ = 0;
   unsigned zcCutOff_ = 0;
   
   uint64_t coinsPerShare_;

   ////
   std::atomic<bool> ready_;

protected:
   std::shared_ptr<AsyncClient::BtcWallet> walletObj_;
   std::shared_ptr<ColoredCoinACT>  actPtr_;

private:
   ////
   std::vector<Tx> grabTxBatch(const std::set<BinaryData>&);

   ParsedCcTx processTx(
      const std::shared_ptr<ColoredCoinSnapshot> &,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const Tx&) const;

   ////
   std::set<BinaryData> processTxBatch(
      std::shared_ptr<ColoredCoinSnapshot>&,
      const std::set<BinaryData>&);

   void processZcBatch(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const std::set<BinaryData>&);

   void processRevocationBatch(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::set<BinaryData>&);

   ////
   void purgeZc(void);

   ////
   void addUtxo(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const BinaryData& txHash, unsigned txOutIndex,
      uint64_t value, const BinaryData& scrAddr);

   void addZcUtxo(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const BinaryData& txHash, unsigned txOutIndex,
      uint64_t value, const BinaryData& scrAddr);

   ////
   const std::shared_ptr<BinaryData> getScrAddrPtr(
      const std::map<BinaryData, OpPtrSet>&,
      const BinaryData&) const;

   ////
   void eraseScrAddrOp(
      const std::shared_ptr<ColoredCoinSnapshot> &,
      const std::shared_ptr<CcOutpoint>&);
   
   void addScrAddrOp(
      std::map<BinaryData, OpPtrSet>&,
      const std::shared_ptr<CcOutpoint>&);

   ////   
   uint64_t getCcOutputValue(
      const std::shared_ptr<ColoredCoinSnapshot> &
      , const std::shared_ptr<ColoredCoinZCSnapshot>&
      , const BinaryData&, unsigned, unsigned) const;

   ////
   std::vector<std::shared_ptr<CcOutpoint>> getSpendableOutpointsForAddress(
      const std::shared_ptr<ColoredCoinSnapshot>&,
      const std::shared_ptr<ColoredCoinZCSnapshot>&,
      const BinaryData&, bool) const;

   std::set<BinaryData> collectOriginAddresses() const;
   std::set<BinaryData> collectRevokeAddresses() const;

   ////
   std::shared_ptr<ColoredCoinSnapshot> snapshot(void) const;
   std::shared_ptr<ColoredCoinZCSnapshot> zcSnapshot(void) const;

   ////
   void waitOnRefresh(const std::string&);
   void pushRefreshID(std::vector<BinaryData>&);

   ////
   void shutdown(void);

protected:
   ////
   std::set<BinaryData> update(void);
   std::set<BinaryData> zcUpdate(void);
   void reorg(bool hard);

public:
   ColoredCoinTracker(uint64_t coinsPerShare,
      std::shared_ptr<ArmoryConnection> connPtr) :
      coinsPerShare_(coinsPerShare), connPtr_(connPtr)
   {
      ready_.store(false, std::memory_order_relaxed);

      auto&& wltIdSbd = CryptoPRNG::generateRandom(12);
      walletObj_ = connPtr_->instantiateWallet(wltIdSbd.toHexStr());
   }

   ~ColoredCoinTracker(void)
   {
      shutdown();
   }

   ////
   void addOriginAddress(const bs::Address&);
   void addRevocationAddress(const bs::Address&);

   ////
   bool goOnline(void);

   uint64_t getCcOutputValue(const BinaryData &txHash
      , unsigned int txOutIndex, unsigned int height) const;

   //in: prefixed address
   uint64_t getCcValueForAddress(const BinaryData&) const;

   //in: prefixed address
   std::vector<std::shared_ptr<CcOutpoint>> getSpendableOutpointsForAddress(
      const BinaryData&) const;

   bool isTxHashValid(const BinaryData &, uint32_t txOutIndex = UINT32_MAX) const;

   //in: set of prefixed addresses
   uint64_t getUnconfirmedCcValueForAddresses(const std::set<BinaryData>&) const;
   uint64_t getConfirmedCcValueForAddresses(const std::set<BinaryData>&) const;

   bool getCCUtxoForAddresses(const std::set<BinaryData>&, bool,
      const std::function<void(std::vector<UTXO>, std::exception_ptr)>&) const;
};

#endif

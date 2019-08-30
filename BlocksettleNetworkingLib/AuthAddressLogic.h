#ifndef _H_AUTHADDRESSLOGIC
#define _H_AUTHADDRESSLOGIC

#include <atomic>
#include <memory>
#include <set>

#include "Address.h"
#include "Wallets/SyncWallet.h"


constexpr unsigned int VALIDATION_CONF_COUNT = 6;


class AuthLogicException : public std::runtime_error
{
public:
   AuthLogicException(const std::string& err) :
      std::runtime_error(err)
   {}
};

enum DBNotificationStruct_Enum
{
   DBNS_Refresh,
   DBNS_ZC,
   DBNS_NewBlock
};

struct DBNotificationStruct
{
   const DBNotificationStruct_Enum type_;

   std::vector<BinaryData> ids_;
   bool online_;

   std::vector<bs::TXEntry> zc_;

   unsigned block_;

   DBNotificationStruct(DBNotificationStruct_Enum type) :
      type_(type)
   {}
};

class ValidationAddressManager;

struct AuthOutpoint
{
   friend class ValidationAddressManager;

private:
   unsigned txOutIndex_ = UINT32_MAX;
   uint64_t value_ = UINT64_MAX;

   //why is it so hard to udpate values in a std::set...
   mutable unsigned txHeight_ = UINT32_MAX;
   mutable unsigned txIndex_ = UINT32_MAX;
   mutable bool isSpent_ = true;
   mutable BinaryData spenderHash_;

private:
   bool operator<(const std::shared_ptr<AuthOutpoint>& rhs) const
   {  /*
      Doesnt work for comparing zc with zc, works with. Works
      with mined vs zc. Only used to setup the first outpoint
      atm.

      ZC outpoints are not eligible for that distinction, so
      there is no need to cover this blind spot (which would
      require more data from the db).
      */

      if (rhs == nullptr) {
         return true;
      }
      if (txHeight_ != rhs->txHeight_) {
         return txHeight_ < rhs->txHeight_;
      }
      if (txIndex_ != rhs->txIndex_) {
         return txIndex_ < rhs->txIndex_;
      }
      return txOutIndex_ < rhs->txOutIndex_;
   }

public:
   AuthOutpoint(
      unsigned txHeight, unsigned txIndex, unsigned txOutIndex,
      uint64_t value, bool isSpent,
      const BinaryData& spenderHash) :
      txHeight_(txHeight), txIndex_(txIndex), txOutIndex_(txOutIndex),
      value_(value), isSpent_(isSpent),
      spenderHash_(spenderHash)
   {}

   AuthOutpoint(void) {}

   ////
   bool isSpent(void) const { return isSpent_; }

   bool isValid(void) const
   {
      return txOutIndex_ != UINT32_MAX;
   }

   bool isZc(void) const
   {
      if (!isValid()) {
         throw std::runtime_error("invalid AuthOutpoint");
      }
      return txHeight_ == UINT32_MAX;
   }

   ////
   unsigned txOutIndex(void) const { return txOutIndex_; }
   unsigned txHeight(void) const { return txHeight_; }

   const BinaryData& spenderHash(void) const
   {
      return spenderHash_;
   }

   ////
   void updateFrom(const AuthOutpoint& rhs)
   {  /*
      You only ever update from previous states, so only do
      so to save a spentness marker
      */

      if (!rhs.isSpent_ || isSpent_) {
         return;
      }
      isSpent_ = rhs.isSpent_;
      txHeight_ = rhs.txHeight_;
      spenderHash_ = rhs.spenderHash_;
      txIndex_ = rhs.txIndex_;
   }
};

////
class ValidationAddressACT : public ArmoryCallbackTarget
{
public:
   ValidationAddressACT(ArmoryConnection *armory)
      : ArmoryCallbackTarget()
   {
      init(armory);
   }
   ~ValidationAddressACT() override
   {
      cleanup();
   }

   ////
   void onRefresh(const std::vector<BinaryData> &, bool) override;
   void onZCReceived(const std::vector<bs::TXEntry> &zcs) override;
   void onNewBlock(unsigned int) override;

   ////
   virtual void start();
   virtual void stop();
   virtual void setAddressMgr(ValidationAddressManager* vamPtr) { vamPtr_ = vamPtr; }

private:
   void processNotification(void);

private:
   BlockingQueue<std::shared_ptr<DBNotificationStruct>> notifQueue_;
   std::thread processThr_;

   ValidationAddressManager* vamPtr_ = nullptr;
};

////
struct ValidationAddressStruct
{
   //<tx hash, outpoint>
   std::map<BinaryData,
      std::map<unsigned, std::shared_ptr<AuthOutpoint>>> outpoints_;

   BinaryData firstOutpointHash_;
   unsigned firstOutpointIndex_ = UINT32_MAX;

   //<tx hash>
   std::set<BinaryDataRef> spenderHashes_;

   ValidationAddressStruct(void)
   {}

   std::shared_ptr<AuthOutpoint> getFirsOutpoint(void) const
   {
      auto opIter = outpoints_.find(firstOutpointHash_);
      if (opIter == outpoints_.end()) {
         return nullptr;
      }
      auto ptrIter = opIter->second.find(firstOutpointIndex_);
      if (ptrIter == opIter->second.end()) {
         return nullptr;
      }
      return ptrIter->second;
   }

   bool isFirstOutpoint(const BinaryData& hash, unsigned index) const
   {
      if (firstOutpointIndex_ == UINT32_MAX ||
         firstOutpointHash_.getSize() != 32) {
         throw std::runtime_error("uninitialized first outpoint");
      }
      return index == firstOutpointIndex_ &&
         hash == firstOutpointHash_;
   }
};

////
class ValidationAddressManager
{  /***
   This class tracks the state of validation addresses, which is
   required to check on the state of a user auth address.

   It uses a blocking model for the purpose of demonstrating the
   features in unit tests.
   ***/

private:
   std::shared_ptr<ArmoryConnection> connPtr_;
   std::shared_ptr<ValidationAddressACT> actPtr_;
   std::shared_ptr<AsyncClient::BtcWallet> walletObj_;
   BlockingQueue<BinaryData> refreshQueue_;

   std::map<bs::Address, std::shared_ptr<ValidationAddressStruct>> validationAddresses_;
   unsigned topBlock_ = 0;
   unsigned zcIndex_ = 0;

   std::atomic<bool> ready_;
   mutable std::mutex vettingMutex_;

private:
   std::shared_ptr<ValidationAddressStruct>
      getValidationAddress(const bs::Address&);

   UTXO getVettingUtxo(const bs::Address &validationAddr
      , const std::vector<UTXO> &) const;

   const std::shared_ptr<ValidationAddressStruct>
      getValidationAddress(const bs::Address&) const;

   void waitOnRefresh(const std::string&);

public:
   ValidationAddressManager(std::shared_ptr<ArmoryConnection>);
   ~ValidationAddressManager(void)
   {  //unregister wallet
      //shutdown act
      if (actPtr_ != nullptr) {
         actPtr_->stop();
      }
   }

   void addValidationAddress(const bs::Address &);

   void setCustomACT(const std::shared_ptr<ValidationAddressACT> &);

   /*
   These methods return the amount of outpoints received. It
   allows for coverage of the db data flow.
   */
   unsigned goOnline(void);
   unsigned update(void);

   //utility methods
   std::shared_ptr<ArmoryConnection> connPtr(void) const { return connPtr_; }
   void pushRefreshID(std::vector<BinaryData>&);

   //validation address logic
   bool isValid(const bs::Address&) const;
   bool hasSpendableOutputs(const bs::Address&) const;
   bool hasZCOutputs(const bs::Address&) const;

   bool getOutpointBatch(const bs::Address &, const std::function<void(const OutpointBatch &)> &) const;
   bool getSpendableTxOutList(const std::function<void(const std::vector<UTXO> &)> &) const;

   const bs::Address& findValidationAddressForUTXO(const UTXO&) const;
   const bs::Address& findValidationAddressForTxHash(const BinaryData&) const;

   //tx generating methods
   BinaryData fundUserAddress(const bs::Address&, std::shared_ptr<ResolverFeed>,
      const bs::Address& validationAddr = BinaryData()) const;
   BinaryData fundUserAddress(const bs::Address&, std::shared_ptr<ResolverFeed>,
      const UTXO &) const;
   BinaryData vetUserAddress(const bs::Address&, std::shared_ptr<ResolverFeed>,
      const bs::Address& validationAddr = BinaryData()) const;
   BinaryData revokeValidationAddress(
      const bs::Address&, std::shared_ptr<ResolverFeed>) const;
   BinaryData revokeUserAddress(
      const bs::Address&, std::shared_ptr<ResolverFeed>);
};

////////////////////////////////////////////////////////////////////////////////
struct AuthAddressLogic
{
   static bool isValid(const ValidationAddressManager&, const bs::Address&);
   static std::vector<OutpointData> getValidPaths(
      const ValidationAddressManager&, const bs::Address&);
   static BinaryData revoke(const ValidationAddressManager&, const bs::Address&,
      std::shared_ptr<ResolverFeed>);
};

#endif

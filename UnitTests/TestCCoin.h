#ifndef __TEST_CCOIN_H__
#define __TEST_CCOIN_H__

#include <atomic>
#include <memory>
#include <set>
#include <gtest/gtest.h>

#include "Address.h"
#include "BlockchainMonitor.h"
#include "TestEnv.h"
#include "ColoredCoinLogic.h"

namespace bs {
   namespace core {
      class Wallet;
      namespace hd {
         class Wallet;
         class Leaf;
      }
      class WalletsManager;
      class SettlementAddressEntry;
   }
   namespace sync {
      class Wallet;
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class WalletsManager;
   }
}

struct CCoinSpender
{
   bs::Address ccAddr_;
   bs::Address xbtAddr_;

   uint64_t ccValue_ = 0;
   uint64_t xbtValue_ = 0;
};

class ColoredCoinTestACT : public ColoredCoinACT
{
private:
   BlockingQueue<std::vector<BinaryData>> refreshQueue_;

public:
   ColoredCoinTestACT(ArmoryConnection *armory)
      : ColoredCoinACT(armory)
   {}

   virtual void onZCReceived(const std::vector<bs::TXEntry> &zcs) override {}
   virtual void onNewBlock(unsigned, unsigned) override {}
   virtual void onRefresh(const std::vector<BinaryData>& ids, bool online) override
   {
      ColoredCoinACT::onRefresh(ids, online);
      
      auto idsCopy = ids;
      refreshQueue_.push_back(std::move(idsCopy));
   }

   std::vector<BinaryData> popRefreshNotif(void)
   {
      return refreshQueue_.pop_front();
   }
};

class ColoredCoinTestACT_WithNotif : public ColoredCoinACT
{
private:
   BlockingQueue<std::shared_ptr<DBNotificationStruct>> updateQueue_;

public:
   ColoredCoinTestACT_WithNotif(ArmoryConnection *armory)
      : ColoredCoinACT(armory)
   {}

   void onUpdate(std::shared_ptr<DBNotificationStruct> notifPtr) override
   {
      updateQueue_.push_back(std::move(notifPtr));
   }

   void purgeUpdates(void)
   {
      updateQueue_.clear();
   }

   void waitOnNotif(DBNotificationStruct_Enum notif)
   {
      while (true)
      {
         try
         {
            auto notifPtr = updateQueue_.pop_front();
            if (notifPtr->type_ == notif)
               return;
         }
         catch (StopBlockingLoop&)
         {
            return;
         }
      }
   }
};

class ColoredCoinTracker_UT : protected ColoredCoinTracker
{
private:
   void registerAddresses(std::set<BinaryData>&);

public:
   void setACT(std::shared_ptr<ColoredCoinACT> actPtr)
   {
      actPtr_ = std::move(actPtr);
   }

   void update_UT(void);
   void zcUpdate_UT(void);
   void reorg_UT(void);
};

class AsyncCCT : public ColoredCoinTracker
{
public:
   AsyncCCT(uint64_t coinsPerShare, std::shared_ptr<ArmoryConnection> connPtr)
      : ColoredCoinTracker(coinsPerShare, connPtr) {}

   std::string registerAddresses(const std::vector<BinaryData> &addrs, bool isNew) const
   {
      return walletObj_->registerAddresses(addrs, isNew);
   }

   std::string registerAddresses(const std::set<BinaryData> &addrSet, bool isNew) const
   {
      std::vector<BinaryData> addrVec;
      for (const auto &addr : addrSet) {
         addrVec.emplace_back(addr);
      }
      return registerAddresses(addrVec, isNew);
   }

   void update(const AddrSetCb &cb) { ColoredCoinTracker::update(cb); }
   void zcUpdate(const AddrSetCb &cb) { ColoredCoinTracker::zcUpdate(cb); }
   void reorg() { ColoredCoinTracker::reorg(true); }
};

class TestCCoin : public ::testing::Test
{
public:
   using UTXOs = std::vector<UTXO>;

   const uint64_t ccLotSize_ = 307;
   const size_t usersCount_ = 10;


   std::shared_ptr<bs::core::hd::Leaf>   rootSignWallet_;
   std::shared_ptr<bs::sync::hd::Leaf>   rootWallet_;
   std::vector<std::shared_ptr<bs::core::hd::Leaf>>   userSignWallets_;
   std::vector<std::shared_ptr<bs::sync::hd::Leaf>>   userWallets_;

   std::shared_ptr<bs::sync::WalletsManager> syncMgr_;
   std::vector<UnitTestLocalACT*> localACTs_;

   bs::Address              genesisAddr_;
   bs::Address              revocationAddr_;
   std::vector<bs::Address> userCCAddresses_;
   std::vector<bs::Address> userFundAddresses_;

   std::shared_ptr<TestEnv> envPtr_;

   // coinbase & mining related
   SecureBinaryData coinbasePrivKey_ =
      READHEX("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
   BinaryData coinbasePubKey_;
   BinaryData coinbaseScrAddr_;
   BinaryData coinbaseUtxoScript_;
   std::shared_ptr<ResolverOneAddress> coinbaseFeed_;

   typedef std::map<unsigned, BinaryData> TCoinbaseHashes;
   TCoinbaseHashes coinbaseHashes_;
   unsigned coinbaseLast_ = std::numeric_limits<unsigned>::max();

   SecureBinaryData passphrase_;

public:
   explicit TestCCoin();

   void SetUp() override;
   void TearDown() override;

   void MineBlocks(unsigned, bool wait = true);
   void setReorgBranchPoint(const BinaryData&);
   BinaryData getCurrentTopBlockHash(void) const;

   void UpdateBalances(std::shared_ptr<bs::sync::hd::Leaf> wallet);
   void UpdateAllBalances();
   void InitialFund();
   std::vector<UTXO> GetUTXOsFor(const bs::Address & addr, bool sortedByValue = true);

   BinaryData FundFromCoinbase(const std::vector<bs::Address> & addresses, const uint64_t & valuePerOne);
   BinaryData SimpleSendMany(const bs::Address & fromAddress, const std::vector<bs::Address> & toAddresses, const uint64_t & valuePerOne);

   Tx CreateCJtx(
      const std::vector<UTXO> & ccSortedInputsUserA, 
      const std::vector<UTXO> & paymentSortedInputsUserB,
      const CCoinSpender& structA, const CCoinSpender& structB,
      unsigned blockDelay = 0);

   void revoke(const bs::Address&);

   void waitOnZc(const Tx&);
   void waitOnZc(const BinaryData& hash, const std::vector<bs::Address>&);

   ////
   std::shared_ptr<ColoredCoinTracker> makeCct(void);
   void update(std::shared_ptr<ColoredCoinTracker>);
   void zcUpdate(std::shared_ptr<ColoredCoinTracker>);
   void reorg(std::shared_ptr<ColoredCoinTracker>);
};


class AsyncCCT_ACT : public SingleUTWalletACT
{
public:
   AsyncCCT_ACT(ArmoryConnection *armory)
      : SingleUTWalletACT(armory) {}

   void onRefresh(const std::vector<BinaryData> &ids, bool online) override
   {
      std::vector<std::string> removedIds;
      for (const auto &id : ids) {
         const auto it = refreshCb_.find(id.toBinStr());
         if (it != refreshCb_.end()) {
            it->second();
            removedIds.push_back(it->first);
         }
      }
      for (const auto &id : removedIds) {
         refreshCb_.erase(id);
      }

      SingleUTWalletACT::onRefresh(ids, online);
   }

   void addRefreshCb(const std::string &regId, const std::function<void()> &cb)
   {
      refreshCb_[regId] = cb;
   }

private:
   std::map<std::string, std::function<void()>> refreshCb_;
};

#endif // __TEST_CCOIN_H__

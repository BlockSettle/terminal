/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TEST_CCOIN_H__
#define __TEST_CCOIN_H__

#include <atomic>
#include <memory>
#include <set>
#include <gtest/gtest.h>

#include "Address.h"
#include "BlockchainMonitor.h"
//#include "ColoredCoinLogic.h"
#include "Wallets/SignContainer.h"
#include "TestEnv.h"

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
class QtHCT;

struct CCoinSpender
{
   bs::Address ccAddr_;
   bs::Address ccChange;
   bs::Address xbtAddr_;

   uint64_t ccValue_ = 0;
   uint64_t xbtValue_ = 0;
};

#if 0 // no CC tests since all CC code is turned off
class ColoredCoinTestACT : public ColoredCoinACT
{
private:
   ArmoryThreading::BlockingQueue<std::vector<BinaryData>> refreshQueue_;

public:
   ColoredCoinTestACT(ArmoryConnection *armory)
      : ColoredCoinACT(armory)
   {}

   virtual void onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>& zcs) override {}
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
   ArmoryThreading::BlockingQueue<std::shared_ptr<DBNotificationStruct>> updateQueue_;

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
         catch (ArmoryThreading::StopBlockingLoop&)
         {
            return;
         }
      }
   }
};

class ColoredCoinTracker_UT : public ColoredCoinTracker
{
private:
   void registerAddresses(std::set<BinaryData>&);

public:
   ColoredCoinTracker_UT(uint64_t coinsPerShare,
                         std::shared_ptr<ArmoryConnection> connPtr)
      : ColoredCoinTracker(coinsPerShare, connPtr)
   {}

   void setACT(std::shared_ptr<ColoredCoinACT> actPtr)
   {
      actPtr_ = actPtr;
   }

   void update_UT(void);
   void zcUpdate_UT(void);
   void reorg_UT(void);
};

class ColoredCoinTrackerClient_UT : public ColoredCoinTrackerClient
{
public:
   ColoredCoinTrackerClient_UT(std::unique_ptr<ColoredCoinTrackerInterface> ccSnapshots)
      : ColoredCoinTrackerClient(std::move(ccSnapshots))
   {}

   ColoredCoinTracker_UT *tracker()
   {
      return dynamic_cast<ColoredCoinTracker_UT*>(ccSnapshots_.get());
   }
};

class TestCCoin : public ::testing::Test, public SignerCallbackTarget
{
public:
   using UTXOs = std::vector<UTXO>;

   const int64_t ccLotSize_ = 307;
   size_t usersCount_ = 20;


   std::shared_ptr<bs::core::hd::Leaf>   rootSignWallet_;
   std::shared_ptr<bs::sync::hd::Leaf>   rootWallet_;
   std::vector<std::shared_ptr<bs::core::hd::Leaf>>   userSignWallets_;
   std::vector<std::shared_ptr<bs::sync::hd::Leaf>>   userWallets_;

   std::shared_ptr<bs::sync::WalletsManager> syncMgr_;
   std::vector<UnitTestLocalACT*> localACTs_;
   std::shared_ptr<QtHCT>        hct_;

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
   void InitialFund(const std::vector<bs::Address> &recipients = {});
   std::vector<UTXO> GetUTXOsFor(const bs::Address & addr, bool sortedByValue = true);
   std::vector<UTXO> GetCCUTXOsFor(std::shared_ptr<ColoredCoinTrackerClient> ccPtr,
      const bs::Address & addr, bool sortedByValue = true, bool withZc = false);

   BinaryData FundFromCoinbase(const std::vector<bs::Address> & addresses, const uint64_t & valuePerOne);
   BinaryData SimpleSendMany(const bs::Address & fromAddress, const std::vector<bs::Address> & toAddresses, const uint64_t & valuePerOne);

   Tx CreateCJtx(
      const std::vector<UTXO> & ccSortedInputsUserA, 
      const std::vector<UTXO> & paymentSortedInputsUserB,
      const CCoinSpender& structA, const CCoinSpender& structB,
      const std::vector<UTXO> & ccInputsAppend = {},
      unsigned blockDelay = 0, bool returnUnsigned = false);

   void revoke(const bs::Address&);

   void waitOnZc(const Tx&);
   bool waitOnZc(const BinaryData& hash, const std::vector<bs::Address>&);

   ////
   std::shared_ptr<ColoredCoinTrackerClient_UT> makeCct(void);
   void update(std::shared_ptr<ColoredCoinTrackerClient_UT>);
   void zcUpdate(std::shared_ptr<ColoredCoinTrackerClient_UT>);
   void reorg(std::shared_ptr<ColoredCoinTrackerClient_UT>);
};
#endif

#endif // __TEST_CCOIN_H__

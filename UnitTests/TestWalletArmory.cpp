/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <gtest/gtest.h>
#include <QComboBox>
#include <QLocale>
#include <QString>

#include "ApplicationSettings.h"
#include "BIP32_Node.h"
#include "CoreHDLeaf.h"
#include "CoreHDWallet.h"
#include "CoreWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessContainer.h"
#include "InprocSigner.h"
#include "SystemFileUtils.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletEncryption.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


class TestWalletWithArmory : public ::testing::Test
{
protected:
   void SetUp()
   {
      UnitTestWalletACT::clear();

      envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
      envPtr_->requireArmory();

      passphrase_ = SecureBinaryData::fromString("pass");
      bs::core::wallet::Seed seed{ 
         SecureBinaryData::fromString("dem seeds"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };

      walletPtr_ = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, envPtr_->armoryInstance()->homedir_);

      auto grp = walletPtr_->createGroup(walletPtr_->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
         leafPtr_ = grp->createLeaf(AddressEntryType_Default, 0, 10);
      }
   }

   void TearDown()
   {
      leafPtr_.reset();
      walletPtr_.reset();
      envPtr_.reset();
   }

public:
   SecureBinaryData passphrase_;
   std::shared_ptr<bs::core::hd::Wallet> walletPtr_;
   std::shared_ptr<bs::core::hd::Leaf> leafPtr_;
   std::shared_ptr<TestEnv> envPtr_;
};

TEST_F(TestWalletWithArmory, AddressChainExtension)
{
   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);

   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr->syncWallets(cbSync);
   EXPECT_TRUE(futSync.get());

   auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   ASSERT_NE(syncHdWallet, nullptr);

   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
   auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
   ASSERT_TRUE(syncLeaf != nullptr);

   //check wallet has 10 assets per account
   ASSERT_EQ(syncLeaf->getAddressPoolSize(), 20);

   const auto &lbdGetExtAddress = [syncWallet](AddressEntryType aet = AddressEntryType_Default) -> bs::Address {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
      };
      syncWallet->getNewExtAddress(cbAddr);
      return futAddr.get();
   };

   /***
   Grab 11 external addresses, we should have an address pool
   extention event, resulting in a pool of 360 hashes
   ***/

   std::vector<bs::Address> addrVec;
   for (unsigned i = 0; i < 12; i++) {
      addrVec.push_back(lbdGetExtAddress());
   }
   EXPECT_EQ(syncLeaf->getAddressPoolSize(), 108);

   //ext address creation should result in ext address chain extention, 
   //which will trigger the registration of the new addresses. There
   //should be a refresh notification for this event in the queue
   auto&& notif = UnitTestWalletACT::popNotif();
   ASSERT_EQ(notif->type_, DBNS_Refresh);
   ASSERT_EQ(notif->ids_.size(), 1);

   /***
   11th address is part of the newly computed assets, we need to
   confirm that it was registered with the db. We will mine some
   coins to it and expect to see a balance.
   ***/

   const auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addrVec[10].getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   /***
   mine some coins to original address set to make sure they
   dont get unregistered by the new addresses registration
   process
   ***/

   curHeight = envPtr_->armoryConnection()->topBlock();
   recipient = addrVec[0].getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //check the address balances
   //update balance
   auto promPtr2 = std::make_shared<std::promise<bool>>();
   auto fut2 = promPtr2->get_future();
   const auto &cbBalance = [promPtr2](void)
   {
      promPtr2->set_value(true);
   };

   //async, has to wait
   syncLeaf->updateBalances(cbBalance);
   fut2.wait();

   //check balance
   auto balances = syncLeaf->getAddrBalance(addrVec[0]);
   EXPECT_EQ(balances[0], 300 * COIN);

   balances = syncLeaf->getAddrBalance(addrVec[10]);
   EXPECT_EQ(balances[0], 300 * COIN);

   /***
   Sign coins off of new asset batch's address. Addresses generated
   that way only have public keys, as the wallet was not unlocked
   prior to chain extention.

   This test verifies the wallet resolver feed can compute the private
   key on the fly, as the wallet is unlocked for the signing process.
   ***/

   auto promPtr1 = std::make_shared<std::promise<bool>>();
   auto fut1 = promPtr1->get_future();

   const auto &cbTxOutList = [this, leaf=leafPtr_, syncLeaf, addrVec, promPtr1]
   (std::vector<UTXO> inputs)->void
   {
      const auto recipient = addrVec[11].getRecipient(bs::XBTAmount{ (uint64_t)(25 * COIN) });
      const auto txReq = syncLeaf->createTXRequest(inputs, { recipient }, true, 0, false, addrVec[0]);
      BinaryData txWrongSigned;
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, SecureBinaryData::fromString("wrongPass"));
         EXPECT_THROW(txWrongSigned = leaf->signTXRequest(txReq), std::exception);
         EXPECT_TRUE(txWrongSigned.empty());
      }
      BinaryData txSigned;
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
         txSigned = leaf->signTXRequest(txReq);
         ASSERT_FALSE(txSigned.empty());
      }
      EXPECT_NE(txWrongSigned, txSigned);

      Tx txObj(txSigned);
      envPtr_->armoryInstance()->pushZC(txSigned);

      auto&& zcVec = UnitTestWalletACT::waitOnZC();
      promPtr1->set_value(zcVec.size() == 1);
      EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());
   };

   //async, has to wait
   syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
   ASSERT_TRUE(fut1.get());

   //mine 6 more blocks
   curHeight = envPtr_->armoryConnection()->topBlock();
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //update balance
   auto promPtr4 = std::make_shared<std::promise<bool>>();
   auto fut4 = promPtr4->get_future();
   const auto &cbBalance2 = [promPtr4](void)
   {
      promPtr4->set_value(true);
   };

   //async, has to wait
   syncLeaf->updateBalances(cbBalance2);
   fut4.wait();

   //check balance
   balances = syncLeaf->getAddrBalance(addrVec[11]);
   EXPECT_EQ(balances[0], 25 * COIN);
   delete hct;
}

TEST_F(TestWalletWithArmory, RestoreWallet_CheckChainLength)
{
   std::shared_ptr<bs::core::wallet::Seed> seed;
   std::vector<bs::Address> extVec;
   std::vector<bs::Address> intVec;

   {
      auto hct = new QtHCT(nullptr);
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);

      auto promSync = std::make_shared<std::promise<bool>>();
      auto futSync = promSync->get_future();
      const auto &cbSync = [this, promSync](int cur, int total) {
         if (cur == total) {
            promSync->set_value(true);
         }
      };
      syncMgr->syncWallets(cbSync);
      EXPECT_TRUE(futSync.get());

      auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
      ASSERT_TRUE(syncLeaf != nullptr);

      syncLeaf->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
      auto regIDs = syncLeaf->registerWallet(envPtr_->armoryConnection());
      UnitTestWalletACT::waitOnRefresh(regIDs);

      //check wallet has 10 assets per account
      ASSERT_EQ(syncLeaf->getAddressPoolSize(), 20);

      const auto &lbdGetAddress = [syncWallet](bool ext) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         if (ext) {
            syncWallet->getNewExtAddress(cbAddr);
         }
         else {
            syncWallet->getNewIntAddress(cbAddr);
         }
         return futAddr.get();
      };

      //pull 13 ext addresses
      for (unsigned i = 0; i < 12; i++)
         extVec.push_back(lbdGetAddress(true));
      extVec.push_back(lbdGetAddress(true));

      //ext address creation should result in ext address chain extention, 
      //which will trigger the registration of the new addresses. There
      //should be a refresh notification for this event in the queue
      auto&& notif = UnitTestWalletACT::popNotif();
      ASSERT_EQ(notif->type_, DBNS_Refresh);
      ASSERT_EQ(notif->ids_.size(), 1);

      //pull 60 int addresses
      for (unsigned i = 0; i < 60; i++) {
         intVec.push_back(lbdGetAddress(false));
      }

      //same deal with int address creation, but this time it will trigger 3 times
      //(20 new addresses per extention call
//      for (unsigned y = 0; y < 3; y++) {
         notif = UnitTestWalletACT::popNotif();
         ASSERT_EQ(notif->type_, DBNS_Refresh);
         ASSERT_EQ(notif->ids_.size(), 1);
//      }

      //mine coins to ext[12]
      auto armoryInstance = envPtr_->armoryInstance();
      unsigned blockCount = 6;

      ASSERT_EQ(extVec.size(), 13);
      unsigned curHeight = envPtr_->armoryConnection()->topBlock();
      auto recipient = extVec[12].getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
      armoryInstance->mineNewBlock(recipient.get(), blockCount);
      auto newTop = UnitTestWalletACT::waitOnNewBlock();
      ASSERT_EQ(curHeight + blockCount, newTop);

      //update balance
      auto promPtr2 = std::make_shared<std::promise<bool>>();
      auto fut2 = promPtr2->get_future();
      const auto &cbBalance = [promPtr2](void)
      {
         promPtr2->set_value(true);
      };

      //async, has to wait
      syncLeaf->updateBalances(cbBalance);
      fut2.wait();

      //check balance
      auto balances = syncLeaf->getAddrBalance(extVec[12]);
      EXPECT_EQ(balances[0], 300 * COIN);

      //send coins to ext[13]
      extVec.push_back(lbdGetAddress(true));

      auto promPtr1 = std::make_shared<std::promise<bool>>();
      auto fut1 = promPtr1->get_future();

      auto leaf = leafPtr_;
      auto pass = passphrase_;
      const auto &cbTxOutList =
         [this, leaf, syncLeaf, extVec, intVec, promPtr1]
      (std::vector<UTXO> inputs)->void
      {
         /*
         Use only 1 utxo, send 25 to ext[13], change to int[41], 
         no fee
         */

         if (inputs.empty()) {
            promPtr1->set_value(false);
         }
         ASSERT_GE(inputs.size(), 1);
         EXPECT_EQ(inputs[0].getValue(), 50 * COIN);

         std::vector<UTXO> utxos;
         utxos.push_back(inputs[0]);

         const auto recipient = extVec[13].getRecipient(bs::XBTAmount{ (uint64_t)(25 * COIN) });
         const auto txReq = syncLeaf->createTXRequest(
            utxos, { recipient }, true, 0, false, intVec[41]);

         BinaryData txSigned;
         {
            const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
            txSigned = leaf->signTXRequest(txReq);
            ASSERT_FALSE(txSigned.empty());
         }

         Tx txObj(txSigned);
         envPtr_->armoryInstance()->pushZC(txSigned);

         auto&& zcVec = UnitTestWalletACT::waitOnZC();
         if (zcVec.size() != 2) {
            promPtr1->set_value(false);
         }
         ASSERT_EQ(zcVec.size(), 2);
         EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

         promPtr1->set_value(true);
      };

      //async, has to wait
      syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
      fut1.wait();

      //mine 6 more blocks
      curHeight = envPtr_->armoryConnection()->topBlock();
      armoryInstance->mineNewBlock(recipient.get(), blockCount);
      newTop = UnitTestWalletACT::waitOnNewBlock();
      ASSERT_EQ(curHeight + blockCount, newTop);

      //update balance
      auto promPtr4 = std::make_shared<std::promise<bool>>();
      auto fut4 = promPtr4->get_future();
      const auto &cbBalance2 = [promPtr4](void)
      {
         promPtr4->set_value(true);
      };

      //async, has to wait
      syncLeaf->updateBalances(cbBalance2);
      fut4.wait();

      //check balance
      balances = syncLeaf->getAddrBalance(extVec[12]);
      EXPECT_EQ(balances[0], 550 * COIN);

      balances = syncLeaf->getAddrBalance(extVec[13]);
      EXPECT_EQ(balances[0], 25 * COIN);

      balances = syncLeaf->getAddrBalance(intVec[41]);
      EXPECT_EQ(balances[0], 25 * COIN);

      //grab wallet seed
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
         seed = std::make_shared<bs::core::wallet::Seed>(
            walletPtr_->getDecryptedSeed());
      }

      //shutdown it all down
      leafPtr_.reset();
      walletPtr_->eraseFile();
      walletPtr_.reset();
      delete hct;
   }

   std::string filename;

   {
      //restore wallet from seed
      const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };
      walletPtr_ = std::make_shared<bs::core::hd::Wallet>(
         "test", "",
         *seed, pd,
         envPtr_->armoryInstance()->homedir_);

      auto grp = walletPtr_->createGroup(walletPtr_->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
         leafPtr_ = grp->createLeaf(AddressEntryType_Default, 0, 100);
      }

      //sync with db
      auto hct = new QtHCT(nullptr);
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);

      auto promSync = std::make_shared<std::promise<bool>>();
      auto futSync = promSync->get_future();
      const auto &cbSync = [this, promSync](int cur, int total) {
         if (cur == total) {
            promSync->set_value(true);
         }
      };
      syncMgr->syncWallets(cbSync);
      EXPECT_TRUE(futSync.get());

      const auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
      ASSERT_NE(syncHdWallet, nullptr);

      syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
      auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
      UnitTestWalletACT::waitOnRefresh(regIDs);

      auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
      ASSERT_TRUE(syncLeaf != nullptr);

      auto trackProm = std::make_shared<std::promise<bool>>();
      auto trackFut = trackProm->get_future();
      auto trackLbd = [trackProm](bool result)->void
      {
         ASSERT_TRUE(result);
         trackProm->set_value(true);
      };

      //synchronize address chain use
      syncMgr->trackAddressChainUse(trackLbd);
      trackFut.wait();

      //check wallet has 100 assets per account
      ASSERT_EQ(syncLeaf->getAddressPoolSize(), 200);

      //update balances
      auto promPtr2 = std::make_shared<std::promise<bool>>();
      auto fut2 = promPtr2->get_future();
      const auto &cbBalance = [promPtr2](void)
      {
         promPtr2->set_value(true);
      };

      //async, has to wait
      ASSERT_TRUE(syncLeaf->updateBalances(cbBalance));
      fut2.wait();

      //check balance
      auto balances = syncLeaf->getAddrBalance(extVec[12]);
      EXPECT_EQ(balances[0], 550 * COIN);

      balances = syncLeaf->getAddrBalance(extVec[13]);
      EXPECT_EQ(balances[0], 25 * COIN);

      balances = syncLeaf->getAddrBalance(intVec[41]);
      EXPECT_EQ(balances[0], 25 * COIN);

      //check address chain length
      EXPECT_EQ(syncLeaf->getExtAddressCount(), 14);
      EXPECT_EQ(syncLeaf->getIntAddressCount(), 42);

      std::vector<bs::Address> intVecRange;
      intVecRange.insert(intVecRange.end(), intVec.cbegin(), intVec.cbegin() + 42);
      const auto &intAddrList = syncLeaf->getIntAddressList();
      EXPECT_EQ(intAddrList.size(), 42);
      EXPECT_EQ(intAddrList, intVecRange);

      //check ext[12] is p2sh_p2wpkh
      const auto &extAddrList = syncLeaf->getExtAddressList();
      ASSERT_EQ(extAddrList.size(), 14);
      EXPECT_EQ(extAddrList[12].getType(), AddressEntryType_P2WPKH); // only one type for leaf now

      //check address list matches
      EXPECT_EQ(extAddrList, extVec);

      const auto &lbdLeafGetAddress = [syncLeaf](bool ext) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         if (ext) {
            syncLeaf->getNewExtAddress(cbAddr);
         }
         else {
            syncLeaf->getNewIntAddress(cbAddr);
         }
         return futAddr.get();
      };

      //pull more addresses
      extVec.push_back(lbdLeafGetAddress(true));

      for (unsigned i = 0; i < 5; i++)
         intVec.push_back(lbdLeafGetAddress(false));

      //check chain length
      EXPECT_EQ(syncLeaf->getExtAddressCount(), 15);
      EXPECT_EQ(syncLeaf->getIntAddressCount(), 47);

      filename = walletPtr_->getFileName();
      delete hct;
   }

   /*
   trackAddressChainUse syncs the wallet address chain length and 
   address types based on on-chain data. Our wallet currently has
   instantiated addresses that lie beyond the top addresse with
   history. 

   If we were to restore the wallet from scratch as tested in the 
   previous scope, those instantiated addresses would not be 
   detected, as they dont have any on chain activity.

   However, synchronizing the wallet from on disk data should not 
   result in the reseting of those extra addresses. This next
   scope tests for that.
   */

   {
      //reload wallet
      walletPtr_ = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet);

      //resync address chain use, it should not disrupt current state
      auto hct = new QtHCT(nullptr);
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);
      inprocSigner->Start();

      auto promSync = std::make_shared<std::promise<bool>>();
      auto futSync = promSync->get_future();
      const auto &cbSync = [this, promSync](int cur, int total) {
         if (cur == total) {
            promSync->set_value(true);
         }
      };
      syncMgr->syncWallets(cbSync);
      EXPECT_TRUE(futSync.get());

      const auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
      ASSERT_NE(syncHdWallet, nullptr);

      syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
      auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
      UnitTestWalletACT::waitOnRefresh(regIDs);

      auto trackProm = std::make_shared<std::promise<bool>>();
      auto trackFut = trackProm->get_future();
      auto trackLbd = [trackProm](bool result)->void
      {
         ASSERT_TRUE(result);
         trackProm->set_value(true);
      };

      //synchronize address chain use
      syncMgr->trackAddressChainUse(trackLbd);
      trackFut.wait();

      auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
      ASSERT_TRUE(syncLeaf != nullptr);

      //check wallet has 100 assets per account
      ASSERT_EQ(syncLeaf->getAddressPoolSize(), 200);

      //update balances
      auto promPtr2 = std::make_shared<std::promise<bool>>();
      auto fut2 = promPtr2->get_future();
      const auto &cbBalance = [promPtr2](void)
      {
         promPtr2->set_value(true);
      };
      //async, has to wait
      ASSERT_TRUE(syncLeaf->updateBalances(cbBalance));
      fut2.wait();

      //check balance
      auto balances = syncLeaf->getAddrBalance(extVec[12]);
      EXPECT_EQ(balances[0], 550 * COIN);

      balances = syncLeaf->getAddrBalance(extVec[13]);
      EXPECT_EQ(balances[0], 25 * COIN);

      balances = syncLeaf->getAddrBalance(intVec[41]);
      EXPECT_EQ(balances[0], 25 * COIN);

      //check address chain length
      EXPECT_EQ(syncLeaf->getExtAddressCount(), 15);
      EXPECT_EQ(syncLeaf->getIntAddressCount(), 47);

      //check ext[12] & [15] are p2sh_p2wpkh
      const auto &extAddrList = syncLeaf->getExtAddressList();
      ASSERT_EQ(extAddrList.size(), 15);
      EXPECT_EQ(extAddrList[12].getType(), AddressEntryType_P2WPKH);
      EXPECT_EQ(extAddrList[14].getType(), AddressEntryType_P2WPKH);

      //check address list matches
      EXPECT_EQ(extAddrList, extVec);
      delete hct;
   }
}

TEST_F(TestWalletWithArmory, Comments)
{
   const std::string addrComment("Test address comment");
   const std::string txComment("Test TX comment");

   auto addr = leafPtr_->getNewExtAddress();
   ASSERT_FALSE(addr.empty());

   auto changeAddr = leafPtr_->getNewChangeAddress();
   ASSERT_FALSE(changeAddr.empty());

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   
   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
   ASSERT_EQ(syncWallet->getUsedAddressCount(), 2);
   EXPECT_EQ(syncWallet->getUsedAddressList()[0], addr);

   EXPECT_TRUE(syncWallet->setAddressComment(addr, addrComment));
   EXPECT_EQ(leafPtr_->getAddressComment(addr), addrComment);

   //mine some coins to our wallet
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   const auto &curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addr.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //create tx from those fresh utxos, set a comment by tx hash and check it
   auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut = promPtr->get_future();
   auto cbTxOutList = [promPtr] (std::vector<UTXO> inputs)
   {
      promPtr->set_value(inputs);
   };
   EXPECT_TRUE(syncWallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true));
   const auto inputs = fut.get();
   ASSERT_FALSE(inputs.empty());
   const auto recip = addr.getRecipient(bs::XBTAmount{ (uint64_t)12000 });

   const auto txReq = syncWallet->createTXRequest(inputs, { recip }, true, 345
      , false, changeAddr);
   BinaryData txData;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txData = leafPtr_->signTXRequest(txReq);
   }

   ASSERT_FALSE(txData.empty());
   EXPECT_TRUE(syncWallet->setTransactionComment(txData, txComment));
   Tx tx(txData);
   EXPECT_TRUE(tx.isInitialized());
   EXPECT_EQ(leafPtr_->getTransactionComment(tx.getThisHash()), txComment);
   delete hct;
}

TEST_F(TestWalletWithArmory, ZCBalance)
{
   const auto addr1 = leafPtr_->getNewExtAddress();
   const auto addr2 = leafPtr_->getNewExtAddress();
   const auto changeAddr = leafPtr_->getNewChangeAddress();
   EXPECT_EQ(leafPtr_->getUsedAddressCount(), 3);

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   //register the wallet
   auto syncWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   auto syncLeaf = syncMgr->getWalletById(leafPtr_->walletId());

   syncWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   regIDs = syncWallet->setUnconfirmedTargets();
   ASSERT_EQ(regIDs.size(), 2);
   UnitTestWalletACT::waitOnRefresh(regIDs);

   //check balances are 0
   auto balProm = std::make_shared<std::promise<bool>>();
   auto balFut = balProm->get_future();
   auto waitOnBalance = [balProm](void)->void
   {
      balProm->set_value(true);
   };
   syncLeaf->updateBalances(waitOnBalance);
   balFut.wait();
   EXPECT_DOUBLE_EQ(syncLeaf->getTotalBalance(), 0);
   EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 0);
   EXPECT_DOUBLE_EQ(syncLeaf->getUnconfirmedBalance(), 0);

   //mine some coins
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addr1.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //grab balances and check
   auto balProm1 = std::make_shared<std::promise<bool>>();
   auto balFut1 = balProm1->get_future();
   auto waitOnBalance1 = [balProm1](void)->void 
   {
      balProm1->set_value(true);
   };
   syncLeaf->updateBalances(waitOnBalance1);
   balFut1.wait();
   EXPECT_DOUBLE_EQ(syncLeaf->getTotalBalance(), 300);
   EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 300);
   EXPECT_DOUBLE_EQ(syncLeaf->getUnconfirmedBalance(), 0);

   //spend these coins
   const uint64_t amount = 5.0 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   auto promPtr1 = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut1 = promPtr1->get_future();
   const auto &cbTxOutList = [promPtr1] (std::vector<UTXO> inputs)->void
   {
      promPtr1->set_value(inputs);
   };
   //async, has to wait
   syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
   const auto inputs = fut1.get();
   ASSERT_GE(inputs.size(), 1);

   //pick a single input
   std::vector<UTXO> utxos;
   utxos.push_back(inputs[0]);

   recipient = addr2.getRecipient(bs::XBTAmount{ amount });
   const auto randomPrivKey2 = CryptoPRNG::generateRandom(32);
   const auto randomPubKey2 = CryptoECDSA().ComputePublicKey(randomPrivKey2, true);
   const auto &otherAddr2 = bs::Address::fromPubKey(randomPubKey2, AddressEntryType_P2WPKH);
   const auto &recipient2 = otherAddr2.getRecipient(bs::XBTAmount{ amount });

   const auto txReq = syncLeaf->createTXRequest(
      utxos, { recipient, recipient2 }, true, fee, false, changeAddr);
   BinaryData txSigned;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txSigned = leafPtr_->signTXRequest(txReq);
      ASSERT_FALSE(txSigned.empty());
   }

   Tx txObj(txSigned);
   envPtr_->armoryInstance()->pushZC(txSigned);

   auto&& zcVec = UnitTestWalletACT::waitOnZC();
   EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

   //update balance
   auto promPtr2 = std::make_shared<std::promise<bool>>();
   auto fut2 = promPtr2->get_future();
   const auto &cbBalance = [promPtr2](void)
   { 
      promPtr2->set_value(true); 
   };

   //async, has to wait
   syncLeaf->updateBalances(cbBalance);
   fut2.wait();

   EXPECT_DOUBLE_EQ(syncLeaf->getTotalBalance(),
      double(300 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider);
   EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 250);
   EXPECT_DOUBLE_EQ(syncLeaf->getUnconfirmedBalance(),
      double(50 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider);

   auto bal = syncLeaf->getAddrBalance(addr1);
   ASSERT_EQ(bal.size(), 3);
   EXPECT_EQ(bal[0], 250 * COIN);

   bal = syncLeaf->getAddrBalance(addr2);
   ASSERT_EQ(bal.size(), 3);
   EXPECT_EQ(bal[0], amount);

   bal = syncLeaf->getAddrBalance(changeAddr);
   ASSERT_EQ(bal.size(), 3);
   EXPECT_EQ(bal[0], 50 * COIN - 2 * amount - fee);

   //try to grab zc utxos
   auto prom3 = std::make_shared<std::promise<bool>>();
   auto fut3 = prom3->get_future();
   auto zcTxOutLbd = [prom3, amount, fee](std::vector<UTXO> utxos)->void
   {
      ASSERT_EQ(utxos.size(), 2);

      EXPECT_EQ(utxos[0].getValue(), amount);
      EXPECT_EQ(utxos[1].getValue(), 50 * COIN - 2 * amount - fee);

      prom3->set_value(true);
   };
   syncLeaf->getSpendableZCList(zcTxOutLbd);
   fut3.wait();

   blockCount = 1;
   curHeight = envPtr_->armoryConnection()->topBlock();
   armoryInstance->mineNewBlock(recipient2.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   auto promUpdBal = std::make_shared<std::promise<bool>>();
   auto futUpdBal = promUpdBal->get_future();
   const auto &cbBalance4 = [promUpdBal](void)
   {
      promUpdBal->set_value(true);
   };
   //async, has to wait
   syncLeaf->updateBalances(cbBalance4);
   futUpdBal.wait();

   EXPECT_EQ(syncLeaf->getTotalBalance(),
      double(300 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider);
   EXPECT_EQ(syncLeaf->getSpendableBalance(),
      double(300 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider - syncLeaf->getUnconfirmedBalance());
   EXPECT_EQ(syncLeaf->getUnconfirmedBalance(), 0);

   blockCount = 5;
   curHeight = envPtr_->armoryConnection()->topBlock();
   armoryInstance->mineNewBlock(recipient2.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   promUpdBal = std::make_shared<std::promise<bool>>();
   futUpdBal = promUpdBal->get_future();
   const auto &cbBalance5 = [promUpdBal](void)
   {
      promUpdBal->set_value(true);
   };
   //async, has to wait
   syncLeaf->updateBalances(cbBalance5);
   futUpdBal.wait();

   EXPECT_EQ(syncLeaf->getTotalBalance(),
      double(300 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider);
   EXPECT_EQ(syncLeaf->getSpendableBalance(),
      double(300 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider - syncLeaf->getUnconfirmedBalance());
   EXPECT_EQ(syncLeaf->getUnconfirmedBalance(), 0);
   delete hct;
}

TEST_F(TestWalletWithArmory, SimpleTX_bech32)
{
   const auto addr1 = leafPtr_->getNewExtAddress();
   const auto addr2 = leafPtr_->getNewExtAddress();
   const auto addr3 = leafPtr_->getNewExtAddress();
   const auto changeAddr = leafPtr_->getNewChangeAddress();
   EXPECT_EQ(leafPtr_->getUsedAddressCount(), 4);

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   auto syncLeaf = syncMgr->getWalletById(leafPtr_->walletId());

   syncWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   //mine some coins
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addr1.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   const uint64_t amount1 = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   const auto &cbTX = [](bool result) {
      ASSERT_TRUE(result);
   };

   auto promPtr1 = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut1 = promPtr1->get_future();
   const auto &cbTxOutList1 = [promPtr1](std::vector<UTXO> inputs)
   {
      promPtr1->set_value(inputs);
   };
   
   syncLeaf->getSpendableTxOutList(cbTxOutList1, UINT64_MAX, true);
   const auto inputs1 = fut1.get();
   ASSERT_FALSE(inputs1.empty());

   const auto recipient1 = addr2.getRecipient(bs::XBTAmount{ amount1 });
   ASSERT_NE(recipient1, nullptr);
   const auto txReq1 = syncLeaf->createTXRequest(
      inputs1, { recipient1 }, true, fee, false, changeAddr);

   BinaryData txSigned1;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txSigned1 = leafPtr_->signTXRequest(txReq1);
      ASSERT_FALSE(txSigned1.empty());
   }

   envPtr_->armoryInstance()->pushZC(txSigned1);
   Tx txObj(txSigned1);

   auto&& zcVec = UnitTestWalletACT::waitOnZC();
   EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

   curHeight = envPtr_->armoryConnection()->topBlock();
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   auto promPtr2 = std::make_shared<std::promise<bool>>();
   auto fut2 = promPtr2->get_future();
   const auto &cbBalance = [promPtr2](void)
   {
         promPtr2->set_value(true);
   };

   syncLeaf->updateBalances(cbBalance);
   fut2.wait();
   EXPECT_EQ(syncLeaf->getAddrBalance(addr2)[0], amount1);

   auto promPtr3 = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut3 = promPtr3->get_future();
   const auto &cbTxOutList2 = [promPtr3](std::vector<UTXO> inputs)
   {
      promPtr3->set_value(inputs);
   };

   syncLeaf->getSpendableTxOutList(cbTxOutList2, UINT64_MAX, true);
   const auto inputs2 = fut3.get();
   ASSERT_FALSE(inputs2.empty());

   const uint64_t amount2 = 0.04 * BTCNumericTypes::BalanceDivider;
   const auto recipient2 = addr3.getRecipient(bs::XBTAmount{ amount2 });
   ASSERT_NE(recipient2, nullptr);
   const auto txReq2 = syncLeaf->createTXRequest(
      inputs2, { recipient2 }, true, fee, false, changeAddr);

   BinaryData txSigned2;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txSigned2 = leafPtr_->signTXRequest(txReq2);
      ASSERT_FALSE(txSigned2.empty());
   }

   envPtr_->armoryInstance()->pushZC(txSigned2);
   Tx txObj2(txSigned2);

   auto&& zcVec2 = UnitTestWalletACT::waitOnZC();
   EXPECT_EQ(zcVec2[0].txHash, txObj2.getThisHash());
   delete hct;
}

TEST_F(TestWalletWithArmory, SignSettlement)
{
   /*create settlement leaf*/

   //Grab a regular address to build the settlement leaf from, too
   //lazy to create an auth leaf. This test does not cover auth
   //logic anyways

   std::shared_ptr<bs::core::hd::SettlementLeaf> settlLeafPtr;
   std::vector<bs::Address> settlAddrVec;
   std::vector<BinaryData> settlementIDs;

   auto&& counterpartyPrivKey = CryptoPRNG::generateRandom(32);
   auto&& counterpartyPubKey = 
      CryptoECDSA().ComputePublicKey(counterpartyPrivKey, true);

   auto btcGroup = walletPtr_->getGroup(bs::hd::Bitcoin_test);
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });
   auto spendLeaf = btcGroup->getLeafByPath(xbtPath);
   auto settlementRootAddress = spendLeaf->getNewExtAddress();

   {
      //need to lock for leaf creation
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);

      /*
      Create settlement leaf from address, wallet will generate
      settlement group on the fly.
      */
      auto leafPtr = walletPtr_->createSettlementLeaf(settlementRootAddress);
      ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());

      settlLeafPtr = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leafPtr);
      ASSERT_TRUE(settlLeafPtr != nullptr);
   }

   /*sync the wallet and connect to db*/
   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   auto syncLeaf = syncMgr->getWalletById(leafPtr_->walletId());
   auto syncSettlLeaf = syncMgr->getWalletById(settlLeafPtr->walletId());
   auto syncLeafPtr = 
      std::dynamic_pointer_cast<bs::sync::hd::SettlementLeaf>(syncSettlLeaf);
   ASSERT_NE(syncLeafPtr, nullptr);

   //grab the settlement leaf root pub key (typically the auth key) to check it
   auto promPubRoot = std::make_shared<std::promise<SecureBinaryData>>();
   auto futPubRoot = promPubRoot->get_future();
   const auto &cbPubRoot = [promPubRoot](const SecureBinaryData &pubRoot) {
      promPubRoot->set_value(pubRoot);
   };
   syncLeafPtr->getRootPubkey(cbPubRoot);
   const auto &settlPubRoot = futPubRoot.get();

   auto settlRootHash = BtcUtils::getHash160(settlPubRoot);
   EXPECT_EQ(settlementRootAddress.unprefixed(), settlRootHash);

   syncWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   //create some settlement ids
   for (unsigned i = 0; i < 5; i++)
      settlementIDs.push_back(CryptoPRNG::generateRandom(32));

   //set a settlemend id
   auto promSettlId = std::make_shared<std::promise<bool>>();
   auto futSettlId = promSettlId->get_future();
   const auto &cbSettlId = [promSettlId](bool result) {
      promSettlId->set_value(result);
   };
   syncLeafPtr->setSettlementID(settlementIDs[0], cbSettlId);
   EXPECT_TRUE(futSettlId.get());

   /*create settlement multisig script and fund it*/
   auto promMsAddr = std::make_shared<std::promise<bs::Address>>();
   auto futMsAddr = promMsAddr->get_future();
   const auto &cbMsAddr = [promMsAddr](const bs::Address &addr) {
      promMsAddr->set_value(addr);
   };
   syncWallet->getSettlementPayinAddress(settlementIDs[0]
      , counterpartyPubKey, cbMsAddr);
   const auto msAddress = futMsAddr.get();

   /*send to the script*/
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = msAddress.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   /*get a multisig utxo*/
   auto promPtr1 = std::make_shared<std::promise<UTXO>>();
   auto fut1 = promPtr1->get_future();
   const auto &cbUtxo = [promPtr1](const std::vector<UTXO> &utxoVec)
   {
      ASSERT_NE(utxoVec.size(), 0);
      promPtr1->set_value(utxoVec[0]);
   };

   envPtr_->armoryConnection()->getUTXOsForAddress(msAddress, cbUtxo);
   auto utxo1 = fut1.get();
   std::vector<UTXO> utxos = { utxo1 };

   /*spend it*/
   {
      //create tx request
      const auto txReq = syncLeaf->createTXRequest(utxos,
         {
            settlementRootAddress.getRecipient(bs::XBTAmount{ (uint64_t)(20 * COIN) }),
            msAddress.getRecipient(bs::XBTAmount{(uint64_t)(30 * COIN)})
         },
         0, false);

      //sign it
      BinaryData signedTx;
      {
         const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
         signedTx = walletPtr_->signSettlementTXRequest(
            txReq, { settlementIDs[0], counterpartyPubKey, true });
      }

      //broadcast and wait on zc
      Tx txObj(signedTx);
      envPtr_->armoryInstance()->pushZC(signedTx);

      while (true)
      {
         auto&& zcVec = UnitTestWalletACT::waitOnZC(false);
         if (zcVec.size() != 1)
            continue;

         if (zcVec[0].txHash == txObj.getThisHash())
            break;
      }
   }
   delete hct;
}

TEST_F(TestWalletWithArmory, GlobalDelegateConf)
{
   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);

   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr->syncWallets(cbSync);
   EXPECT_TRUE(futSync.get());

   auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   ASSERT_NE(syncHdWallet, nullptr);

   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
   auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
   ASSERT_TRUE(syncLeaf != nullptr);

   const auto &lbdGetExtAddress = [syncWallet](AddressEntryType aet = AddressEntryType_Default) -> bs::Address {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
      };
      syncWallet->getNewExtAddress(cbAddr);
      return futAddr.get();
   };
   const auto &lbdGetIntAddress = [syncWallet](AddressEntryType aet = AddressEntryType_Default) -> bs::Address {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
      };
      syncWallet->getNewIntAddress(cbAddr);
      return futAddr.get();
   };

   const bs::Address addr = lbdGetExtAddress();

   const auto armoryInstance = envPtr_->armoryInstance();

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   const auto recipient = addr.getRecipient(bs::XBTAmount{ (uint64_t)(5 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), 6);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + 6, newTop);
   curHeight = newTop;

   //check the address balances
   //update balance
   auto promPtrBal = std::make_shared<std::promise<bool>>();
   auto futBal = promPtrBal->get_future();
   const auto &cbBalance = [promPtrBal](void)
   {
      promPtrBal->set_value(true);
   };
   //async, has to wait
   syncLeaf->updateBalances(cbBalance);
   futBal.wait();

   //check balance
   const auto balances = syncLeaf->getAddrBalance(addr);
   EXPECT_EQ(balances[0], 30 * COIN);

   auto promLedger1 = std::make_shared<std::promise<std::shared_ptr<AsyncClient::LedgerDelegate>>>();
   auto futLedger1 = promLedger1->get_future();
   const auto cbLedger1 = [promLedger1](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      promLedger1->set_value(delegate);
   };
   ASSERT_TRUE(envPtr_->armoryConnection()->getWalletsLedgerDelegate(cbLedger1));
   auto globalLedger = futLedger1.get();
   ASSERT_NE(globalLedger, nullptr);

   const auto &lbdGetLDEntries = [](const std::shared_ptr<AsyncClient::LedgerDelegate> &ledger)
      -> std::shared_ptr<std::vector<ClientClasses::LedgerEntry>>
   {
      auto promLDPageCnt1 = std::make_shared<std::promise<uint64_t>>();
      auto futLDPageCnt1 = promLDPageCnt1->get_future();
      const auto cbLDPageCnt1 = [promLDPageCnt1](ReturnMessage<uint64_t> msg) {
         try {
            const auto pageCnt = msg.get();
            promLDPageCnt1->set_value(pageCnt);
         } catch (...) {
            promLDPageCnt1->set_value(0);
         }
      };
      ledger->getPageCount(cbLDPageCnt1);
      auto pageCnt1 = futLDPageCnt1.get();
      EXPECT_GE(pageCnt1, 1);

      auto ledgerEntries = std::make_shared<std::vector<ClientClasses::LedgerEntry>>();
      auto promLDEntries1 = std::make_shared<std::promise<bool>>();
      auto futLDEntries1 = promLDEntries1->get_future();
      const auto &cbHistPage1 = [&pageCnt1, promLDEntries1, ledgerEntries]
      (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> msg)
      {
         try {
            const auto &entries = msg.get();
            ledgerEntries->insert(ledgerEntries->end(), entries.cbegin(), entries.cend());
         } catch (...) {
            promLDEntries1->set_value(false);
            return;
         }
         if (--pageCnt1 == 0) {
            promLDEntries1->set_value(true);
         }
      };
      for (uint64_t pageId = 0; pageId < pageCnt1; ++pageId) {
         ledger->getHistoryPage(pageId, cbHistPage1);
      }
      EXPECT_TRUE(futLDEntries1.get());
      return ledgerEntries;
   };
   auto ledgerEntries = lbdGetLDEntries(globalLedger);
   ASSERT_FALSE(ledgerEntries->empty());
   EXPECT_EQ(ledgerEntries->size(), 6);   // 6 blocks were mined as initial funding
   EXPECT_EQ((*ledgerEntries)[0].getBlockNum(), newTop);
   EXPECT_EQ((*ledgerEntries)[5].getBlockNum(), 1);

   const auto addr1 = lbdGetExtAddress();
   const auto changeAddr = lbdGetIntAddress();

   auto promTxOut = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto futTxOut = promTxOut->get_future();
   const auto &cbTxOutList = [promTxOut] (const std::vector<UTXO> &inputs)
   {
      promTxOut->set_value(inputs);
   };
   //async, has to wait
   EXPECT_TRUE(syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true));
   const auto &inputs = futTxOut.get();
   ASSERT_FALSE(inputs.empty());

   const auto recip1 = addr1.getRecipient(bs::XBTAmount{ (uint64_t)(5 * COIN) });
   const auto txReq = syncLeaf->createTXRequest(inputs, { recip1 }, true, 0, false, changeAddr);
   BinaryData txSigned;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txSigned = leafPtr_->signTXRequest(txReq);
      ASSERT_FALSE(txSigned.empty());
   }

   UnitTestWalletACT::clear();

   envPtr_->armoryInstance()->pushZC(txSigned);

   auto&& zcVec = UnitTestWalletACT::waitOnZC();

   ledgerEntries = lbdGetLDEntries(globalLedger);
   ASSERT_FALSE(ledgerEntries->empty());
   EXPECT_EQ(ledgerEntries->size(), 8);
   EXPECT_EQ((*ledgerEntries)[0].getBlockNum(), UINT32_MAX);

   // Mine new block - conf count for entry[0] should increase
   armoryInstance->mineNewBlock(recipient.get(), 1);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + 1, newTop);

#if 0    // ledger delegate should be refreshed on each new block event, but it works fine even without it
   auto promLedger2 = std::make_shared<std::promise<std::shared_ptr<AsyncClient::LedgerDelegate>>>();
   auto futLedger2 = promLedger2->get_future();
   const auto cbLedger2 = [promLedger2](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      promLedger2->set_value(delegate);
   };
   ASSERT_TRUE(envPtr_->armoryConnection()->getWalletsLedgerDelegate(cbLedger2));
   globalLedger = futLedger2.get();
   ASSERT_NE(globalLedger, nullptr);
#endif   //0

   ledgerEntries = lbdGetLDEntries(globalLedger);
   ASSERT_FALSE(ledgerEntries->empty());
   EXPECT_EQ(ledgerEntries->size(), 9);   // we have one additional TX on addr at mining
   EXPECT_EQ(envPtr_->armoryConnection()->getConfirmationsNumber((*ledgerEntries)[1].getBlockNum()), 1);
   delete hct;
}

TEST_F(TestWalletWithArmory, CallbackReturnTxCrash)
{
   auto addr = leafPtr_->getNewExtAddress();
   ASSERT_FALSE(addr.empty());

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());

   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto recipient = addr.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   envPtr_->armoryInstance()->mineNewBlock(recipient.get(), 1);
   UnitTestWalletACT::waitOnNewBlock();

   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   // request some unknown TX, CallbackReturn_Tx should not crash
   envPtr_->armoryConnection()->getTxByHash(BinaryData::CreateFromHex("ed3e119ee826752bc49bf33e86eee1b079dcd7d3ee294a4586192fb0bb1f1002")
      , [promSync] (const Tx& tx) {
      promSync->set_value(true);
   }, true);
   futSync.wait();
   delete hct;
}

TEST_F(TestWalletWithArmory, PushZC_retry)
{
   const auto addr1 = leafPtr_->getNewExtAddress();
   const auto addr2 = leafPtr_->getNewExtAddress();
   const auto changeAddr = leafPtr_->getNewChangeAddress();
   EXPECT_EQ(leafPtr_->getUsedAddressCount(), 3);

   //add an extra address not part of the wallet
   BinaryData prefixed;
   prefixed.append(AddressEntry::getPrefixByte(AddressEntryType_P2PKH));
   prefixed.append(CryptoPRNG::generateRandom(20));
   auto otherAddr = bs::Address::fromHash(prefixed);

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, hct, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   //register the wallet
   auto syncWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   auto syncLeaf = syncMgr->getWalletById(leafPtr_->walletId());

   syncWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   regIDs = syncWallet->setUnconfirmedTargets();
   ASSERT_EQ(regIDs.size(), 2);
   UnitTestWalletACT::waitOnRefresh(regIDs);

   //mine some coins
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addr1.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //spend these coins
   const uint64_t amount = 5.0 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   auto promPtr1 = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut1 = promPtr1->get_future();
   const auto &cbTxOutList = [promPtr1](std::vector<UTXO> inputs)->void
   {
      promPtr1->set_value(inputs);
   };
   //async, has to wait
   syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
   const auto inputs = fut1.get();
   ASSERT_GE(inputs.size(), 1);

   //pick a single input
   std::vector<UTXO> utxos;
   utxos.push_back(inputs[0]);

   recipient = addr2.getRecipient(bs::XBTAmount{ amount });
   const auto recipient2 = otherAddr.getRecipient(bs::XBTAmount{ amount });
   const auto txReq = syncLeaf->createTXRequest(
      utxos, { recipient, recipient2 }, true, fee, false, changeAddr);
   BinaryData txSigned;
   {
      const bs::core::WalletPasswordScoped lock(walletPtr_, passphrase_);
      txSigned = leafPtr_->signTXRequest(txReq);
      ASSERT_FALSE(txSigned.empty());
   }

   Tx txObj(txSigned);
   envPtr_->armoryConnection()->pushZC(txSigned);

   auto&& zcVec = UnitTestWalletACT::waitOnZC();
   EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

   blockCount = 7;
   curHeight = envPtr_->armoryConnection()->topBlock();
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   const auto &zcId = envPtr_->armoryConnection()->pushZC(txSigned);
   EXPECT_EQ(UnitTestWalletACT::waitOnBroadcastError(zcId), -27); // Already-in-chain
   delete hct;
}

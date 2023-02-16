/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
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
#include "Wallets/HeadlessContainer.h"
#include "Wallets/InprocSigner.h"
#include "SystemFileUtils.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletEncryption.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

/***
unit tests to add:
- BIP32 path codec, with expected success and failure tests. Check vanity 
  node hashing in particular

- Generate an address for each eligible address type, check the path can be 
  resolved (getAddressIndex). Make sure invalid ones can't be fetched.

- Create wallet, alter state, destroy object, load from disk, check state 
  matches.

- Same but with a sync wallet this time. Make sure in particular that the 
  various address maps match, and that outer and inner address chain use 
  index are the same over multiple loads. Both remote and inproc.

- Restoring a wallet from seed should restore its meta state as well (
  auth/cc chains)
***/

////////////////////////////////////////////////////////////////////////////////
class TestWallet : public ::testing::Test, public SignerCallbackTarget
{
   void SetUp()
   {
      envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
      passphrase_ = SecureBinaryData::fromString("pass");
      walletFolder_ = std::string("./homedir");

      DBUtils::removeDirectory(walletFolder_);
      SystemFileUtils::mkPath(walletFolder_);
   }

   void TearDown()
   {
      envPtr_.reset();
      DBUtils::removeDirectory(walletFolder_);
   }

public:
   SecureBinaryData passphrase_;
   std::shared_ptr<TestEnv> envPtr_;
   std::string walletFolder_;
};

TEST_F(TestWallet, BIP84_derivation)
{
   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
   const auto passphrase = SecureBinaryData::fromString("passphrase");
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
   const SecureBinaryData ctrlPass;
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", "", seed, pd, walletFolder_);
   ASSERT_NE(wallet, nullptr);

   {
      const bs::core::WalletPasswordScoped lock(wallet, passphrase);
      wallet->createStructure(false);
      ASSERT_NE(wallet->getGroup(wallet->getXBTGroupType()), nullptr);
   }

   const auto grpXbt = wallet->getGroup(wallet->getXBTGroupType());
   ASSERT_NE(grpXbt, nullptr);

   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, wallet->getXBTGroupType(), 0 });
   const auto leafXbt = grpXbt->getLeafByPath(xbtPath);
   EXPECT_NE(leafXbt, nullptr);

   auto val = leafXbt->getExtAddressCount();

   BIP32_Node node;
   node.initFromSeed(seed.seed());
   std::vector<unsigned> derPath = {
      0x80000054, //84'
      0x80000001, //1'
      0x80000000, //0'
      0, 8
   };

   for (const auto &derInt : derPath) {
      node.derivePrivate(derInt);
   }
   auto pathstr = leafXbt->name();
   auto addrObj = leafXbt->getAddressByIndex(8, true);
   const auto pubKey = node.getPublicKey();
   auto pubkeyHash = BtcUtils::getHash160(pubKey);
   EXPECT_EQ(addrObj.unprefixed(), pubkeyHash);

   node.initFromSeed(seed.seed());
   derPath = {
      0x80000054, //84'
      0x80000001, //1'
      0x80000000  //0'
   };
   for (const auto &derInt : derPath) {
      node.derivePrivate(derInt);
   }

   BIP32_Node pubNode;
   pubNode.initFromPublicKey(derPath.size(), derPath.back(), node.getThisFingerprint()
      , node.getPublicKey(), node.getChaincode());
   pubNode.derivePublic(0);
   pubNode.derivePublic(8);
   EXPECT_EQ(pubKey.toHexStr(), pubNode.getPublicKey().toHexStr());

   ASSERT_TRUE(wallet->eraseFile());
}

TEST_F(TestWallet, BIP84_primary)
{
   auto passphrase = SecureBinaryData::fromString("passphrase");
   auto wrongPass = SecureBinaryData::fromString("wrongPass");
   ASSERT_NE(envPtr_->walletsMgr(), nullptr);

   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("Sample test seed")
      , NetworkType::TestNet };
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };

   auto coreWallet = envPtr_->walletsMgr()->createWallet("primary", "test"
      , seed, walletFolder_, pd, true, false);
   EXPECT_NE(envPtr_->walletsMgr()->getPrimaryWallet(), nullptr);

   auto wltMgr = envPtr_->walletsMgr();
   const auto wallet = envPtr_->walletsMgr()->getPrimaryWallet();
   ASSERT_NE(wallet, nullptr);
   EXPECT_EQ(wallet->name(), "primary");
   EXPECT_EQ(wallet->description(), "test");
   EXPECT_EQ(wallet->walletId(), seed.getWalletId());
   EXPECT_EQ(wallet->walletId(), "35ADZst46");

   const auto grpXbt = wallet->getGroup(wallet->getXBTGroupType());
   ASSERT_NE(grpXbt, nullptr);

   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, wallet->getXBTGroupType(), 0 });
   const auto leafXbt = grpXbt->getLeafByPath(xbtPath);
   EXPECT_NE(leafXbt, nullptr);
   EXPECT_EQ(leafXbt->shortName(), "0'");
   EXPECT_EQ(leafXbt->name(), "84'/1'/0'");
   EXPECT_EQ(leafXbt->getRootId().toHexStr(), "ead70174");

   EXPECT_THROW(grpXbt->createLeaf(AddressEntryType_Default, 0), std::exception);

   {
      const bs::core::WalletPasswordScoped lock(wallet, wrongPass);
      EXPECT_THROW(grpXbt->createLeaf(AddressEntryType_Default, 1, 10), std::exception);
      EXPECT_EQ(grpXbt->getNumLeaves(), 2);
   }

   {
      const bs::core::WalletPasswordScoped lock(wallet, passphrase);
      const auto leaf1 = grpXbt->createLeaf(AddressEntryType_P2WPKH, 1, 10);
      ASSERT_NE(leaf1, nullptr);
      EXPECT_EQ(grpXbt->getNumLeaves(), 3);
      EXPECT_EQ(leaf1->shortName(), "1'");
      EXPECT_EQ(leaf1->name(), "84'/1'/1'");
      //EXPECT_EQ(leaf1->description(), "test");
      EXPECT_TRUE(envPtr_->walletsMgr()->deleteWalletFile(leaf1));
      EXPECT_EQ(grpXbt->getNumLeaves(), 2);

      const auto grpCC = wallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
      const auto leafCC = grpCC->createLeaf(AddressEntryType_P2WPKH, 7568, 10);
      EXPECT_EQ(leafCC->name(), "84'/16979'/7568'"); //16979 == 0x4253
   }

   auto inprocSigner = std::make_shared<InprocSigner>(envPtr_->walletsMgr()
      , envPtr_->logger(), this, "", NetworkType::TestNet);
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncXbtLeaf = syncMgr->getWalletById(leafXbt->walletId());
   EXPECT_EQ(syncXbtLeaf->name(), "primary/XBT [TESTNET]/0'");

   EXPECT_TRUE(envPtr_->walletsMgr()->deleteWalletFile(wallet));
   EXPECT_EQ(envPtr_->walletsMgr()->getPrimaryWallet(), nullptr);
}

TEST_F(TestWallet, BIP84_address)
{
   const auto passphrase = SecureBinaryData::fromString("passphrase");
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet }
      , pd, walletFolder_);
   ASSERT_NE(wallet, nullptr);

   const bs::core::WalletPasswordScoped lock(wallet, passphrase);

   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(AddressEntryType_Default, 0, 10);
   ASSERT_NE(leaf, nullptr);
   EXPECT_EQ(leaf->getUsedAddressCount(), 0);

   const auto addr = leaf->getNewExtAddress();
   EXPECT_EQ(addr.display(), "tb1qgg97gkcelz6skrykaq3l4jszdd959atdnhtfj8");
   EXPECT_EQ(leaf->getUsedAddressCount(), 1);

   const auto chgAddr = leaf->getNewChangeAddress();
   EXPECT_EQ(chgAddr.display(), "tb1qrky2j35vncfg4q4gfexqn8824xgyd0njfcgg2y");
   EXPECT_EQ(leaf->getUsedAddressCount(), 2);

   EXPECT_TRUE(wallet->eraseFile());
}

TEST_F(TestWallet, BIP84_WatchingOnly)
{
   const auto passphrase = SecureBinaryData::fromString("passphrase");
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
   const size_t nbAddresses = 10;
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet}
      , pd, walletFolder_);
   ASSERT_NE(wallet, nullptr);
   EXPECT_FALSE(wallet->isWatchingOnly());
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   std::shared_ptr<bs::core::hd::Leaf> leaf1;
   {
      const bs::core::WalletPasswordScoped lock(wallet, passphrase);
      leaf1 = grp->createLeaf(AddressEntryType_Default, 0, 10);
      ASSERT_NE(leaf1, nullptr);
      EXPECT_FALSE(leaf1->isWatchingOnly());
      for (size_t i = 0; i < nbAddresses; i++) {
         leaf1->getNewExtAddress();
      }
      EXPECT_EQ(leaf1->getUsedAddressCount(), nbAddresses);

      auto leaf2 = grp->createLeaf(AddressEntryType_Default, 1, 10);
      ASSERT_NE(leaf2, nullptr);
      for (size_t i = 0; i < nbAddresses; i++) {
         leaf2->getNewExtAddress();
      }
      EXPECT_EQ(leaf2->getUsedAddressCount(), nbAddresses);
   }

   auto woWallet = wallet->createWatchingOnly();
   ASSERT_NE(woWallet, nullptr);
   EXPECT_EQ(woWallet->getGroups().size(), 1);
   auto woGroup = woWallet->getGroup(woWallet->getXBTGroupType());
   ASSERT_NE(woGroup, nullptr);

   const bs::hd::Path xbtPath1({ bs::hd::Purpose::Native, woWallet->getXBTGroupType(), 0 });
   auto woLeaf1 = woGroup->getLeafByPath(xbtPath1);
   ASSERT_NE(woLeaf1, nullptr);
   EXPECT_TRUE(woLeaf1->isWatchingOnly());
   const bs::hd::Path xbtPath2({ bs::hd::Purpose::Native, woWallet->getXBTGroupType(), 1 });
   auto woLeaf2 = woGroup->getLeafByPath(xbtPath2);
   ASSERT_NE(woLeaf2, nullptr);
   EXPECT_TRUE(woLeaf2->isWatchingOnly());
   EXPECT_EQ(woLeaf1->getUsedAddressCount(), nbAddresses);
   EXPECT_EQ(woLeaf2->getUsedAddressCount(), nbAddresses);

   const auto addrList = leaf1->getUsedAddressList();
   for (size_t i = 0; i < nbAddresses; i++) {
      const auto index = woLeaf1->addressIndex(addrList[i]);
      const auto addr = leaf1->getAddressByIndex(index, true);
      EXPECT_EQ(addrList[i].prefixed(), addr.prefixed()) << "addresses at " << index << " are unequal";
   }
   EXPECT_EQ(leaf1->getUsedAddressCount(), nbAddresses);

   EXPECT_TRUE(woWallet->isWatchingOnly());
   EXPECT_NE(woWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth), nullptr);
   EXPECT_THROW(woGroup->createLeaf(AddressEntryType_Default, 2), std::exception);

   EXPECT_TRUE(wallet->eraseFile());
   EXPECT_TRUE(woWallet->eraseFile());
}

TEST_F(TestWallet, ExtOnlyAddresses)
{
   const auto passphrase = SecureBinaryData::fromString("test");
   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };

   auto wallet1 = std::make_shared<bs::core::hd::Wallet>("test1", "", seed, pd, walletFolder_
      , envPtr_->logger());
   wallet1->setExtOnly();

   std::shared_ptr<bs::core::hd::Leaf> leaf1;
   {
      const bs::core::WalletPasswordScoped lock(wallet1, passphrase);

      auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
      ASSERT_NE(grp1, nullptr);

      leaf1 = grp1->createLeaf(AddressEntryType_Default, 0);
      ASSERT_NE(leaf1, nullptr);
      EXPECT_TRUE(leaf1->hasExtOnlyAddresses());
   }

   const auto addr1 = leaf1->getNewChangeAddress();
   const auto index1 = leaf1->getAddressIndex(addr1);
   EXPECT_EQ(index1, "0/0");

   const bs::core::wallet::Seed seed2{ SecureBinaryData::fromString("test seed 2"), NetworkType::TestNet };
   auto wallet2 = std::make_shared<bs::core::hd::Wallet>("test2", "", seed2, pd, walletFolder_
      , envPtr_->logger());

   std::shared_ptr<bs::core::hd::Leaf> leaf2;
   {
      const bs::core::WalletPasswordScoped lock(wallet2, passphrase);

      auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());
      ASSERT_NE(grp2, nullptr);

      leaf2 = grp2->createLeaf(AddressEntryType_Default, 0);
      ASSERT_NE(leaf2, nullptr);
      EXPECT_FALSE(leaf2->hasExtOnlyAddresses());
   }

   const auto addr2 = leaf2->getNewChangeAddress();
   const auto index2 = leaf2->getAddressIndex(addr2);
   EXPECT_EQ(index2, "1/0");
   EXPECT_NE(addr1, addr2);

   EXPECT_TRUE(wallet1->eraseFile());
   EXPECT_TRUE(wallet2->eraseFile());
}

TEST_F(TestWallet, CreateDestroyLoad)
{
   //setup bip32 node
   BIP32_Node baseNodeNative, baseNodeNested;
   baseNodeNative.initFromSeed(SecureBinaryData::fromString("test seed"));
   baseNodeNested.initFromSeed(SecureBinaryData::fromString("test seed"));

   std::vector<bs::Address> extAddrVecNative, extAddrVecNested;
   std::vector<bs::Address> intAddrVec;
   std::set<BinaryData> grabbedAddrHash;

   std::string filename, woFilename;

   const auto passphrase = SecureBinaryData::fromString("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, walletFolder_, envPtr_->logger());

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         walletPtr->createStructure(10);
      }

      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(groupPtr != nullptr);

      const bs::hd::Path xbtPathNative({ bs::hd::Purpose::Native, walletPtr->getXBTGroupType(), 0 });
      const bs::hd::Path xbtPathNested({ bs::hd::Purpose::Nested, walletPtr->getXBTGroupType(), 0 });
      auto leafPtrNative = groupPtr->getLeafByPath(xbtPathNative);
      auto leafPtrNested = groupPtr->getLeafByPath(xbtPathNested);
      ASSERT_NE(leafPtrNative, nullptr);
      ASSERT_NE(leafPtrNested, nullptr);

      //reproduce the keys as bip32 nodes
      std::vector<unsigned> derPathNative = {
         0x80000054, //84'
         0x80000001, //1'
         0x80000000  //0'
      };
      std::vector<unsigned> derPathNested = {
         0x80000031, //49'
         0x80000001, //1'
         0x80000000  //0'
      };

      for (auto& path : derPathNative) {
         baseNodeNative.derivePrivate(path);
      }
      for (auto& path : derPathNested) {
         baseNodeNested.derivePrivate(path);
      }

      //grab a bunch of addresses
      for (unsigned i = 0; i < 5; i++) {
         extAddrVecNative.push_back(leafPtrNative->getNewExtAddress());
      }
      for (unsigned i = 0; i < 5; i++) {
         extAddrVecNested.push_back(leafPtrNested->getNewExtAddress());
      }
      for (unsigned i = 0; i < 5; i++) {
         intAddrVec.push_back(leafPtrNative->getNewIntAddress());
      }
      //check address maps
      BIP32_Node extNode = baseNodeNative;
      extNode.derivePrivate(0);
      for (unsigned i = 0; i < 5; i++) {
         auto addrNode = extNode;
         addrNode.derivePrivate(i);
         auto addrHash = BtcUtils::getHash160(addrNode.getPublicKey());
         EXPECT_EQ(addrHash, extAddrVecNative[i].unprefixed());
      }

      extNode = baseNodeNested;
      extNode.derivePrivate(0);
      for (unsigned i = 0; i < 5; i++) {
         auto addrNode = extNode;
         addrNode.derivePrivate(i);
         auto scrHash = BtcUtils::getHash160(addrNode.getPublicKey());
         auto addrHash = BtcUtils::getP2WPKHOutputScript(scrHash);
         auto p2shHash = BtcUtils::getHash160(addrHash);
         EXPECT_EQ(p2shHash, extAddrVecNested[i].unprefixed());
      }

      BIP32_Node intNode = baseNodeNative;
      intNode.derivePrivate(1);
      for (unsigned i = 0; i < 5; i++) {
         auto addrNode = intNode;
         addrNode.derivePrivate(i);
         auto addr_hash = BtcUtils::getHash160(addrNode.getPublicKey());
         EXPECT_EQ(addr_hash, intAddrVec[i].unprefixed());
      }

      //check chain use counters
      EXPECT_EQ(leafPtrNative->getUsedAddressCount(), 10);
      EXPECT_EQ(leafPtrNative->getExtAddressCount(), 5);
      EXPECT_EQ(leafPtrNative->getIntAddressCount(), 5);
      EXPECT_EQ(leafPtrNested->getUsedAddressCount(), 5);
      EXPECT_EQ(leafPtrNested->getExtAddressCount(), 5);

      //fetch used address list, turn it into a set, 
      //same with grabbed addresses, check they match
      auto usedAddrList = leafPtrNative->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList) {
         usedAddrHash.insert(addr.prefixed());
      }

      grabbedAddrHash.insert(extAddrVecNative.begin(), extAddrVecNative.end());
      grabbedAddrHash.insert(intAddrVec.begin(), intAddrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when this scope exits
      filename = walletPtr->getFileName();
   }

   const SecureBinaryData ctrlPass;
   {
      //load from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(filename
         , NetworkType::TestNet, "", ctrlPass, envPtr_->logger());
      StaticLogger::loggerPtr->debug("walletPtr: {}", (void*)walletPtr.get());

      //run checks anew
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(groupPtr != nullptr);

      const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, walletPtr->getXBTGroupType(), 0 });
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      ASSERT_TRUE(leafPtr != nullptr);

      //fetch used address list, turn it into a set, 
      auto usedAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList) {
         usedAddrHash.insert(addr.prefixed());
      }
      //test it vs grabbed addresses
      EXPECT_EQ(usedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafPtr->getUsedAddressCount(), 10);
      EXPECT_EQ(leafPtr->getExtAddressCount(), 5);
      EXPECT_EQ(leafPtr->getIntAddressCount(), 5);

      ////////////////
      //create WO copy
      auto woCopy = walletPtr->createWatchingOnly();

      auto groupWO = woCopy->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(groupPtr != nullptr);

      auto leafWO = groupWO->getLeafByPath(xbtPath);
      ASSERT_TRUE(leafPtr != nullptr);

      //fetch used address list, turn it into a set, 
      auto woAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> woAddrHash;
      for (auto& addr : woAddrList)
         woAddrHash.insert(addr.prefixed());

      //test it vs grabbed addresses
      EXPECT_EQ(woAddrHash.size(), 10);
      EXPECT_EQ(woAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafWO->getUsedAddressCount(), 10);
      EXPECT_EQ(leafWO->getExtAddressCount(), 5);
      EXPECT_EQ(leafWO->getIntAddressCount(), 5);

      //exiting this scope will destroy both loaded wallet and wo copy object
      woFilename = woCopy->getFileName();

      //let's make sure the code isn't trying sneak the real wallet on us 
      //instead of the WO copy
      ASSERT_NE(woFilename, filename);
   }

   {
      int noop = 0;
      //load wo from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(woFilename
         , NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      EXPECT_TRUE(walletPtr->isWatchingOnly());

      //run checks one last time
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(groupPtr != nullptr);

      const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, walletPtr->getXBTGroupType(), 0 });
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      ASSERT_TRUE(leafPtr != nullptr);

      //fetch used address list, turn it into a set, 
      auto usedAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      //test it vs grabbed addresses
      EXPECT_EQ(usedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafPtr->getUsedAddressCount(), 10);
      EXPECT_EQ(leafPtr->getExtAddressCount(), 5);
      EXPECT_EQ(leafPtr->getIntAddressCount(), 5);
   }
}

TEST_F(TestWallet, CreateDestroyLoad_SyncWallet)
{
   const auto passphrase = SecureBinaryData::fromString("test");
   const SecureBinaryData ctrlPass;
   std::string filename;

   std::vector<bs::Address> extAddrVecNative, extAddrVecNested;
   std::vector<bs::Address> intAddrVec;

   //bip32 derived counterpart
   BIP32_Node base_node;
   base_node.initFromSeed(SecureBinaryData::fromString("test seed"));

   std::vector<unsigned> derPath = {
      0x80000054, //84' 
      0x80000001, //1'
      0x80000000  //0'
   };

   for (auto& path : derPath) {
      base_node.derivePrivate(path);
   }
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });

   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, walletFolder_, envPtr_->logger());

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         walletPtr->createStructure(10);
      }

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, this, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);
      auto promSync = std::make_shared<std::promise<bool>>();
      auto futSync = promSync->get_future();
      const auto &cbSync = [promSync](int cur, int total) {
         if ((cur < 0) || (total < 0)) {
            promSync->set_value(false);
            return;
         }
         if (cur == total) {
            promSync->set_value(true);
         }
      };
      syncMgr->syncWallets(cbSync);
      EXPECT_TRUE(futSync.get());

      //grab sync wallet
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      auto syncWallet = syncMgr->getWalletById(leafPtr->walletId());

      //grab addresses from sync wallet

      const auto &lbdGetSyncAddress = [syncWallet](bool ext) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         if (ext) {
            syncWallet->getNewExtAddress(cbAddr);
         }
         else {
            syncWallet->getNewChangeAddress(cbAddr);
         }
         return futAddr.get();
      };

      //p2wpkh
      for (unsigned i = 0; i < 5; i++) {
         const auto addr = lbdGetSyncAddress(true);
         extAddrVecNative.push_back(addr);
      }

      //nested p2wpkh
/*      for (unsigned i = 0; i < 4; i++) {
         const auto addr = lbdGetSyncAddress(true);   //TODO: should get from another leaf
         extAddrVecNested.push_back(addr);
      }*/

      //change addresses, p2wpkh
      for (unsigned i = 0; i < 5; i++) {
         const auto addr = lbdGetSyncAddress(false);
         intAddrVec.push_back(addr);
      }

      //check used addr count
      EXPECT_EQ(syncWallet->getUsedAddressCount(), 10);
      EXPECT_EQ(syncWallet->getExtAddressCount(), 5);
      EXPECT_EQ(syncWallet->getIntAddressCount(), 5);
#if 0
      syncWallet->syncAddresses();
#endif
      //check address maps
      BIP32_Node extNode = base_node;
      extNode.derivePrivate(0);
      for (unsigned i = 0; i < 5; i++) {
         auto addrNode = extNode;
         addrNode.derivePrivate(i);
         auto addrHash = BtcUtils::getHash160(addrNode.getPublicKey());
         EXPECT_EQ(addrHash, extAddrVecNative[i].unprefixed());
      }

/*      for (unsigned i = 0; i < 4; i++) {
         auto addrNode = extNode;
         addrNode.derivePrivate(i);
         auto scrHash = BtcUtils::getHash160(addrNode.getPublicKey());
         auto addrHash = BtcUtils::getP2WPKHOutputScript(scrHash);
         auto p2shHash = BtcUtils::getHash160(addrHash);
         EXPECT_EQ(p2shHash, extAddrVecNested[i].unprefixed());
      }*/

      BIP32_Node intNode = base_node;
      intNode.derivePrivate(1);
      for (unsigned i = 0; i < 5; i++) {
         auto addrNode = intNode;
         addrNode.derivePrivate(i);
         auto addrHash = BtcUtils::getHash160(addrNode.getPublicKey());
         EXPECT_EQ(addrHash, intAddrVec[i].unprefixed());
      }
   
      //shut it all down
      filename = walletPtr->getFileName();
   }

   {
      //reload wallet
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, this, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);
      auto promSync = std::make_shared<std::promise<bool>>();
      auto futSync = promSync->get_future();
      const auto &cbSync = [promSync](int cur, int total) {
         if ((cur < 0) || (total < 0)) {
            promSync->set_value(false);
            return;
         }
         if (cur == total) {
            promSync->set_value(true);
         }
      };
      syncMgr->syncWallets(cbSync);
      EXPECT_TRUE(futSync.get());

      //grab sync wallet
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      auto syncWallet = syncMgr->getWalletById(leafPtr->walletId());

      //check used addr count
      EXPECT_EQ(syncWallet->getUsedAddressCount(), leafPtr->getUsedAddressCount());
      EXPECT_EQ(syncWallet->getUsedAddressCount(), 10);
      EXPECT_EQ(syncWallet->getExtAddressCount(), leafPtr->getExtAddressCount());
      EXPECT_EQ(syncWallet->getExtAddressCount(), 5);
      EXPECT_EQ(syncWallet->getIntAddressCount(), leafPtr->getIntAddressCount());
      EXPECT_EQ(syncWallet->getIntAddressCount(), 5);

      //check used addresses match
      auto&& usedAddrList = syncWallet->getUsedAddressList();

      std::set<BinaryData> originalSet;
      for (auto& addr : extAddrVecNative) {
         originalSet.insert(addr.prefixed());
      }
      for (auto& addr : intAddrVec) {
         originalSet.insert(addr.prefixed());
      }
      std::set<BinaryData> loadedSet;
      for (auto& addr : usedAddrList) {
         loadedSet.insert(addr.prefixed());
      }
      EXPECT_EQ(originalSet.size(), 10);
      EXPECT_EQ(loadedSet.size(), 10);
      EXPECT_EQ(originalSet, loadedSet);

      const auto &lbdGetSyncAddress = [syncWallet](bool ext) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         if (ext) {
            syncWallet->getNewExtAddress(cbAddr);
         } else {
            syncWallet->getNewIntAddress(cbAddr);
         }
         return futAddr.get();
      };

      //grab new address, check it has the expected bip32 index
      auto newAddrExt = lbdGetSyncAddress(true);
      BIP32_Node ext_node = base_node;
      ext_node.derivePrivate(0);
      ext_node.derivePrivate(5);
      auto newAddrExtHash = BtcUtils::getHash160(ext_node.getPublicKey());
      EXPECT_EQ(newAddrExtHash, newAddrExt.unprefixed());
      extAddrVecNative.push_back(newAddrExt);

      auto newAddrInt = lbdGetSyncAddress(false);
      BIP32_Node int_node = base_node;
      int_node.derivePrivate(1);
      int_node.derivePrivate(5);
      auto newAddrIntHash = BtcUtils::getHash160(int_node.getPublicKey());
      EXPECT_EQ(newAddrIntHash, newAddrInt.unprefixed());
      intAddrVec.push_back(newAddrInt);

      //check used addr count again
      EXPECT_EQ(syncWallet->getUsedAddressCount(), 12);
      EXPECT_EQ(syncWallet->getExtAddressCount(), 6);
      EXPECT_EQ(syncWallet->getIntAddressCount(), 6);
#if 0
      syncWallet->syncAddresses();
#endif
      //create WO copy
      auto WOcopy = walletPtr->createWatchingOnly();
      filename = WOcopy->getFileName();

      //scope out to clean up prior to WO testing
   }

   //load WO, perform checks one last time
   {
      //reload wallet
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      EXPECT_EQ(walletPtr->isWatchingOnly(), true);

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, this, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);
      syncMgr->syncWallets();

      //grab sync wallet
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      auto syncWallet = syncMgr->getWalletById(leafPtr->walletId());

      //check used addr count
      EXPECT_EQ(syncWallet->getUsedAddressCount(), leafPtr->getUsedAddressCount() /*16*/);
      EXPECT_EQ(syncWallet->getExtAddressCount(), leafPtr->getExtAddressCount() /*10*/);
      EXPECT_EQ(syncWallet->getIntAddressCount(), leafPtr->getIntAddressCount() /*6*/);

      //check used addresses match
      auto&& usedAddrList = syncWallet->getUsedAddressList();

      std::set<BinaryData> originalSet;
      for (auto& addr : extAddrVecNative) {
         originalSet.insert(addr.prefixed());
      }
      for (auto& addr : intAddrVec) {
         originalSet.insert(addr.prefixed());
      }

      std::set<BinaryData> loadedSet;
      for (auto& addr : usedAddrList) {
         loadedSet.insert(addr.prefixed());
      }

      EXPECT_EQ(originalSet.size(), 12);
      EXPECT_EQ(originalSet.size(), loadedSet.size());
      EXPECT_EQ(originalSet, loadedSet);
   }
}

TEST_F(TestWallet, CreateDestroyLoad_AuthLeaf)
{
   //setup bip32 node
   BIP32_Node base_node;
   base_node.initFromSeed(SecureBinaryData::fromString("test seed"));

   std::vector<bs::Address> extAddrVec;
   std::set<BinaryData> grabbedAddrHash;

   std::string filename, woFilename;
   auto&& salt = CryptoPRNG::generateRandom(32);

   auto passphrase = SecureBinaryData::fromString("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, walletFolder_, envPtr_->logger());

      auto group = walletPtr->createGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(group != nullptr);

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      ASSERT_TRUE(authGroup != nullptr);
      authGroup->setSalt(salt);

      std::shared_ptr<bs::core::hd::Leaf> leafPtr;

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         leafPtr = group->createLeaf(AddressEntryType_Default, 0x800000b1, 10);
         ASSERT_TRUE(leafPtr != nullptr);
         ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());
      }

      auto authLeafPtr = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafPtr);
      ASSERT_TRUE(authLeafPtr != nullptr);
      ASSERT_EQ(authLeafPtr->getSalt(), salt);

      //reproduce the keys as bip32 nodes
      std::vector<unsigned> derPath = {
         0x80000054, //84' 
         0xc1757468, //Auth' in hexits
         0x800000b1 
      };

      for (auto& path : derPath)
         base_node.derivePrivate(path);

      //grab a bunch of addresses
      for (unsigned i = 0; i < 5; i++)
         extAddrVec.push_back(leafPtr->getNewExtAddress());

      //check address maps
      BIP32_Node ext_node = base_node;
      ext_node.derivePrivate(0);
      for (unsigned i = 0; i < 5; i++)
      {
         auto addr_node = ext_node;
         addr_node.derivePrivate(i);

         auto pubKey = addr_node.movePublicKey();
         auto saltedKey = CryptoECDSA::PubKeyScalarMultiply(pubKey, salt);
         auto addr_hash = BtcUtils::getHash160(saltedKey);
         EXPECT_EQ(addr_hash, extAddrVec[i].unprefixed());
      }

      //check chain use counters
      EXPECT_EQ(leafPtr->getUsedAddressCount(), 5);
      EXPECT_EQ(leafPtr->getExtAddressCount(), 5);
      EXPECT_EQ(leafPtr->getIntAddressCount(), 0);

      //fetch used address list, turn it into a set, 
      //same with grabbed addresses, check they match
      auto usedAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      grabbedAddrHash.insert(extAddrVec.begin(), extAddrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 5);
      EXPECT_EQ(usedAddrHash.size(), 5);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when on scope out
      filename = walletPtr->getFileName();
   }

   const SecureBinaryData ctrlPass;
   const bs::hd::Path authPath({ bs::hd::Purpose::Native, bs::hd::BlockSettle_Auth, 0xb1 });
   {
      //load from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      //run checks anew
      auto groupPtr = walletPtr->getGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(groupPtr != nullptr);

      auto authGroupPtr = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(groupPtr);
      ASSERT_TRUE(authGroupPtr != nullptr);
      EXPECT_EQ(authGroupPtr->getSalt(), salt);

      auto leafPtr = groupPtr->getLeafByPath(authPath);
      ASSERT_TRUE(leafPtr != nullptr);
      ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());

      auto authLeafPtr = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafPtr);
      ASSERT_TRUE(authLeafPtr != nullptr);
      EXPECT_EQ(authLeafPtr->getSalt(), salt);

      //fetch used address list, turn it into a set, 
      auto usedAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      //test it vs grabbed addresses
      EXPECT_EQ(usedAddrHash.size(), 5);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafPtr->getUsedAddressCount(), 5);
      EXPECT_EQ(leafPtr->getExtAddressCount(), 5);
      EXPECT_EQ(leafPtr->getIntAddressCount(), 0);

      //grab new address
      {
         auto newAddr = leafPtr->getNewExtAddress();
         BIP32_Node ext_node = base_node;
         ext_node.derivePrivate(0);
         ext_node.derivePrivate(5);
      
         auto pubKey = ext_node.movePublicKey();
         auto saltedKey = CryptoECDSA::PubKeyScalarMultiply(pubKey, salt);
         auto addr_hash = BtcUtils::getHash160(saltedKey);
         EXPECT_EQ(addr_hash, newAddr.unprefixed());

         extAddrVec.push_back(newAddr);
         grabbedAddrHash.insert(newAddr);
      }

      ////////////////
      //create WO copy
      auto woCopy = walletPtr->createWatchingOnly();
      EXPECT_TRUE(woCopy->isWatchingOnly());

      auto groupWO = woCopy->getGroup(bs::hd::BlockSettle_Auth);
      ASSERT_NE(groupWO, nullptr);

      auto authGroupWO = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(groupWO);
      ASSERT_NE(authGroupWO, nullptr);
      EXPECT_EQ(authGroupWO->getSalt(), salt);

      auto leafWO = groupWO->getLeafByPath(authPath);
      ASSERT_NE(leafWO, nullptr);
      EXPECT_TRUE(leafWO->hasExtOnlyAddresses());
      EXPECT_TRUE(leafWO->isWatchingOnly());

      auto authLeafWO = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafWO);
      ASSERT_NE(authLeafWO, nullptr);
      EXPECT_EQ(authLeafWO->getSalt(), salt);

      //fetch used address list, turn it into a set, 
      auto woAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> woAddrHash;
      for (auto& addr : woAddrList)
         woAddrHash.insert(addr.prefixed());

      //test it vs grabbed addresses
      EXPECT_EQ(woAddrHash.size(), 6);
      EXPECT_EQ(woAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafWO->getUsedAddressCount(), 6);
      EXPECT_EQ(leafWO->getExtAddressCount(), 6);
      EXPECT_EQ(leafWO->getIntAddressCount(), 0);

      //exiting this scope will destroy both loaded wallet and wo copy object
      woFilename = woCopy->getFileName();

      //let's make sure the code isn't trying sneak the real wallet on us 
      //instead of the WO copy
      ASSERT_NE(woFilename, filename);
   }
   
   {
      //load wo from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         woFilename, NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      EXPECT_TRUE(walletPtr->isWatchingOnly());

      //run checks one last time
      auto groupWO = walletPtr->getGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(groupWO != nullptr);

      auto authGroupWO = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(groupWO);
      ASSERT_TRUE(authGroupWO != nullptr);
      EXPECT_EQ(authGroupWO->getSalt(), salt);

      auto leafWO = authGroupWO->getLeafByPath(authPath);
      ASSERT_TRUE(leafWO != nullptr);
      ASSERT_TRUE(leafWO->hasExtOnlyAddresses());

      auto authLeafWO = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafWO);
      ASSERT_TRUE(authLeafWO != nullptr);
      EXPECT_EQ(authLeafWO->getSalt(), salt);
      ASSERT_TRUE(leafWO->isWatchingOnly());

      //fetch used address list, turn it into a set, 
      auto usedAddrList = leafWO->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      //test it vs grabbed addresses
      EXPECT_EQ(usedAddrHash.size(), 6);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //check chain use counters
      EXPECT_EQ(leafWO->getUsedAddressCount(), 6);
      EXPECT_EQ(leafWO->getExtAddressCount(), 6);
      EXPECT_EQ(leafWO->getIntAddressCount(), 0);

      //grab new address
      {
         auto newAddr = leafWO->getNewExtAddress();
         BIP32_Node ext_node = base_node;
         ext_node.derivePrivate(0);
         ext_node.derivePrivate(6);

         auto pubKey = ext_node.movePublicKey();
         auto saltedKey = CryptoECDSA::PubKeyScalarMultiply(pubKey, salt);
         auto addr_hash = BtcUtils::getHash160(saltedKey);
         EXPECT_EQ(addr_hash, newAddr.unprefixed());
      }
   }
}

TEST_F(TestWallet, CreateDestroyLoad_SettlementLeaf)
{
   std::vector<bs::Address> addrVec;
   std::set<BinaryData> grabbedAddrHash;

   std::string filename, woFilename;
   auto&& salt = CryptoPRNG::generateRandom(32);
   SecureBinaryData authPubKey;

   std::vector<BinaryData> settlementIDs;
   bs::Address authAddr;

   const auto passphrase = SecureBinaryData::fromString("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, walletFolder_, envPtr_->logger());

      auto group = walletPtr->createGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(group != nullptr);

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      ASSERT_TRUE(authGroup != nullptr);
      authGroup->setSalt(salt);

      std::shared_ptr<bs::core::hd::Leaf> leafPtr;

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         leafPtr = group->createLeaf(AddressEntryType_Default, 0, 10);
         ASSERT_TRUE(leafPtr != nullptr);
         ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());
      }

      auto authLeafPtr = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafPtr);
      ASSERT_TRUE(authLeafPtr != nullptr);
      ASSERT_EQ(authLeafPtr->getSalt(), salt);

      //grab auth address and its pubkey
      authAddr = authLeafPtr->getNewExtAddress();
      authPubKey = authLeafPtr->getPublicKeyFor(authAddr);

      {
         //need to lock for leaf creation
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);

         /*
         Create settlement leaf from address, wallet will generate 
         settlement group on the fly.
         */
         leafPtr = walletPtr->createSettlementLeaf(authAddr);
         ASSERT_TRUE(leafPtr != nullptr);
         ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());

      }

      //create a bunch of settlementIDs
      for (unsigned i = 0; i < 13; i++)
         settlementIDs.push_back(CryptoPRNG::generateRandom(32));

      //feed the ids to the settlement leaf
      auto settlLeafPtr = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leafPtr);
      ASSERT_TRUE(settlLeafPtr != nullptr);
      for (auto& settlID : settlementIDs)
         settlLeafPtr->addSettlementID(settlID);

      //grab a bunch of addresses
      for (unsigned i = 0; i < 7; i++)
         addrVec.push_back(leafPtr->getNewExtAddress());

      //check address maps
      for (unsigned i = 0; i < 7; i++)
      {
         auto saltedPubKey = CryptoECDSA::PubKeyScalarMultiply(
            authPubKey, settlementIDs[i]);
         auto addr_hash = BtcUtils::getHash160(saltedPubKey);

         EXPECT_EQ(addr_hash, addrVec[i].unprefixed());
      }

      //check chain use counters
      EXPECT_EQ(leafPtr->getUsedAddressCount(), 7);
      EXPECT_EQ(leafPtr->getExtAddressCount(), 7);

      //fetch used address list, turn it into a set, 
      //same with grabbed addresses, check they match
      auto usedAddrList = leafPtr->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      grabbedAddrHash.insert(addrVec.begin(), addrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 7);
      EXPECT_EQ(usedAddrHash.size(), 7);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when on scope out
      filename = walletPtr->getFileName();
   }

   //reload
   const SecureBinaryData ctrlPass;
   const bs::hd::Path settlPath({ bs::hd::Purpose::Native, bs::hd::BlockSettle_Settlement, 0 });
   {
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, StaticLogger::loggerPtr);

      //grab settlement group
      auto group = walletPtr->getGroup(bs::hd::BlockSettle_Settlement);
      ASSERT_NE(group, nullptr);

      //get settlement leaf from auth address path

      auto settlLeaf = group->getLeafByPath(settlPath);
      ASSERT_NE(settlLeaf, nullptr);

      auto usedAddrList = settlLeaf->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      EXPECT_EQ(usedAddrHash.size(), 7);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //grab extra addresses
      for (unsigned i = 7; i < 10; i++)
         addrVec.push_back(settlLeaf->getNewExtAddress());

      //check address maps
      for (unsigned i = 7; i < 10; i++)
      {
         auto saltedPubKey = CryptoECDSA::PubKeyScalarMultiply(
            authPubKey, settlementIDs[i]);
         auto addr_hash = BtcUtils::getHash160(saltedPubKey);

         EXPECT_EQ(addr_hash, addrVec[i].unprefixed());
         grabbedAddrHash.insert(addrVec[i]);
      }

      //create WO
      auto woWlt = walletPtr->createWatchingOnly();
      woFilename = woWlt->getFileName();
   }

   //wo
   {
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, StaticLogger::loggerPtr);

      //grab settlement group
      auto group = walletPtr->getGroup(bs::hd::BlockSettle_Settlement);
      ASSERT_NE(group, nullptr);

      //get settlement leaf from auth address path
      auto settlLeaf = group->getLeafByPath(settlPath);
      auto settlLeafPtr = 
         std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(settlLeaf);
      ASSERT_NE(settlLeafPtr, nullptr);

      auto usedAddrList = settlLeaf->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      EXPECT_EQ(usedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //add extra settlement ids
      for (unsigned i = 10; i < 13; i++)
      {
         settlLeafPtr->addSettlementID(settlementIDs[i]);
         addrVec.push_back(settlLeaf->getNewExtAddress());
         grabbedAddrHash.insert(addrVec[i]);
      }

      //check address maps
      for (unsigned i = 10; i < 13; i++)
      {
         auto saltedPubKey = CryptoECDSA::PubKeyScalarMultiply(
            authPubKey, settlementIDs[i]);
         auto addr_hash = BtcUtils::getHash160(saltedPubKey);

         EXPECT_EQ(addr_hash, addrVec[i].unprefixed());
      }
   }

   //reload WO, check again
   {
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", ctrlPass, StaticLogger::loggerPtr);

      //grab settlement group
      auto group = walletPtr->getGroup(bs::hd::BlockSettle_Settlement);
      ASSERT_NE(group, nullptr);

      //get settlement leaf from auth address path
      auto settlLeaf = group->getLeafByPath(settlPath);
      auto settlLeafPtr =
         std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(settlLeaf);
      ASSERT_NE(settlLeafPtr, nullptr);

      auto usedAddrList = settlLeaf->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.prefixed());

      EXPECT_EQ(usedAddrHash.size(), 13);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);
   }
}

TEST_F(TestWallet, SyncWallet_TriggerPoolExtension)
{
   const auto passphrase = SecureBinaryData::fromString("test");
   std::string filename;

   std::vector<bs::Address> extAddrVec;
   std::vector<bs::Address> intAddrVec;

   //bip32 derived counterpart
   BIP32_Node base_node;
   base_node.initFromSeed(SecureBinaryData::fromString("test seed"));

   std::vector<unsigned> derPath = {
      0x80000054, //84' 
      0x80000001, //1'
      0x80000000  //0'
   };
   for (auto& path : derPath) {
      base_node.derivePrivate(path);
   }
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });

   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>("test", ""
         , seed, pd, walletFolder_, envPtr_->logger());

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         walletPtr->createStructure(false, 10);
      }

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, this, envPtr_->logger());
      inprocSigner->Start();
      auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
         , envPtr_->appSettings(), envPtr_->armoryConnection());
      syncMgr->setSignContainer(inprocSigner);
      syncMgr->syncWallets();

      //grab sync wallet
      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      auto leafPtr = groupPtr->getLeafByPath(xbtPath);
      auto syncWallet = syncMgr->getWalletById(leafPtr->walletId());

      /*
      sync wallet should have 60 addresses:
       - 10 per assets per account
       - 2 accounts (inner & outer)
       - 3 address entry types per asset
      */

      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
      ASSERT_TRUE(syncLeaf != nullptr);
      EXPECT_EQ(syncLeaf->getAddressPoolSize(), 20);

      //grab addresses from sync wallet
      const auto &lbdGetSyncAddress = [syncWallet](bool ext, AddressEntryType aet = AddressEntryType_Default) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         if (ext) {
            syncWallet->getNewExtAddress(cbAddr);
         } else {
            syncWallet->getNewChangeAddress(cbAddr);
         }
         return futAddr.get();
      };

      //p2wpkh
      for (unsigned i = 0; i < 10; i++) {
         auto addr = lbdGetSyncAddress(true);
         extAddrVec.push_back(addr);
      }

      //change addresses, p2wpkh
      for (unsigned i = 0; i < 10; i++) {
         auto addr = lbdGetSyncAddress(false);
         intAddrVec.push_back(addr);
      }

      //check used addr count
      EXPECT_EQ(syncWallet->getUsedAddressCount(), 20);
      EXPECT_EQ(syncWallet->getExtAddressCount(), 10);
      EXPECT_EQ(syncWallet->getIntAddressCount(), 10);

      /***
      This shouldn't have triggered a pool top up. 20 addresses
      were pulled from the pool, 0 should be left
      ***/
      EXPECT_EQ(syncLeaf->getAddressPoolSize(), 0);

      {
         //pull 1 more external address, should trigger top up
         auto addr = lbdGetSyncAddress(true);
         extAddrVec.push_back(addr);

         /***
         This will add 100 addresses to the pool, minus the one just grabbed.
         ***/
         EXPECT_EQ(syncLeaf->getAddressPoolSize(), 99);
      }

      const auto &lbdGetIntAddress = [syncWallet](AddressEntryType aet = AddressEntryType_Default) -> bs::Address {
         auto promAddr = std::make_shared<std::promise<bs::Address>>();
         auto futAddr = promAddr->get_future();
         const auto &cbAddr = [promAddr](const bs::Address &addr) {
            promAddr->set_value(addr);
         };
         syncWallet->getNewIntAddress(cbAddr);
         return futAddr.get();
      };

      {
         //pull 1 more internal address, should trigger top up
         auto addr = lbdGetIntAddress();
         intAddrVec.push_back(addr);

         /***
         This will add 100 addresses to the pool, minus the one just grabbed.
         ***/
         EXPECT_EQ(syncLeaf->getAddressPoolSize(), 198);
      }

      //check address maps
      BIP32_Node ext_node = base_node;
      ext_node.derivePrivate(0);
      for (unsigned i = 0; i < 11; i++) {
         auto addr_node = ext_node;
         addr_node.derivePrivate(i);
         auto addr_hash = BtcUtils::getHash160(addr_node.getPublicKey());
         EXPECT_EQ(addr_hash, extAddrVec[i].unprefixed());
      }

      BIP32_Node int_node = base_node;
      int_node.derivePrivate(1);
      for (unsigned i = 0; i < 11; i++) {
         auto addr_node = int_node;
         addr_node.derivePrivate(i);
         auto addr_hash = BtcUtils::getHash160(addr_node.getPublicKey());
         EXPECT_EQ(addr_hash, intAddrVec[i].unprefixed());
      }

      //grab another 20 external addresses, shouldn't trigger top up
      for (unsigned i = 0; i < 20; i++) {
         auto addr = lbdGetSyncAddress(true);
         extAddrVec.push_back(addr);
      }
      EXPECT_EQ(syncLeaf->getAddressPoolSize(), 178);

      //grab another 100 internal addresses, should trigger top up
      for (unsigned i = 0; i < 100; i++) {
         const auto addr = lbdGetIntAddress();
         intAddrVec.push_back(addr);
      }
      EXPECT_EQ(syncLeaf->getAddressPoolSize(), 178);

      //check address maps
      for (unsigned i = 0; i < 31; i++) {
         auto addr_node = ext_node;
         addr_node.derivePrivate(i);
         auto addr_hash = BtcUtils::getHash160(addr_node.getPublicKey());
         EXPECT_EQ(addr_hash, extAddrVec[i].unprefixed());
      }

      for (unsigned i = 0; i < 31; i++) {
         auto addr_node = int_node;
         addr_node.derivePrivate(i);
         auto addr_hash = BtcUtils::getHash160(addr_node.getPublicKey());
         EXPECT_EQ(addr_hash, intAddrVec[i].unprefixed());
      }
   }
}

TEST_F(TestWallet, ImportExport_Easy16)
{
   const auto passphrase = SecureBinaryData::fromString("test");

   bs::core::wallet::Seed seed{ CryptoPRNG::generateRandom(32), NetworkType::TestNet };
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
   ASSERT_EQ(seed.seed().getSize(), 32);

   std::string filename, leaf1Id;
   bs::Address addr1;

   EasyCoDec::Data easySeed;

   {
      std::shared_ptr<bs::core::hd::Leaf> leaf1;

      auto wallet1 = std::make_shared<bs::core::hd::Wallet>(
         "test1", "", seed, pd, walletFolder_);
      auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(wallet1, passphrase);
         leaf1 = grp1->createLeaf(AddressEntryType_Default, 0u);
         addr1 = leaf1->getNewExtAddress();
      }

      //grab clear text seed
      std::shared_ptr<bs::core::wallet::Seed> seed1;
      try {
         //wallet isn't locked, should throw
         seed1 = std::make_shared<bs::core::wallet::Seed>(
            wallet1->getDecryptedSeed());
         ASSERT_TRUE(false);
      }
      catch (...) { }

      try {
         const bs::core::WalletPasswordScoped lock(wallet1, passphrase);
         seed1 = std::make_shared<bs::core::wallet::Seed>(
            wallet1->getDecryptedSeed());
      }
      catch (...) {
         ASSERT_TRUE(false);
      }

      //check seeds
      EXPECT_EQ(seed.seed(), seed1->seed());

      //create backup string for seed
      easySeed = seed1->toEasyCodeChecksum();
      ASSERT_FALSE(easySeed.part1.empty());
      ASSERT_FALSE(easySeed.part2.empty());

      //erase wallet1
      leaf1Id = leaf1->walletId();
      filename = wallet1->getFileName();
      ASSERT_TRUE(wallet1->eraseFile());
   }

   {
      //restore from easy16 seed
      const auto seedRestored =
         bs::core::wallet::Seed::fromEasyCodeChecksum(easySeed, NetworkType::TestNet);
      auto wallet2 = std::make_shared<bs::core::hd::Wallet>(
         "test2", "", seedRestored, pd, walletFolder_);
      auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());

      //check leaf id and addr data
      std::shared_ptr<bs::core::hd::Leaf> leaf2;
      bs::Address addr2;
      {
         const bs::core::WalletPasswordScoped lock(wallet2, passphrase);
         leaf2 = grp2->createLeaf(AddressEntryType_Default, 0u);
         addr2 = leaf2->getNewExtAddress();
      }

      EXPECT_EQ(leaf1Id, leaf2->walletId());
      EXPECT_EQ(addr1, addr2);

      //check seeds again
      std::shared_ptr<bs::core::wallet::Seed> seed2;
      try {
         const bs::core::WalletPasswordScoped lock(wallet2, passphrase);
         seed2 = std::make_shared<bs::core::wallet::Seed>(
            wallet2->getDecryptedSeed());
      }
      catch (...) {
         ASSERT_TRUE(false);
      }

      EXPECT_EQ(seed.seed(), seed2->seed());

      //shut it all down, reload, check seeds again
      filename = wallet2->getFileName();
   }

   auto wallet3 = std::make_shared<bs::core::hd::Wallet>(
      filename, NetworkType::TestNet);
   auto grp3 = wallet3->getGroup(wallet3->getXBTGroupType());

   //grab seed
   std::shared_ptr<bs::core::wallet::Seed> seed3;
   try {
      //wallet isn't locked, should throw
      seed3 = std::make_shared<bs::core::wallet::Seed>(
         wallet3->getDecryptedSeed());
      ASSERT_TRUE(false);
   }
   catch (...) { }

   try {
      const bs::core::WalletPasswordScoped lock(wallet3, passphrase);
      seed3 = std::make_shared<bs::core::wallet::Seed>(
         wallet3->getDecryptedSeed());
   }
   catch (...) {
      ASSERT_TRUE(false);
   }

   //check seed
   EXPECT_EQ(seed.seed(), seed3->seed());

   //check addr & id
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });
   auto leaf3 = grp3->getLeafByPath(xbtPath);
   auto addr3 = leaf3->getAddressByIndex(0, true);
   EXPECT_EQ(leaf1Id, leaf3->walletId());
   EXPECT_EQ(addr1, addr3);
}

TEST_F(TestWallet, ImportExport_xpriv)
{
   const auto passphrase = SecureBinaryData::fromString("test");
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };

   bs::core::wallet::Seed seed{ CryptoPRNG::generateRandom(32), NetworkType::TestNet };
   ASSERT_EQ(seed.seed().getSize(), 32);

   std::string filename, leaf1Id;
   bs::Address addr1;

   SecureBinaryData xpriv;

   {
      std::shared_ptr<bs::core::hd::Leaf> leaf1;

      auto wallet1 = std::make_shared<bs::core::hd::Wallet>(
         "test1", "", seed, pd, walletFolder_);
      auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(wallet1, passphrase);
         leaf1 = grp1->createLeaf(AddressEntryType_Default, 0u);
         addr1 = leaf1->getNewExtAddress();
      }

      //grab clear text seed
      std::shared_ptr<bs::core::wallet::Seed> seed1;
      try {
         //wallet isn't locked, should throw
         seed1 = std::make_shared<bs::core::wallet::Seed>(
            wallet1->getDecryptedSeed());
         ASSERT_TRUE(false);
      }
      catch (...) { }

      try {
         const bs::core::WalletPasswordScoped lock(wallet1, passphrase);
         seed1 = std::make_shared<bs::core::wallet::Seed>(
            wallet1->getDecryptedSeed());
      }
      catch (...) {
         ASSERT_TRUE(false);
      }

      //check seeds
      EXPECT_EQ(seed.seed(), seed1->seed());

      //create xpriv from seed
      xpriv = seed1->toXpriv();
      ASSERT_NE(xpriv.getSize(), 0);

      //erase wallet1
      leaf1Id = leaf1->walletId();
      filename = wallet1->getFileName();
      ASSERT_TRUE(wallet1->eraseFile());
   }

   {
      //restore from xpriv
      const auto seedRestored =
         bs::core::wallet::Seed::fromXpriv(xpriv, NetworkType::TestNet);
      auto wallet2 = std::make_shared<bs::core::hd::Wallet>(
         "test2", "", seedRestored, pd, walletFolder_);
      auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());

      //check leaf id and addr data
      std::shared_ptr<bs::core::hd::Leaf> leaf2;
      bs::Address addr2;
      {
         const bs::core::WalletPasswordScoped lock(wallet2, passphrase);
         leaf2 = grp2->createLeaf(AddressEntryType_Default, 0u);
         addr2 = leaf2->getNewExtAddress();
      }

      EXPECT_EQ(leaf1Id, leaf2->walletId());
      EXPECT_EQ(addr1, addr2);

      //check restoring from xpriv yields no seed
      try {
         const bs::core::WalletPasswordScoped lock(wallet2, passphrase);
         auto seed2 = std::make_shared<bs::core::wallet::Seed>(
            wallet2->getDecryptedSeed());
         ASSERT_TRUE(false);
      }
      catch (const Armory::Wallets::WalletException &) {}

      //shut it all down, reload, check seeds again
      filename = wallet2->getFileName();
   }

   auto wallet3 = std::make_shared<bs::core::hd::Wallet>(
      filename, NetworkType::TestNet);
   auto grp3 = wallet3->getGroup(wallet3->getXBTGroupType());

   //there still shouldnt be a seed to grab
   try {
      const bs::core::WalletPasswordScoped lock(wallet3, passphrase);
      auto seed3 = std::make_shared<bs::core::wallet::Seed>(
         wallet3->getDecryptedSeed());
      ASSERT_TRUE(false);
   }
   catch (...) {}

   //grab root
   {
      const bs::core::WalletPasswordScoped lock(wallet3, passphrase);
      auto xpriv3 = wallet3->getDecryptedRootXpriv();
      EXPECT_EQ(xpriv3, xpriv);
   }

   //check addr & id
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });
   auto leaf3 = grp3->getLeafByPath(xbtPath);
   auto addr3 = leaf3->getAddressByIndex(0, true);
   EXPECT_EQ(leaf1Id, leaf3->walletId());
   EXPECT_EQ(addr1, addr3);
}

TEST_F(TestWallet, MultipleKeys)
{
   const auto passphrase = SecureBinaryData::fromString("test");
   const auto authEidKey = CryptoPRNG::generateRandom(32);
   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
   const bs::wallet::PasswordData pd1{ passphrase, { bs::wallet::EncryptionType::Password } };
   const bs::wallet::PasswordData pd2{ authEidKey, { bs::wallet::EncryptionType::Auth, BinaryData::fromString("email@example.com") } };
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test1", "", seed, pd1, "./homedir"
      , StaticLogger::loggerPtr);
   ASSERT_NE(wallet, nullptr);

   SecureBinaryData privKey;
   {
      bs::core::WalletPasswordScoped lock(wallet, pd1.password);
      privKey = wallet->getDecryptedRootXpriv();
      ASSERT_FALSE(privKey.empty());
      wallet->addPassword(pd2);
   }
   {
      bs::core::WalletPasswordScoped lock(wallet, pd2.password);
      const auto privKey2 = wallet->getDecryptedRootXpriv();
      EXPECT_EQ(privKey, privKey2);
   }

   EXPECT_EQ(wallet->encryptionKeys().size(), 2);
   EXPECT_EQ(wallet->encryptionTypes().size(), 2);
   EXPECT_EQ(wallet->encryptionRank().n, 2);
   EXPECT_EQ(wallet->encryptionKeys()[1], pd2.metaData.encKey);
}

TEST_F(TestWallet, TxIdNativeSegwit)
{
   bs::core::wallet::TXSignRequest request;

   UTXO input;
   input.unserialize(BinaryData::CreateFromHex(
      "cc16060000000000741618000300010020d5921cfa9b95c9fdafa9dca6d2765b5d7d2285914909b8f5f74f0b137259153b16001428d45f4ef82103691ea40c26b893a4566729b335ffffffff"));
   request.armorySigner_.addSpender(std::make_shared<Armory::Signer::ScriptSpender>(input));

   auto recipient = Armory::Signer::ScriptRecipient::fromScript(BinaryData::CreateFromHex(
      "a086010000000000220020aa38b39ed9b524967159ad2bd488d14c1b9ccd70364655a7d9f35cb83e4dc6ed"));
   request.armorySigner_.addRecipient(recipient);

   request.change.value = 298894;
   request.fee = 158;
   request.change.address = bs::Address::fromAddressString("tb1q0yk8wytvdqhc4r3cm2kyjuj9l0l06dylncdlr0");

   ASSERT_NO_THROW(request.txId());
}

TEST_F(TestWallet, TxIdNestedSegwit)
{
   const auto passphrase = SecureBinaryData::fromString("password");
   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("TxId test seed"), NetworkType::TestNet };
   const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };

   auto coreWallet = std::make_shared<bs::core::hd::Wallet>("test", "", seed, pd, walletFolder_);
   ASSERT_NE(coreWallet, nullptr);

   auto grp = coreWallet->createGroup(coreWallet->getXBTGroupType());
   std::shared_ptr<bs::core::hd::Leaf> coreLeaf;
   {
      const bs::core::WalletPasswordScoped lock(coreWallet, passphrase);
      coreLeaf = grp->createLeaf(static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH)
         , 0, 10);
   }
   ASSERT_NE(coreLeaf, nullptr);

   const auto address = coreLeaf->getNewExtAddress();
   ASSERT_FALSE(address.empty());

   envPtr_->requireArmory();
   ASSERT_NE(envPtr_->armoryConnection(), nullptr);

   auto inprocSigner = std::make_shared<InprocSigner>(coreWallet, this, envPtr_->logger());
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

   const auto syncHdWallet = syncMgr->getHDWalletById(coreWallet->walletId());
   ASSERT_NE(syncHdWallet, nullptr);

   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
#if 0
   const auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   ASSERT_FALSE(regIDs.empty());
   UnitTestWalletACT::waitOnRefresh(regIDs);
#endif
   auto syncWallet = syncMgr->getWalletById(coreLeaf->walletId());
   auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);
   ASSERT_TRUE(syncLeaf != nullptr);

   const auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 6;

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto addrRecip = address.getRecipient(bs::XBTAmount{ (int64_t)(50 * COIN) });
   armoryInstance->mineNewBlock(addrRecip.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   auto promUtxo = std::make_shared<std::promise<UTXO>>();
   auto futUtxo = promUtxo->get_future();
   const auto &cbTxOutList = [this, promUtxo]
      (const std::vector<UTXO> &inputs)->void
   {
      if (inputs.empty()) {
         promUtxo->set_value({});
      }
      else {
         promUtxo->set_value(inputs.front());
      }
   };
#ifdef OLD_WALLETS_CODE
   ASSERT_TRUE(syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true));
#endif
   const auto input = futUtxo.get();
   ASSERT_TRUE(input.isInitialized());

   bs::core::wallet::TXSignRequest request;
   request.armorySigner_.addSpender(std::make_shared<Armory::Signer::ScriptSpender>(input));

   auto recipient = Armory::Signer::ScriptRecipient::fromScript(BinaryData::CreateFromHex(
      "a086010000000000220020d35c94ed03ae988841bd990124e176dae3928ba41f5a684074a857e788d768ba"));
   request.armorySigner_.addRecipient(recipient);

   request.change.value = 19899729;
   request.fee = 271;
   request.change.address = bs::Address::fromAddressString("2MykWqWBJGBeuyPGv73CisrokXKeGKNXU2C");

   const auto resolver = coreLeaf->getPublicResolver();

   try {
      const auto txId = request.txId(resolver);
   }
   catch (const std::exception &e) {
      ASSERT_FALSE(true) << e.what();
   }
}

TEST_F(TestWallet, ChangePassword)
{
   auto passMd = bs::wallet::PasswordMetaData{ bs::wallet::EncryptionType::Password, {} };

   auto passphraseOld = SecureBinaryData::fromString("passwordOld");
   auto passphraseNew = SecureBinaryData::fromString("passwordNew");

   const bs::wallet::PasswordData pdOld{ passphraseOld, passMd, {}, {} };
   const bs::wallet::PasswordData pdNew{ passphraseNew, passMd, {}, {} };

   ASSERT_NE(envPtr_->walletsMgr(), nullptr);

   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("Sample test seed")
      , NetworkType::TestNet };
   auto coreWallet = envPtr_->walletsMgr()->createWallet("primary", "test", seed, walletFolder_, pdOld, true, false);
   envPtr_->walletsMgr()->reset();

   {
      const bs::core::WalletPasswordScoped lock(coreWallet, passphraseOld);
      auto seedDecrypted = coreWallet->getDecryptedSeed();
      ASSERT_EQ(seed.seed(), seedDecrypted.seed());
   }

   {
      const bs::core::WalletPasswordScoped lock(coreWallet, passphraseOld);
      bool result = coreWallet->changePassword(passMd, pdNew);
      ASSERT_TRUE(result);
   }

   {
      const bs::core::WalletPasswordScoped lock(coreWallet, passphraseNew);
      auto seedDecrypted = coreWallet->getDecryptedSeed();
      EXPECT_EQ(seed.seed(), seedDecrypted.seed());
   }

   {
      const bs::core::WalletPasswordScoped lock(coreWallet, passphraseOld);
      EXPECT_THROW(coreWallet->getDecryptedSeed(), std::exception);
   }
}

TEST_F(TestWallet, ChangeControlPassword)
{
   auto passMd = bs::wallet::PasswordMetaData{ bs::wallet::EncryptionType::Password, {} };
   auto passphrase = SecureBinaryData::fromString("password");
   const auto walletName = "test";
   const auto walletDescr = "";
   std::string fileName;

   auto controlPassphraseEmpty = SecureBinaryData::fromString("");
   auto controlPassphrase1 = SecureBinaryData::fromString("controlPassword1");
   auto controlPassphrase2 = SecureBinaryData::fromString("controlPassword2");
   auto controlPassphraseWrong = SecureBinaryData::fromString("controlPassphraseWrong");

   const bs::wallet::PasswordData pd1{ passphrase, passMd, {}, controlPassphraseEmpty };

   ASSERT_NE(envPtr_->walletsMgr(), nullptr);

   const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("Sample test seed")
      , NetworkType::TestNet };

   {
      auto wallet = std::make_shared<bs::core::hd::Wallet>(
         walletName, walletDescr, seed, pd1, walletFolder_, envPtr_->logger());

      fileName = wallet->getFileName();

      // Set control password
      ASSERT_NO_THROW(wallet->changeControlPassword(controlPassphraseEmpty, controlPassphrase1));
      ASSERT_THROW(wallet->changeControlPassword(controlPassphraseEmpty, controlPassphrase2), std::runtime_error);

      // Change control password
      ASSERT_NO_THROW(wallet->changeControlPassword(controlPassphrase1, controlPassphrase2));
      ASSERT_THROW(wallet->changeControlPassword(controlPassphraseEmpty, controlPassphrase1), std::runtime_error);
      ASSERT_THROW(wallet->changeControlPassword(controlPassphraseWrong, controlPassphrase1), std::runtime_error);
   }

   EXPECT_THROW(bs::core::hd::Wallet(fileName, NetworkType::TestNet, "", controlPassphraseEmpty, envPtr_->logger()), std::runtime_error);
   EXPECT_THROW(bs::core::hd::Wallet(fileName, NetworkType::TestNet, "", controlPassphrase1, envPtr_->logger()), std::runtime_error);
   EXPECT_THROW(bs::core::hd::Wallet(fileName, NetworkType::TestNet, "", controlPassphraseWrong, envPtr_->logger()), std::runtime_error);

   EXPECT_NO_THROW(bs::core::hd::Wallet(fileName, NetworkType::TestNet, "", controlPassphrase2, envPtr_->logger()));
}

TEST_F(TestWallet, WalletMeta)
{
   const auto metaCount = 20;

   std::vector<std::pair<bs::Address, std::string>> addrComments;
   std::vector<std::pair<BinaryData, std::string>> txComments;
   for (int i = 0; i < metaCount; i++) {
      addrComments.push_back(std::make_pair(randomAddressPKH(), CryptoPRNG::generateRandom(20).toHexStr()));
   }

   struct SettlCp
   {
      BinaryData payinHash;
      BinaryData settlementId;
      BinaryData cpAddr;
   };
   std::vector<SettlCp> settlCps;
   for (int i = 0; i < metaCount; i++) {
      SettlCp cp;
      cp.payinHash = CryptoPRNG::generateRandom(32);
      cp.settlementId = CryptoPRNG::generateRandom(32);
      cp.cpAddr = CryptoPRNG::generateRandom(33);
      settlCps.push_back(cp);
      txComments.push_back(std::make_pair(cp.payinHash, CryptoPRNG::generateRandom(20).toHexStr()));
   }

   struct SettlMeta
   {
      BinaryData settlementId;
      bs::Address authAddr;
   };
   std::vector<SettlMeta> settlMetas;
   for (int i = 0; i < metaCount; i++) {
      SettlMeta meta;
      meta.settlementId = CryptoPRNG::generateRandom(32);
      meta.authAddr = randomAddressPKH();
      settlMetas.push_back(meta);
   }

   const auto passphrase = SecureBinaryData::fromString("test");
   std::string fileName;
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData::fromString("test seed"), NetworkType::TestNet };
      const bs::wallet::PasswordData pd{ passphrase, { bs::wallet::EncryptionType::Password } };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, pd, walletFolder_, envPtr_->logger());

      {
         const bs::core::WalletPasswordScoped lock(walletPtr, passphrase);
         walletPtr->createStructure(10);
      }

      auto group = walletPtr->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(group != nullptr);

      const bs::hd::Path xbtPathNative({ bs::hd::Purpose::Native, walletPtr->getXBTGroupType(), 0 });
      auto leafNative = group->getLeafByPath(xbtPathNative);
      ASSERT_NE(leafNative, nullptr);
      fileName = walletPtr->getFileName();

      for (auto &settlCp : settlCps) {
         EXPECT_TRUE(leafNative->setSettlCPMeta(settlCp.payinHash, settlCp.settlementId, settlCp.cpAddr));
      }
      for (auto &settlMeta : settlMetas) {
         EXPECT_TRUE(leafNative->setSettlementMeta(settlMeta.settlementId, settlMeta.authAddr));
      }
      for (auto &addr : addrComments) {
         EXPECT_TRUE(leafNative->setAddressComment(addr.first, addr.second));
      }
      for (auto &tx : txComments) {
         EXPECT_TRUE(leafNative->setTransactionComment(tx.first, tx.second));
      }
   }

   const SecureBinaryData ctrlPass;
   {
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(fileName
         , NetworkType::TestNet, "", ctrlPass, envPtr_->logger());

      auto groupPtr = walletPtr->getGroup(bs::hd::Bitcoin_test);
      ASSERT_TRUE(groupPtr);

      const bs::hd::Path xbtPathNative({ bs::hd::Purpose::Native, walletPtr->getXBTGroupType(), 0 });
      auto leafNative = groupPtr->getLeafByPath(xbtPathNative);
      ASSERT_TRUE(leafNative);

      for (auto &settlCp : settlCps) {
         auto result = leafNative->getSettlCP(settlCp.payinHash);
         EXPECT_EQ(result.first, settlCp.settlementId);
         EXPECT_EQ(result.second, settlCp.cpAddr);
      }
      for (auto &settlMeta : settlMetas) {
         auto result = leafNative->getSettlAuthAddr(settlMeta.settlementId);
         EXPECT_EQ(result, settlMeta.authAddr);
      }
      for (auto &addr : addrComments) {
         EXPECT_EQ(leafNative->getAddressComment(addr.first), addr.second);
      }
      for (auto &tx : txComments) {
         EXPECT_EQ(leafNative->getTransactionComment(tx.first), tx.second);
      }
   }
}

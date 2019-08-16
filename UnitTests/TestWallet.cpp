#include <gtest/gtest.h>
#include <QComboBox>
#include <QDebug>
#include <QString>
#include <QLocale>
#include "ApplicationSettings.h"
#include "CoreHDLeaf.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "SystemFileUtils.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletEncryption.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BIP32_Node.h"

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
class TestWallet : public ::testing::Test
{
   void SetUp()
   {
      envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
      passphrase_ = SecureBinaryData("pass");
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

TEST_F(TestWallet, BIP44_derivation)
{
   SecureBinaryData seed("test seed");
   SecureBinaryData passphrase("passphrase");
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ SecureBinaryData("test seed"), NetworkType::TestNet },
      passphrase, walletFolder_);
   ASSERT_NE(wallet, nullptr);

   {
      auto lock = wallet->lockForEncryption(passphrase);
      wallet->createStructure();
      ASSERT_NE(wallet->getGroup(wallet->getXBTGroupType()), nullptr);
   }

   const auto grpXbt = wallet->getGroup(wallet->getXBTGroupType());
   ASSERT_NE(grpXbt, nullptr);

   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, wallet->getXBTGroupType(), 0 });
   const auto leafXbt = grpXbt->getLeafByPath(xbtPath);
   EXPECT_NE(leafXbt, nullptr);

   auto val = leafXbt->getExtAddressCount();

   BIP32_Node node;
   node.initFromSeed(seed);
   std::vector<unsigned> derPath = {
      0x8000002c, //44' 
      0x80000001, //1'
      0x80000000, //0'
      0, 8
   };

   for (auto& derInt : derPath)
      node.derivePrivate(derInt);

   auto pathstr = leafXbt->name();
   auto addrObj = leafXbt->getAddressByIndex(8, true);
   auto pubkeyHash = BtcUtils::getHash160(node.getPublicKey());
   EXPECT_EQ(addrObj.unprefixed(), pubkeyHash);

   ASSERT_TRUE(wallet->eraseFile());
}

TEST_F(TestWallet, BIP44_primary)
{
   SecureBinaryData passphrase("passphrase");
   SecureBinaryData wrongPass("wrongPass");
   ASSERT_NE(envPtr_->walletsMgr(), nullptr);

   const bs::core::wallet::Seed seed{ SecureBinaryData("Sample test seed")
      , NetworkType::TestNet };
   auto coreWallet = envPtr_->walletsMgr()->createWallet("primary", "test"
      , seed, walletFolder_, passphrase, true);
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
   EXPECT_EQ(leafXbt->name(), "44'/1'/0'");
   EXPECT_EQ(leafXbt->getRootId().toHexStr(), "64134dca");

   EXPECT_THROW(grpXbt->createLeaf(AddressEntryType_Default, 0), std::exception);

   {
      auto lock = wallet->lockForEncryption(wrongPass);
      EXPECT_THROW(grpXbt->createLeaf(AddressEntryType_Default, 1, 10), std::exception);
      EXPECT_EQ(grpXbt->getNumLeaves(), 2);
   }

   {
      auto lock = wallet->lockForEncryption(passphrase);
      const auto leaf1 = grpXbt->createLeaf(AddressEntryType_P2WPKH, 1, 10);
      ASSERT_NE(leaf1, nullptr);
      EXPECT_EQ(grpXbt->getNumLeaves(), 3);
      EXPECT_EQ(leaf1->shortName(), "1'");
      EXPECT_EQ(leaf1->name(), "44'/1'/1'");
      //EXPECT_EQ(leaf1->description(), "test");
      EXPECT_TRUE(envPtr_->walletsMgr()->deleteWalletFile(leaf1));
      EXPECT_EQ(grpXbt->getNumLeaves(), 2);

      const auto grpCC = wallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
      const auto leafCC = grpCC->createLeaf(AddressEntryType_P2WPKH, 7568, 10);
      EXPECT_EQ(leafCC->name(), "44'/16979'/7568'"); //16979 == 0x4253
   }

   auto inprocSigner = std::make_shared<InprocSigner>(
      envPtr_->walletsMgr(), envPtr_->logger(), "", NetworkType::TestNet);
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncXbtLeaf = syncMgr->getWalletById(leafXbt->walletId());
   EXPECT_EQ(syncXbtLeaf->name(), "primary/XBT [TESTNET]/0'");

   QComboBox cbox;
   UiUtils::fillWalletsComboBox(&cbox, syncMgr);
   EXPECT_EQ(cbox.count(), 2);
//   EXPECT_EQ(cbox.currentText().toStdString(), syncXbtLeaf->name());

   EXPECT_TRUE(envPtr_->walletsMgr()->deleteWalletFile(wallet));
   EXPECT_EQ(envPtr_->walletsMgr()->getPrimaryWallet(), nullptr);
}

TEST_F(TestWallet, BIP44_address)
{
   SecureBinaryData passphrase("passphrase");
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ SecureBinaryData("test seed"), NetworkType::TestNet },
      passphrase, walletFolder_);
   ASSERT_NE(wallet, nullptr);

   auto lock = wallet->lockForEncryption(passphrase);
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(AddressEntryType_Default, 0, 10);
   ASSERT_NE(leaf, nullptr);
   EXPECT_EQ(leaf->getUsedAddressCount(), 0);

   const auto addr = leaf->getNewExtAddress();
   EXPECT_EQ(addr.display(), "tb1qyss0ws75vn3fdqpvgeht6jwj3vas6s46dpv46g");
   EXPECT_EQ(leaf->getUsedAddressCount(), 1);

   const auto chgAddr = leaf->getNewChangeAddress();
   EXPECT_EQ(chgAddr.display(), "tb1q7p4fdj9prly96qg3aq97v627q6cstwlze2jh3g");
   EXPECT_EQ(leaf->getUsedAddressCount(), 2);

   EXPECT_TRUE(wallet->eraseFile());
}

TEST_F(TestWallet, BIP44_WatchingOnly)
{
   SecureBinaryData passphrase("passphrase");
   const size_t nbAddresses = 10;
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ SecureBinaryData("test seed"), NetworkType::TestNet},
      passphrase, walletFolder_);
   ASSERT_NE(wallet, nullptr);
   EXPECT_FALSE(wallet->isWatchingOnly());
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   std::shared_ptr<bs::core::hd::Leaf> leaf1;
   {
      auto lock = wallet->lockForEncryption(passphrase);
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
   SecureBinaryData passphrase("test");
   const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
   bs::core::hd::Wallet wallet1("test1", "", seed, passphrase, walletFolder_, envPtr_->logger());
   wallet1.setExtOnly();

   std::shared_ptr<bs::core::hd::Leaf> leaf1;
   {
      auto lock = wallet1.lockForEncryption(passphrase);

      auto grp1 = wallet1.createGroup(wallet1.getXBTGroupType());
      ASSERT_NE(grp1, nullptr);

      leaf1 = grp1->createLeaf(AddressEntryType_Default, 0);
      ASSERT_NE(leaf1, nullptr);
      EXPECT_TRUE(leaf1->hasExtOnlyAddresses());
   }

   const auto addr1 = leaf1->getNewChangeAddress();
   const auto index1 = leaf1->getAddressIndex(addr1);
   EXPECT_EQ(index1, "0/0");

   const bs::core::wallet::Seed seed2{ SecureBinaryData("test seed 2"), NetworkType::TestNet };
   bs::core::hd::Wallet wallet2("test2", "", seed2, passphrase, walletFolder_, envPtr_->logger());

   std::shared_ptr<bs::core::hd::Leaf> leaf2;
   {
      auto lock = wallet2.lockForEncryption(passphrase);

      auto grp2 = wallet2.createGroup(wallet2.getXBTGroupType());
      ASSERT_NE(grp2, nullptr);

      leaf2 = grp2->createLeaf(AddressEntryType_Default, 0);
      ASSERT_NE(leaf2, nullptr);
      EXPECT_FALSE(leaf2->hasExtOnlyAddresses());
   }

   const auto addr2 = leaf2->getNewChangeAddress();
   const auto index2 = leaf2->getAddressIndex(addr2);
   EXPECT_EQ(index2, "1/0");
   EXPECT_NE(addr1, addr2);

   EXPECT_TRUE(wallet1.eraseFile());
   EXPECT_TRUE(wallet2.eraseFile());
}

TEST_F(TestWallet, CreateDestroyLoad)
{
   //setup bip32 node
   BIP32_Node baseNodeNative, baseNodeNested;
   baseNodeNative.initFromSeed(SecureBinaryData("test seed"));
   baseNodeNested.initFromSeed(SecureBinaryData("test seed"));

   std::vector<bs::Address> extAddrVecNative, extAddrVecNested;
   std::vector<bs::Address> intAddrVec;
   std::set<BinaryData> grabbedAddrHash;

   std::string filename, woFilename;

   SecureBinaryData passphrase("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase, walletFolder_, envPtr_->logger());

      {
         auto lock = walletPtr->lockForEncryption(passphrase);
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
         0x8000002c, //44'
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
         usedAddrHash.insert(addr.unprefixed());
      }

      grabbedAddrHash.insert(extAddrVecNative.begin(), extAddrVecNative.end());
      grabbedAddrHash.insert(intAddrVec.begin(), intAddrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash.size(), 10);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when this scope exits
      filename = walletPtr->getFileName();
   }

   {
      //load from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", envPtr_->logger());
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
         usedAddrHash.insert(addr.unprefixed());
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
         woAddrHash.insert(addr.unprefixed());

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
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         woFilename, NetworkType::TestNet, "", envPtr_->logger());

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
         usedAddrHash.insert(addr.unprefixed());

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
   SecureBinaryData passphrase("test");
   std::string filename;

   std::vector<bs::Address> extAddrVecNative, extAddrVecNested;
   std::vector<bs::Address> intAddrVec;

   //bip32 derived counterpart
   BIP32_Node base_node;
   base_node.initFromSeed(SecureBinaryData("test seed"));

   std::vector<unsigned> derPath = {
      0x8000002c, //44' 
      0x80000001, //1'
      0x80000000  //0'
   };

   for (auto& path : derPath) {
      base_node.derivePrivate(path);
   }
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });

   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase, walletFolder_, envPtr_->logger());

      {
         auto lock = walletPtr->lockForEncryption(passphrase);
         walletPtr->createStructure(10);
      }

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, envPtr_->logger());
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
      syncWallet->syncAddresses();

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
         filename, NetworkType::TestNet, "", envPtr_->logger());

      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, envPtr_->logger());
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
      syncWallet->syncAddresses();

      //create WO copy
      auto WOcopy = walletPtr->createWatchingOnly();
      filename = WOcopy->getFileName();

      //scope out to clean up prior to WO testing
   }

   //load WO, perform checks one last time
   {
      //reload wallet
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", envPtr_->logger());

      EXPECT_EQ(walletPtr->isWatchingOnly(), true);

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, envPtr_->logger());
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
   base_node.initFromSeed(SecureBinaryData("test seed"));

   std::vector<bs::Address> extAddrVec;
   std::set<BinaryData> grabbedAddrHash;

   std::string filename, woFilename;
   auto&& salt = CryptoPRNG::generateRandom(32);

   SecureBinaryData passphrase("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase, walletFolder_, envPtr_->logger());

      auto group = walletPtr->createGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(group != nullptr);

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      ASSERT_TRUE(authGroup != nullptr);
      authGroup->setSalt(salt);

      std::shared_ptr<bs::core::hd::Leaf> leafPtr;

      {
         auto lock = walletPtr->lockForEncryption(passphrase);
         leafPtr = group->createLeaf(AddressEntryType_Default, 0x800000b1, 10);
         ASSERT_TRUE(leafPtr != nullptr);
         ASSERT_TRUE(leafPtr->hasExtOnlyAddresses());
      }

      auto authLeafPtr = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leafPtr);
      ASSERT_TRUE(authLeafPtr != nullptr);
      ASSERT_EQ(authLeafPtr->getSalt(), salt);

      //reproduce the keys as bip32 nodes
      std::vector<unsigned> derPath = {
         0x8000002c, //44' 
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
         usedAddrHash.insert(addr.unprefixed());

      grabbedAddrHash.insert(extAddrVec.begin(), extAddrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 5);
      EXPECT_EQ(usedAddrHash.size(), 5);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when on scope out
      filename = walletPtr->getFileName();
   }

   const bs::hd::Path authPath({ bs::hd::Purpose::Native, bs::hd::BlockSettle_Auth, 0xb1 });
   {
      //load from file
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", envPtr_->logger());

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
         usedAddrHash.insert(addr.unprefixed());

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
         EXPECT_EQ(addr_hash, newAddr);

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
         woAddrHash.insert(addr.unprefixed());

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
         woFilename, NetworkType::TestNet, "", envPtr_->logger());

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
         usedAddrHash.insert(addr.unprefixed());

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
         EXPECT_EQ(addr_hash, newAddr);
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

   SecureBinaryData passphrase("test");
   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase, walletFolder_, envPtr_->logger());

      auto group = walletPtr->createGroup(bs::hd::BlockSettle_Auth);
      ASSERT_TRUE(group != nullptr);

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      ASSERT_TRUE(authGroup != nullptr);
      authGroup->setSalt(salt);

      std::shared_ptr<bs::core::hd::Leaf> leafPtr;

      {
         auto lock = walletPtr->lockForEncryption(passphrase);
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
         auto lock = walletPtr->lockForEncryption(passphrase);
         
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
         usedAddrHash.insert(addr.unprefixed());

      grabbedAddrHash.insert(addrVec.begin(), addrVec.end());

      ASSERT_EQ(grabbedAddrHash.size(), 7);
      EXPECT_EQ(usedAddrHash.size(), 7);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);

      //wallet object will be destroyed when on scope out
      filename = walletPtr->getFileName();
   }

   //reload
   const bs::hd::Path settlPath({ bs::hd::Purpose::Native, bs::hd::BlockSettle_Settlement, 0 });
   {
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         filename, NetworkType::TestNet, "", StaticLogger::loggerPtr);

      //grab settlement group
      auto group = walletPtr->getGroup(bs::hd::BlockSettle_Settlement);
      ASSERT_NE(group, nullptr);

      //get settlement leaf from auth address path

      auto settlLeaf = group->getLeafByPath(settlPath);
      ASSERT_NE(settlLeaf, nullptr);

      auto usedAddrList = settlLeaf->getUsedAddressList();
      std::set<BinaryData> usedAddrHash;
      for (auto& addr : usedAddrList)
         usedAddrHash.insert(addr.unprefixed());

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
         filename, NetworkType::TestNet, "", StaticLogger::loggerPtr);

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
         usedAddrHash.insert(addr.unprefixed());

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
         filename, NetworkType::TestNet, "", StaticLogger::loggerPtr);

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
         usedAddrHash.insert(addr.unprefixed());

      EXPECT_EQ(usedAddrHash.size(), 13);
      EXPECT_EQ(usedAddrHash, grabbedAddrHash);
   }
}

TEST_F(TestWallet, SyncWallet_TriggerPoolExtension)
{
   SecureBinaryData passphrase("test");
   std::string filename;

   std::vector<bs::Address> extAddrVec;
   std::vector<bs::Address> intAddrVec;

   //bip32 derived counterpart
   BIP32_Node base_node;
   base_node.initFromSeed(SecureBinaryData("test seed"));

   std::vector<unsigned> derPath = {
      0x8000002c, //44' 
      0x80000001, //1'
      0x80000000  //0'
   };
   for (auto& path : derPath) {
      base_node.derivePrivate(path);
   }
   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, bs::hd::Bitcoin_test, 0 });

   {
      //create a wallet
      const bs::core::wallet::Seed seed{ SecureBinaryData("test seed"), NetworkType::TestNet };
      auto walletPtr = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase, walletFolder_, envPtr_->logger());

      {
         auto lock = walletPtr->lockForEncryption(passphrase);
         walletPtr->createStructure(10);
      }

      //create sync manager
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr, envPtr_->logger());
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
   SecureBinaryData passphrase("test");

   bs::core::wallet::Seed seed{ CryptoPRNG::generateRandom(32), NetworkType::TestNet };
   ASSERT_EQ(seed.seed().getSize(), 32);

   std::string filename, leaf1Id;
   bs::Address addr1;

   EasyCoDec::Data easySeed;

   {
      std::shared_ptr<bs::core::hd::Leaf> leaf1;

      auto wallet1 = std::make_shared<bs::core::hd::Wallet>(
         "test1", "", seed, passphrase, walletFolder_, nullptr);
      auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
      {
         auto lock = wallet1->lockForEncryption(passphrase);
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
         auto lock = wallet1->lockForEncryption(passphrase);
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
         "test2", "", seedRestored, passphrase, walletFolder_, nullptr);
      auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());

      //check leaf id and addr data
      std::shared_ptr<bs::core::hd::Leaf> leaf2;
      bs::Address addr2;
      {
         auto lock = wallet2->lockForEncryption(passphrase);
         leaf2 = grp2->createLeaf(AddressEntryType_Default, 0u);
         addr2 = leaf2->getNewExtAddress();
      }

      EXPECT_EQ(leaf1Id, leaf2->walletId());
      EXPECT_EQ(addr1, addr2);

      //check seeds again
      std::shared_ptr<bs::core::wallet::Seed> seed2;
      try
      {
         auto lock = wallet2->lockForEncryption(passphrase);
         seed2 = std::make_shared<bs::core::wallet::Seed>(
            wallet2->getDecryptedSeed());
      }
      catch (...)
      {
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
      auto lock = wallet3->lockForEncryption(passphrase);
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
   SecureBinaryData passphrase("test");

   bs::core::wallet::Seed seed{ CryptoPRNG::generateRandom(32), NetworkType::TestNet };
   ASSERT_EQ(seed.seed().getSize(), 32);

   std::string filename, leaf1Id;
   bs::Address addr1;

   SecureBinaryData xpriv;

   {
      std::shared_ptr<bs::core::hd::Leaf> leaf1;

      auto wallet1 = std::make_shared<bs::core::hd::Wallet>(
         "test1", "", seed, passphrase, walletFolder_, nullptr);
      auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
      {
         auto lock = wallet1->lockForEncryption(passphrase);
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
         auto lock = wallet1->lockForEncryption(passphrase);
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
         "test2", "", seedRestored, passphrase, walletFolder_, nullptr);
      auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());

      //check leaf id and addr data
      std::shared_ptr<bs::core::hd::Leaf> leaf2;
      bs::Address addr2;
      {
         auto lock = wallet2->lockForEncryption(passphrase);
         leaf2 = grp2->createLeaf(AddressEntryType_Default, 0u);
         addr2 = leaf2->getNewExtAddress();
      }

      EXPECT_EQ(leaf1Id, leaf2->walletId());
      EXPECT_EQ(addr1, addr2);

      //check restoring from xpriv yields no seed
      try
      {
         auto lock = wallet2->lockForEncryption(passphrase);
         auto seed2 = std::make_shared<bs::core::wallet::Seed>(
            wallet2->getDecryptedSeed());
         ASSERT_TRUE(false);
      }
      catch (WalletException&)
      {}

      //shut it all down, reload, check seeds again
      filename = wallet2->getFileName();
   }

   auto wallet3 = std::make_shared<bs::core::hd::Wallet>(
      filename, NetworkType::TestNet);
   auto grp3 = wallet3->getGroup(wallet3->getXBTGroupType());

   //there still shouldnt be a seed to grab
   try
   {
      auto lock = wallet3->lockForEncryption(passphrase);
      auto seed3 = std::make_shared<bs::core::wallet::Seed>(
         wallet3->getDecryptedSeed());
      ASSERT_TRUE(false);
   }
   catch (...)
   {}

   //grab root
   {
      auto lock = wallet3->lockForEncryption(passphrase);
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


////////////////////////////////////////////////////////////////////////////////
class TestWalletWithArmory : public ::testing::Test
{
protected:
   void SetUp()
   {
      UnitTestWalletACT::clear();

      envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
      envPtr_->requireArmory();

      passphrase_ = SecureBinaryData("pass");
      bs::core::wallet::Seed seed{ 
         SecureBinaryData("dem seeds"), NetworkType::TestNet };

      walletPtr_ = std::make_shared<bs::core::hd::Wallet>(
         "test", "", seed, passphrase_, 
         envPtr_->armoryInstance()->homedir_);

      auto grp = walletPtr_->createGroup(walletPtr_->getXBTGroupType());
      {
         auto lock = walletPtr_->lockForEncryption(passphrase_);
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
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
   auto recipient = addrVec[10].getRecipient((uint64_t)(50 * COIN));
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   /***
   mine some coins to original address set to make sure they
   dont get unregistered by the new addresses registration
   process
   ***/

   curHeight = envPtr_->armoryConnection()->topBlock();
   recipient = addrVec[0].getRecipient((uint64_t)(50 * COIN));
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

   auto leaf = leafPtr_;
   auto pass = passphrase_;
   const auto &cbTxOutList =
      [this, leaf, syncLeaf, addrVec, promPtr1]
   (std::vector<UTXO> inputs)->void
   {
      const auto recipient = addrVec[11].getRecipient((uint64_t)(25 * COIN));
      const auto txReq = syncLeaf->createTXRequest(inputs, { recipient });
      BinaryData txWrongSigned;
      {
         auto lock = leaf->lockForEncryption(SecureBinaryData{"wrongPass"});
         EXPECT_THROW(txWrongSigned = leaf->signTXRequest(txReq), std::exception);
         EXPECT_TRUE(txWrongSigned.isNull());
      }
      BinaryData txSigned;
      {
         auto lock = leaf->lockForEncryption(passphrase_);
         txSigned = leaf->signTXRequest(txReq);
         ASSERT_FALSE(txSigned.isNull());
      }
      EXPECT_NE(txWrongSigned, txSigned);

      Tx txObj(txSigned);
      envPtr_->armoryInstance()->pushZC(txSigned);

      auto&& zcVec = UnitTestWalletACT::waitOnZC();
      promPtr1->set_value(zcVec.size() == 1);
      EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());
   };

   //async, has to wait
   syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
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
}

TEST_F(TestWalletWithArmory, RestoreWallet_CheckChainLength)
{
   std::shared_ptr<bs::core::wallet::Seed> seed;
   std::vector<bs::Address> extVec;
   std::vector<bs::Address> intVec;

   {
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
      auto recipient = extVec[12].getRecipient((uint64_t)(50 * COIN));
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

         const auto recipient = extVec[13].getRecipient((uint64_t)(25 * COIN));
         const auto txReq = syncLeaf->createTXRequest(
            utxos, { recipient }, 0, false, intVec[41]);

         BinaryData txSigned;
         {
            auto lock = leaf->lockForEncryption(passphrase_);
            txSigned = leaf->signTXRequest(txReq);
            ASSERT_FALSE(txSigned.isNull());
         }

         Tx txObj(txSigned);
         envPtr_->armoryInstance()->pushZC(txSigned);

         auto&& zcVec = UnitTestWalletACT::waitOnZC();
         if (zcVec.size() != 1) {
            promPtr1->set_value(false);
         }
         ASSERT_EQ(zcVec.size(), 1);
         EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

         promPtr1->set_value(true);
      };

      //async, has to wait
      syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
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
         auto lock = walletPtr_->lockForEncryption(passphrase_);
         seed = std::make_shared<bs::core::wallet::Seed>(
            walletPtr_->getDecryptedSeed());
      }

      //shutdown it all down
      leafPtr_.reset();
      walletPtr_->eraseFile();
      walletPtr_.reset();
   }

   std::string filename;

   {
      //restore wallet from seed
      walletPtr_ = std::make_shared<bs::core::hd::Wallet>(
         "test", "",
         *seed, passphrase_,
         envPtr_->armoryInstance()->homedir_);

      auto grp = walletPtr_->createGroup(walletPtr_->getXBTGroupType());
      {
         auto lock = walletPtr_->lockForEncryption(passphrase_);
         leafPtr_ = grp->createLeaf(AddressEntryType_Default, 0, 100);
      }

      //sync with db
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
      auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
   }
}

TEST_F(TestWalletWithArmory, Comments)
{
   const std::string addrComment("Test address comment");
   const std::string txComment("Test TX comment");

   auto addr = leafPtr_->getNewExtAddress();
   ASSERT_FALSE(addr.isNull());

   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncHdWallet = syncMgr->getHDWalletById(walletPtr_->walletId());
   auto syncWallet = syncMgr->getWalletById(leafPtr_->walletId());
   
   syncHdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   EXPECT_TRUE(syncWallet->setAddressComment(addr, addrComment));
   EXPECT_EQ(leafPtr_->getAddressComment(addr), addrComment);

   //mine some coins to our wallet
   auto armoryInstance = envPtr_->armoryInstance();
   unsigned blockCount = 7;

   const auto &curHeight = envPtr_->armoryConnection()->topBlock();
   auto recipient = addr.getRecipient((uint64_t)(50 * COIN));
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   //create tx from those fresh utxos, set a comment by tx hash and check it
   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();

   auto wallet = leafPtr_;
   auto passphrase = passphrase_;
   auto cbTxOutList = [wallet, syncWallet, addr, txComment, promPtr, passphrase]
      (std::vector<UTXO> inputs)->void
   {
      if (inputs.empty()) {
         promPtr->set_value(false);
      }
      ASSERT_FALSE(inputs.empty());
      const auto recip = addr.getRecipient((uint64_t)12000);
      const auto txReq = syncWallet->createTXRequest(inputs, { recip }, 345);

      BinaryData txData;
      {
         auto lock = wallet->lockForEncryption(passphrase);
         txData = wallet->signTXRequest(txReq);
      }

      ASSERT_FALSE(txData.isNull());
      EXPECT_TRUE(syncWallet->setTransactionComment(txData, txComment));
      Tx tx(txData);
      EXPECT_TRUE(tx.isInitialized());
      EXPECT_EQ(wallet->getTransactionComment(tx.getThisHash()), txComment);
      promPtr->set_value(true);
   };

   EXPECT_TRUE(syncWallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX));
   EXPECT_TRUE(fut.get());
}

TEST_F(TestWalletWithArmory, ZCBalance)
{
   const auto addr1 = leafPtr_->getNewExtAddress();
   const auto addr2 = leafPtr_->getNewExtAddress();
   const auto changeAddr = leafPtr_->getNewChangeAddress();
   EXPECT_EQ(leafPtr_->getUsedAddressCount(), 3);

   //add an extra address not part of the wallet
   bs::Address otherAddr(
      READHEX("0000000000000000000000000000000000000000"), 
      AddressEntryType_P2PKH);

   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
   auto recipient = addr1.getRecipient((uint64_t)(50 * COIN));
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
   auto promPtr1 = std::make_shared<std::promise<bool>>();
   auto fut1 = promPtr1->get_future();

   auto leaf = leafPtr_;
   auto pass = passphrase_;
   const auto &cbTxOutList = 
      [this, leaf, syncLeaf, changeAddr, addr2, otherAddr,
       amount, fee, pass, promPtr1]
      (std::vector<UTXO> inputs)->void
   {
      if (inputs.empty()) {
         promPtr1->set_value(false);
      }
      ASSERT_GE(inputs.size(), 1);

      //pick a single input
      std::vector<UTXO> utxos;
      utxos.push_back(inputs[0]);

      const auto recipient = addr2.getRecipient(amount);
      const auto recipient2 = otherAddr.getRecipient(amount);
      const auto txReq = syncLeaf->createTXRequest(
         utxos, { recipient, recipient2 }, fee, false, changeAddr);
      BinaryData txSigned;
      {
         auto lock = leaf->lockForEncryption(pass);
         txSigned = leaf->signTXRequest(txReq);
         ASSERT_FALSE(txSigned.isNull());
      }

      Tx txObj(txSigned);
      envPtr_->armoryInstance()->pushZC(txSigned);

      auto&& zcVec = UnitTestWalletACT::waitOnZC();
      promPtr1->set_value(zcVec.size() == 1);
      EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());
   };
   
   //async, has to wait
   syncLeaf->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
   ASSERT_TRUE(fut1.get());

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
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   auto promPtr4 = std::make_shared<std::promise<bool>>();
   auto fut4 = promPtr4->get_future();
   const auto &cbBalance4 = [promPtr4](void)
   {
      promPtr4->set_value(true);
   };

   //async, has to wait
   syncLeaf->updateBalances(cbBalance4);
   fut4.wait();

   EXPECT_EQ(syncLeaf->getTotalBalance(),
      double(350 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider);
   EXPECT_EQ(syncLeaf->getSpendableBalance(),
      double(350 * COIN - amount - fee) / BTCNumericTypes::BalanceDivider - syncLeaf->getUnconfirmedBalance());
   EXPECT_EQ(syncLeaf->getUnconfirmedBalance(), 
      double(5 * COIN) / BTCNumericTypes::BalanceDivider);
}

TEST_F(TestWalletWithArmory, SimpleTX_bech32)
{
   const auto addr1 = leafPtr_->getNewExtAddress();
   const auto addr2 = leafPtr_->getNewExtAddress();
   const auto addr3 = leafPtr_->getNewExtAddress();
   const auto changeAddr = leafPtr_->getNewChangeAddress();
   EXPECT_EQ(leafPtr_->getUsedAddressCount(), 4);

   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
   auto recipient = addr1.getRecipient((uint64_t)(50 * COIN));
   armoryInstance->mineNewBlock(recipient.get(), blockCount);
   auto newTop = UnitTestWalletACT::waitOnNewBlock();
   ASSERT_EQ(curHeight + blockCount, newTop);

   const uint64_t amount1 = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   const auto &cbTX = [](bool result) {
      ASSERT_TRUE(result);
   };

   auto promPtr1 = std::make_shared<std::promise<bool>>();
   auto fut1 = promPtr1->get_future();
   const auto &cbTxOutList1 = 
      [this, syncLeaf, addr2, changeAddr, amount1, fee, cbTX, promPtr1]
      (std::vector<UTXO> inputs1) 
   {
      const auto recipient1 = addr2.getRecipient(amount1);
      ASSERT_NE(recipient1, nullptr);
      const auto txReq1 = syncLeaf->createTXRequest(
         inputs1, { recipient1 }, fee, false, changeAddr);

      BinaryData txSigned1;
      {
         auto lock = leafPtr_->lockForEncryption(passphrase_);
         txSigned1 = leafPtr_->signTXRequest(txReq1);
         ASSERT_FALSE(txSigned1.isNull());
      }

      envPtr_->armoryInstance()->pushZC(txSigned1);
      Tx txObj(txSigned1);

      auto&& zcVec = UnitTestWalletACT::waitOnZC();
      promPtr1->set_value(zcVec.size() == 1);
      EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());
   };
   
   syncLeaf->getSpendableTxOutList(cbTxOutList1, UINT64_MAX);
   ASSERT_TRUE(fut1.get());

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

   auto promPtr3 = std::make_shared<std::promise<bool>>();
   auto fut3 = promPtr3->get_future();
   const auto &cbTxOutList2 = 
      [this, syncLeaf, addr3, fee, changeAddr, cbTX, promPtr3]
      (std::vector<UTXO> inputs2) 
   {
      const uint64_t amount2 = 0.04 * BTCNumericTypes::BalanceDivider;
      const auto recipient2 = addr3.getRecipient(amount2);
      ASSERT_NE(recipient2, nullptr);
      const auto txReq2 = syncLeaf->createTXRequest(
         inputs2, { recipient2 }, fee, false, changeAddr);

      BinaryData txSigned2;
      {
         auto lock = leafPtr_->lockForEncryption(passphrase_);
         txSigned2 = leafPtr_->signTXRequest(txReq2);
         ASSERT_FALSE(txSigned2.isNull());
      }

      envPtr_->armoryInstance()->pushZC(txSigned2);
      Tx txObj(txSigned2);

      auto&& zcVec = UnitTestWalletACT::waitOnZC();
      promPtr3->set_value(zcVec.size() == 1);
      EXPECT_EQ(zcVec[0].txHash, txObj.getThisHash());

   };
   syncLeaf->getSpendableTxOutList(cbTxOutList2, UINT64_MAX);
   ASSERT_TRUE(fut3.get());
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
      auto lock = walletPtr_->lockForEncryption(passphrase_);

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
   auto inprocSigner = std::make_shared<InprocSigner>(walletPtr_, envPtr_->logger());
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
   auto recipient = msAddress.getRecipient((uint64_t)(50 * COIN));
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
            settlementRootAddress.getRecipient((uint64_t)(20 * COIN)),
            msAddress.getRecipient((uint64_t)(30 * COIN))
         },
         0, false);

      //sign it
      BinaryData signedTx;
      {
         auto lock = walletPtr_->lockForEncryption(passphrase_);
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
}

#if 0
TEST(TestWallet, 1of2_SameKey)
{
   bs::core::hd::Wallet wallet1("hdWallet", "", NetworkType::TestNet, TestEnv::logger());
   const std::string email = "email@example.com";
   const std::vector<bs::wallet::PasswordData> authKeys = {
      { CryptoPRNG::generateRandom(32), bs::wallet::EncryptionType::Auth, email },
      { CryptoPRNG::generateRandom(32), bs::wallet::EncryptionType::Auth, email }
   };
   const bs::wallet::KeyRank keyRank = { 1, 2 };
   EXPECT_EQ(wallet1.changePassword(authKeys, keyRank, {}, false, false, false), true);
   ASSERT_EQ(wallet1.encryptionTypes().size(), 1);
   EXPECT_EQ(wallet1.encryptionTypes()[0], bs::wallet::EncryptionType::Auth);
   ASSERT_EQ(wallet1.encryptionKeys().size(), 1);
   EXPECT_EQ(wallet1.encryptionKeys()[0], email);
   EXPECT_EQ(wallet1.encryptionRank(), keyRank);
   EXPECT_NE(wallet1.getRootNode(authKeys[0].password), nullptr);
   EXPECT_NE(wallet1.getRootNode(authKeys[1].password), nullptr);

   const std::string filename = "m_of_n_test.lmdb";
   wallet1.saveToFile(filename);
   {
      bs::core::hd::Wallet wallet2(filename);
      ASSERT_EQ(wallet2.encryptionTypes().size(), 1);
      EXPECT_EQ(wallet2.encryptionTypes()[0], bs::wallet::EncryptionType::Auth);
      ASSERT_EQ(wallet2.encryptionKeys().size(), 1);
      EXPECT_EQ(wallet2.encryptionKeys()[0], email);
      EXPECT_EQ(wallet2.encryptionRank(), keyRank);
      EXPECT_NE(wallet2.getRootNode(authKeys[0].password), nullptr);
      EXPECT_NE(wallet2.getRootNode(authKeys[1].password), nullptr);
   }
   EXPECT_TRUE(wallet1.eraseFile());
}
#endif   //0

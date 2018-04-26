////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //            
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                   
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "TestUtils.h"

////////////////////////////////////////////////////////////////////////////////
class AddressTests : public ::testing::Test
{};

TEST_F(AddressTests, base58_Tests)
{
   BinaryData h_160 = READHEX("00010966776006953d5567439e5e39f86a0d273bee");
   BinaryData scrAddr("16UwLL9Risc3QfPqBUvKofHmBQ7wMtjvM");

   auto&& encoded = BtcUtils::scrAddrToBase58(h_160);
   EXPECT_EQ(encoded, scrAddr);

   auto&& decoded = BtcUtils::base58toScrAddr(scrAddr);
   EXPECT_EQ(decoded, h_160);
}

TEST_F(AddressTests, bech32_Tests)
{
   BinaryData pubkey =
      READHEX("0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
   BinaryData p2wpkhScrAddr("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
   BinaryData p2wshAddr("bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3");

   auto pubkey_hash = BtcUtils::getHash160(pubkey);
   auto&& scrAddr_p2wpkh = BtcUtils::scrAddrToSegWitAddress(pubkey_hash);
   EXPECT_EQ(p2wpkhScrAddr, scrAddr_p2wpkh);

   BinaryWriter bw;
   bw.put_uint8_t(pubkey.getSize());
   bw.put_BinaryData(pubkey);
   bw.put_uint8_t(OP_CHECKSIG);

   auto&& script_hash = BtcUtils::getSha256(bw.getData());
   auto&& scrAddr_p2wsh = BtcUtils::scrAddrToSegWitAddress(script_hash);
   EXPECT_EQ(p2wshAddr, scrAddr_p2wsh);

   auto&& pubkey_hash2 = BtcUtils::segWitAddressToScrAddr(scrAddr_p2wpkh);
   EXPECT_EQ(pubkey_hash, pubkey_hash2);

   auto&& script_hash2 = BtcUtils::segWitAddressToScrAddr(scrAddr_p2wsh);
   EXPECT_EQ(script_hash, script_hash2);
}

////////////////////////////////////////////////////////////////////////////////
class DerivationTests : public ::testing::Test
{
protected:
   BinaryData seed_ = READHEX("000102030405060708090a0b0c0d0e0f");
};

////////////////////////////////////////////////////////////////////////////////
TEST_F(DerivationTests, BIP32_Tests)
{
   auto&& root = CryptoECDSA::bip32_seed_to_master_root(seed_);

   //m
   {
      //priv ser & deser
      {
         string ext_prv(
            "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi");

         //ser
         BIP32_Serialization serObj(
            0, 0,
            root.first, root.second);
         EXPECT_EQ(serObj.getBase58(), ext_prv);

         //deser
         BIP32_Serialization deserObj(ext_prv);
         EXPECT_EQ(deserObj.getVersion(), BIP32_SER_VERSION_MAIN_PRV);
         EXPECT_EQ(deserObj.getDepth(), 0);
         EXPECT_EQ(deserObj.getLeafID(), 0);
         
         EXPECT_EQ(deserObj.getChaincode(), root.second);
         
         auto& privkey = deserObj.getKey();
         EXPECT_EQ(privkey.getPtr()[0], 0);
         EXPECT_EQ(privkey.getSliceCopy(1, 32), root.first);
      }

      //pub ser & deser
      {
         string ext_pub(
            "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8");

         auto&& compressed_pub = CryptoECDSA().CompressPoint(root.first);

         //ser
         BIP32_Serialization serObj(
            0, 0,
            compressed_pub, root.second);
         EXPECT_EQ(serObj.getBase58(), ext_pub);

         //deser
         BIP32_Serialization deserObj(ext_pub);
         EXPECT_EQ(deserObj.getVersion(), BIP32_SER_VERSION_MAIN_PUB);
         EXPECT_EQ(deserObj.getDepth(), 0);
         EXPECT_EQ(deserObj.getLeafID(), 0);

         EXPECT_EQ(deserObj.getChaincode(), root.second);
         EXPECT_EQ(deserObj.getKey(), compressed_pub);
      }
   }

   //m/0'
   {
      auto&& node = CryptoECDSA::bip32_derive_private_key(
         root.first, root.second, 0x80000000);

      //priv ser & deser
      {

         string ext_prv(
            "xprv9uHRZZhk6KAJC1avXpDAp4MDc3sQKNxDiPvvkX8Br5ngLNv1TxvUxt4cV1rGL5hj6KCesnDYUhd7oWgT11eZG7XnxHrnYeSvkzY7d2bhkJ7");

         //ser
         BIP32_Serialization serObj(
            0, 0,
            node.first, node.second);
         EXPECT_EQ(serObj.getBase58(), ext_prv);

         //deser
         BIP32_Serialization deserObj(ext_prv);
         EXPECT_EQ(deserObj.getVersion(), BIP32_SER_VERSION_MAIN_PRV);
         EXPECT_EQ(deserObj.getDepth(), 1);
         EXPECT_EQ(deserObj.getLeafID(), 0);

         EXPECT_EQ(deserObj.getChaincode(), node.second);

         auto& privkey = deserObj.getKey();
         EXPECT_EQ(privkey.getPtr()[0], 0);
         EXPECT_EQ(privkey.getSliceCopy(1, 32), node.first);
      }

      //pub ser & deser
      {
         string ext_pub(
            "xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw");

         auto&& compressed_pub = CryptoECDSA().CompressPoint(node.first);

         //ser
         BIP32_Serialization serObj(
            0, 0,
            compressed_pub, node.second);
         EXPECT_EQ(serObj.getBase58(), ext_pub);

         //deser
         BIP32_Serialization deserObj(ext_pub);
         EXPECT_EQ(deserObj.getVersion(), BIP32_SER_VERSION_MAIN_PUB);
         EXPECT_EQ(deserObj.getDepth(), 1);
         EXPECT_EQ(deserObj.getLeafID(), 0);

         EXPECT_EQ(deserObj.getChaincode(), node.second);
         EXPECT_EQ(deserObj.getKey(), compressed_pub);
      }
   }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class WalletsTest : public ::testing::Test
{
protected:
   string homedir_;

   /////////////////////////////////////////////////////////////////////////////
   virtual void SetUp()
   {
      LOGDISABLESTDOUT();
      homedir_ = string("./fakehomedir");
      rmdir(homedir_);
      mkdir(homedir_);
   }

   /////////////////////////////////////////////////////////////////////////////
   virtual void TearDown(void)
   {
      rmdir(homedir_);
   }
};

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, CreateCloseOpen_Test)
{
   map<string, vector<BinaryData>> addrMap;

   //create 3 wallets
   for (unsigned i = 0; i < 3; i++)
   {
      auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
      auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
         homedir_,
         move(wltRoot), //root as a r value
         SecureBinaryData(), //empty passphrase, will use default key
         4); //set lookup computation to 4 entries

      //get AddrVec
      auto&& hashSet = assetWlt->getAddrHashSet();

      auto id = assetWlt->getID();
      auto& vec = addrMap[id];

      vec.insert(vec.end(), hashSet.begin(), hashSet.end());

      //close wallet 
      assetWlt.reset();
   }

   //load all wallets in homedir
   WalletManager wltMgr(homedir_);

   class WalletContainerEx : public WalletContainer
   {
   public:
      shared_ptr<AssetWallet> getWalletPtr(void) const
      {
         return WalletContainer::getWalletPtr();
      }
   };

   for (auto& addrVecPair : addrMap)
   {
      auto wltCtr = (WalletContainerEx*)&wltMgr.getCppWallet(addrVecPair.first);
      auto wltSingle =
         dynamic_pointer_cast<AssetWallet_Single>(wltCtr->getWalletPtr());
      ASSERT_NE(wltSingle, nullptr);

      auto&& hashSet = wltSingle->getAddrHashSet();

      vector<BinaryData> addrVec;
      addrVec.insert(addrVec.end(), hashSet.begin(), hashSet.end());

      ASSERT_EQ(addrVec, addrVecPair.second);
   }
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, CreateWOCopy_Test)
{
   //create 1 wallet from priv key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a r value
      SecureBinaryData(),
      4); //set lookup computation to 4 entries

   //get AddrVec
   auto&& hashSet = assetWlt->getAddrHashSet();

   //get pub root and chaincode
   auto pubRoot = assetWlt->getPublicRoot();
   auto chainCode = assetWlt->getArmory135Chaincode();

   //close wallet 
   assetWlt.reset();

   auto woWallet = AssetWallet_Single::createFromPublicRoot_Armory135(
      homedir_,
      pubRoot,
      chainCode,
      4);

   //get AddrVec
   auto&& hashSetWO = woWallet->getAddrHashSet();

   ASSERT_EQ(hashSet, hashSetWO);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, Encryption_Test)
{
   /* #1: check deriving from an encrypted root yield correct chain */
   //create 1 wallet from priv key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      wltRoot, //root as a r value
      SecureBinaryData(),
      4); //set lookup computation to 4 entries

   //derive private chain from root
   auto&& chaincode = BtcUtils::computeChainCode_Armory135(wltRoot);

   vector<SecureBinaryData> privateKeys;
   auto currentPrivKey = &wltRoot;

   for (int i = 0; i < 4; i++)
   {
      privateKeys.push_back(move(CryptoECDSA().ComputeChainedPrivateKey(
         *currentPrivKey, chaincode)));

      currentPrivKey = &privateKeys.back();
   }

   //compute public keys
   vector<SecureBinaryData> publicKeys;
   for (auto& privkey : privateKeys)
   {
      publicKeys.push_back(move(CryptoECDSA().ComputePublicKey(privkey)));
   }

   //compare with wallet's own
   for (int i = 0; i < 4; i++)
   {
      //grab indexes from 0 to 3
      auto assetptr = assetWlt->getMainAccountAssetForIndex(i);
      ASSERT_EQ(assetptr->getType(), AssetEntryType_Single);

      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(assetptr);
      if (asset_single == nullptr)
         throw runtime_error("unexpected assetptr type");

      auto pubkey_ptr = asset_single->getPubKey();
      ASSERT_EQ(pubkey_ptr->getUncompressedKey(), publicKeys[i]);
   }

   /* #2: check no unencrypted private keys are on disk. Incidentally,
   check public keys are, for sanity */

   //close wallet object
   auto filename = assetWlt->getFilename();
   assetWlt.reset();

   //parse file for the presence of pubkeys and absence of priv keys
   for (auto& privkey : privateKeys)
   {
      ASSERT_FALSE(TestUtils::searchFile(filename, privkey));
   }

   for (auto& pubkey : publicKeys)
   {
      ASSERT_TRUE(TestUtils::searchFile(filename, pubkey));
   }
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, LockAndExtend_Test)
{
   //create wallet from priv key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      wltRoot, //root as a r value
      SecureBinaryData(), //set passphrase to "test"
      4); //set lookup computation to 4 entries


   //derive private chain from root
   auto&& chaincode = BtcUtils::computeChainCode_Armory135(wltRoot);

   vector<SecureBinaryData> privateKeys;
   auto currentPrivKey = &wltRoot;

   for (int i = 0; i < 10; i++)
   {
      privateKeys.push_back(move(CryptoECDSA().ComputeChainedPrivateKey(
         *currentPrivKey, chaincode)));

      currentPrivKey = &privateKeys.back();
   }

   auto secondthread = [assetWlt, &privateKeys](void)->void
   {
      //lock wallet
      auto secondlock = assetWlt->lockDecryptedContainer();

      //wallet should have 10 assets, last half with only pub keys
      ASSERT_TRUE(assetWlt->getMainAccountAssetCount() == 10);

      //none of the new assets should have private keys
      for (unsigned i = 4; i < 10; i++)
      {
         auto asseti = assetWlt->getMainAccountAssetForIndex(i);
         ASSERT_FALSE(asseti->hasPrivateKey());
      }

      //grab last asset with a priv key
      auto asset3 = assetWlt->getMainAccountAssetForIndex(3);
      auto asset3_single = dynamic_pointer_cast<AssetEntry_Single>(asset3);
      if (asset3_single == nullptr)
         throw runtime_error("unexpected asset entry type");
      auto& privkey3 = assetWlt->getDecryptedValue(asset3_single->getPrivKey());

      //check privkey
      ASSERT_EQ(privkey3, privateKeys[3]);

      //extend private chain to 10 entries
      assetWlt->extendPrivateChainToIndex(assetWlt->getMainAccountID(), 9);

      //there should still be 10 assets
      ASSERT_EQ(assetWlt->getMainAccountAssetCount(), 10);

      //try to grab 10th private key
      auto asset9 = assetWlt->getMainAccountAssetForIndex(9);
      auto asset9_single = dynamic_pointer_cast<AssetEntry_Single>(asset9);
      if (asset9_single == nullptr)
         throw runtime_error("unexpected asset entry type");

      auto& privkey9 = assetWlt->getDecryptedValue(asset9_single->getPrivKey());

      //check priv key
      ASSERT_EQ(privkey9, privateKeys[9]);
   };

   thread t2;

   {
      //grab lock
      auto firstlock = assetWlt->lockDecryptedContainer();

      //start second thread
      t2 = thread(secondthread);

      //sleep for a second
      this_thread::sleep_for(chrono::seconds(1));

      //make sure there are only 4 entries
      ASSERT_EQ(assetWlt->getMainAccountAssetCount(), 4);

      //grab 4th privkey 
      auto asset3 = assetWlt->getMainAccountAssetForIndex(3);
      auto asset3_single = dynamic_pointer_cast<AssetEntry_Single>(asset3);
      if (asset3_single == nullptr)
         throw runtime_error("unexpected asset entry type");
      auto& privkey3 = assetWlt->getDecryptedValue(asset3_single->getPrivKey());

      //check privkey
      ASSERT_EQ(privkey3, privateKeys[3]);

      //extend address chain to 10 entries
      assetWlt->extendPublicChainToIndex(
         assetWlt->getMainAccountID(), 9);

      ASSERT_EQ(assetWlt->getMainAccountAssetCount(), 10);

      //none of the new assets should have private keys
      for (unsigned i = 4; i < 10; i++)
      {
         auto asseti = assetWlt->getMainAccountAssetForIndex(i);
         ASSERT_FALSE(asseti->hasPrivateKey());
      }
   }

   if (t2.joinable())
      t2.join();

   //wallet should be unlocked now
   ASSERT_FALSE(assetWlt->isDecryptedContainerLocked());

   //delete wallet, reload and check private keys are on disk and valid
   auto wltID = assetWlt->getID();
   assetWlt.reset();

   WalletManager wltMgr(homedir_);

   class WalletContainerEx : public WalletContainer
   {
   public:
      shared_ptr<AssetWallet> getWalletPtr(void) const
      {
         return WalletContainer::getWalletPtr();
      }
   };

   auto wltCtr = (WalletContainerEx*)&wltMgr.getCppWallet(wltID);
   auto wltSingle =
      dynamic_pointer_cast<AssetWallet_Single>(wltCtr->getWalletPtr());
   ASSERT_NE(wltSingle, nullptr);
   ASSERT_FALSE(wltSingle->isDecryptedContainerLocked());

   auto lastlock = wltSingle->lockDecryptedContainer();
   for (unsigned i = 0; i < 10; i++)
   {
      auto asseti = wltSingle->getMainAccountAssetForIndex(i);
      auto asseti_single = dynamic_pointer_cast<AssetEntry_Single>(asseti);
      ASSERT_NE(asseti_single, nullptr);

      auto& asseti_privkey = wltSingle->getDecryptedValue(
         asseti_single->getPrivKey());

      ASSERT_EQ(asseti_privkey, privateKeys[i]);
   }
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, WrongPassphrase_Test)
{
   //create wallet from priv key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      wltRoot, //root as a r value
      SecureBinaryData("test"), //set passphrase to "test"
      4); //set lookup computation to 4 entries

   unsigned passphraseCount = 0;
   auto badPassphrase = [&passphraseCount](const BinaryData&)->SecureBinaryData
   {
      //pass wrong passphrase once then give up
      if (passphraseCount++ > 1)
         return SecureBinaryData();
      return SecureBinaryData("bad pass");
   };

   //set passphrase lambd
   assetWlt->setPassphrasePromptLambda(badPassphrase);

   //try to decrypt with wrong passphrase
   try
   {
      auto containerLock = assetWlt->lockDecryptedContainer();
      auto asset = assetWlt->getMainAccountAssetForIndex(0);
      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset);
      if (asset_single == nullptr)
         throw runtime_error("unexpected asset entry type");

      assetWlt->getDecryptedValue(asset_single->getPrivKey());

      ASSERT_TRUE(false);
   }
   catch (DecryptedDataContainerException&)
   {
   }

   passphraseCount = 0;
   auto goodPassphrase = [&passphraseCount](const BinaryData&)->SecureBinaryData
   {
      //pass wrong passphrase once then the right one
      if (passphraseCount++ > 1)
         return SecureBinaryData("test");
      return SecureBinaryData("another bad pass");
   };

   assetWlt->setPassphrasePromptLambda(goodPassphrase);

   //try to decrypt with wrong passphrase
   try
   {
      auto&& containerLock = assetWlt->lockDecryptedContainer();
      auto asset = assetWlt->getMainAccountAssetForIndex(0);
      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset);
      if (asset_single == nullptr)
         throw runtime_error("unexpected asset entry type");

      auto& privkey = assetWlt->getDecryptedValue(asset_single->getPrivKey());

      //make sure decrypted privkey is valid
      auto&& chaincode = BtcUtils::computeChainCode_Armory135(wltRoot);
      auto&& privkey_ex =
         CryptoECDSA().ComputeChainedPrivateKey(wltRoot, chaincode);

      ASSERT_EQ(privkey, privkey_ex);
   }
   catch (DecryptedDataContainerException&)
   {
      ASSERT_TRUE(false);
   }
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(WalletsTest, ChangePassphrase_Test)
{
   //create wallet from priv key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      wltRoot, //root as a r value
      SecureBinaryData("test"), //set passphrase to "test"
      4); //set lookup computation to 4 entries

   auto&& chaincode = BtcUtils::computeChainCode_Armory135(wltRoot);
   auto&& privkey_ex =
      CryptoECDSA().ComputeChainedPrivateKey(wltRoot, chaincode);
   auto filename = assetWlt->getFilename();


   //grab all IVs and encrypted private keys
   vector<SecureBinaryData> ivVec;
   vector<SecureBinaryData> privateKeys;
   struct DecryptedDataContainerEx : public DecryptedDataContainer
   {
      const SecureBinaryData& getMasterKeyIV(void) const
      {
         auto keyIter = encryptionKeyMap_.begin();
         return keyIter->second->getIV();
      }

      const SecureBinaryData& getMasterEncryptionKey(void) const
      {
         auto keyIter = encryptionKeyMap_.begin();
         return keyIter->second->getEncryptedData();
      }
   };

   struct AssetWalletEx : public AssetWallet_Single
   {
      shared_ptr<DecryptedDataContainer> getDecryptedDataContainer(void) const
      {
         return decryptedData_;
      }
   };

   {
      auto assetWltEx = (AssetWalletEx*)assetWlt.get();
      auto decryptedDataEx =
         (DecryptedDataContainerEx*)assetWltEx->getDecryptedDataContainer().get();
      ivVec.push_back(decryptedDataEx->getMasterKeyIV());
      privateKeys.push_back(decryptedDataEx->getMasterEncryptionKey());
   }

   for (unsigned i = 0; i < 4; i++)
   {
      auto asseti = assetWlt->getMainAccountAssetForIndex(i);
      auto asseti_single = dynamic_pointer_cast<AssetEntry_Single>(asseti);
      ASSERT_NE(asseti_single, nullptr);

      ivVec.push_back(asseti_single->getPrivKey()->getIV());
      privateKeys.push_back(asseti_single->getPrivKey()->getEncryptedData());
   }

   //make sure the IVs are unique
   auto ivVecCopy = ivVec;

   while (ivVecCopy.size() > 0)
   {
      auto compare_iv = ivVecCopy.back();
      ivVecCopy.pop_back();

      for (auto& iv : ivVecCopy)
         ASSERT_NE(iv, compare_iv);
   }

   //change passphrase
   SecureBinaryData newPassphrase("new pass");

   unsigned counter = 0;
   auto passphrasePrompt = [&counter](const BinaryData&)->SecureBinaryData
   {
      if (counter++ == 0)
         return SecureBinaryData("test");
      else
         return SecureBinaryData();
   };

   {
      //set passphrase prompt lambda
      assetWlt->setPassphrasePromptLambda(passphrasePrompt);

      //change passphrase
      assetWlt->changeMasterPassphrase(newPassphrase);
   }

   //try to decrypt with new passphrase
   auto newPassphrasePrompt = [&newPassphrase](const BinaryData&)->SecureBinaryData
   {
      return newPassphrase;
   };

   {
      assetWlt->setPassphrasePromptLambda(newPassphrasePrompt);
      auto lock = assetWlt->lockDecryptedContainer();

      auto asset0 = assetWlt->getMainAccountAssetForIndex(0);
      auto asset0_single = dynamic_pointer_cast<AssetEntry_Single>(asset0);
      ASSERT_NE(asset0_single, nullptr);

      auto& decryptedKey =
         assetWlt->getDecryptedValue(asset0_single->getPrivKey());

      ASSERT_EQ(decryptedKey, privkey_ex);
   }

   //close wallet, reload
   auto walletID = assetWlt->getID();
   assetWlt.reset();

   WalletManager wltMgr(homedir_);

   class WalletContainerEx : public WalletContainer
   {
   public:
      shared_ptr<AssetWallet> getWalletPtr(void) const
      {
         return WalletContainer::getWalletPtr();
      }
   };

   auto wltCtr = (WalletContainerEx*)&wltMgr.getCppWallet(walletID);
   auto wltSingle =
      dynamic_pointer_cast<AssetWallet_Single>(wltCtr->getWalletPtr());
   ASSERT_NE(wltSingle, nullptr);
   ASSERT_FALSE(wltSingle->isDecryptedContainerLocked());

   //grab all IVs and private keys again
   vector<SecureBinaryData> newIVs;
   vector<SecureBinaryData> newPrivKeys;

   {
      auto wltSingleEx = (AssetWalletEx*)wltSingle.get();
      auto decryptedDataEx =
         (DecryptedDataContainerEx*)wltSingleEx->getDecryptedDataContainer().get();
      newIVs.push_back(decryptedDataEx->getMasterKeyIV());
      newPrivKeys.push_back(decryptedDataEx->getMasterEncryptionKey());
   }

   for (unsigned i = 0; i < 4; i++)
   {
      auto asseti = wltSingle->getMainAccountAssetForIndex(i);
      auto asseti_single = dynamic_pointer_cast<AssetEntry_Single>(asseti);
      ASSERT_NE(asseti_single, nullptr);

      newIVs.push_back(asseti_single->getPrivKey()->getIV());
      newPrivKeys.push_back(asseti_single->getPrivKey()->getEncryptedData());
   }

   //check only the master key and iv have changed, and that the new iv does 
   //not match existing ones
   ASSERT_NE(newIVs[0], ivVec[0]);
   ASSERT_NE(newPrivKeys[0], privateKeys[0]);

   for (unsigned i = 1; i < 4; i++)
   {
      ASSERT_EQ(newIVs[i], ivVec[i]);
      ASSERT_EQ(newPrivKeys[i], privateKeys[i]);

      ASSERT_NE(newIVs[0], ivVec[i]);
   }


   {
      //try to decrypt with old passphrase, should fail
      auto lock = wltSingle->lockDecryptedContainer();

      counter = 0;
      wltSingle->setPassphrasePromptLambda(passphrasePrompt);

      auto asset0 = wltSingle->getMainAccountAssetForIndex(0);
      auto asset0_single = dynamic_pointer_cast<AssetEntry_Single>(asset0);
      ASSERT_NE(asset0_single, nullptr);

      try
      {
         auto& decryptedKey =
            wltSingle->getDecryptedValue(asset0_single->getPrivKey());
         ASSERT_FALSE(true);
      }
      catch (...)
      {
      }

      //try to decrypt with new passphrase instead
      wltSingle->setPassphrasePromptLambda(newPassphrasePrompt);
      auto& decryptedKey =
         wltSingle->getDecryptedValue(asset0_single->getPrivKey());

      ASSERT_EQ(decryptedKey, privkey_ex);
   }

   //check old iv and key are not on disk anymore
   ASSERT_FALSE(TestUtils::searchFile(filename, ivVec[0]));
   ASSERT_FALSE(TestUtils::searchFile(filename, privateKeys[0]));

   ASSERT_TRUE(TestUtils::searchFile(filename, newIVs[0]));
   ASSERT_TRUE(TestUtils::searchFile(filename, newPrivKeys[0]));
}

//armory derscheme tests
//bip32 tests

//wo copy tests
//bip32 wo create test

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Now actually execute all the tests
////////////////////////////////////////////////////////////////////////////////
GTEST_API_ int main(int argc, char **argv)
{
#ifdef _MSC_VER
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

   WSADATA wsaData;
   WORD wVersion = MAKEWORD(2, 0);
   WSAStartup(wVersion, &wsaData);
#endif

   srand(time(0));
   std::cout << "Running main() from gtest_main.cc\n";

   // Setup the log file 
   STARTLOGGING("cppTestsLog.txt", LogLvlDebug2);
   //LOGDISABLESTDOUT();

   testing::InitGoogleTest(&argc, argv);
   int exitCode = RUN_ALL_TESTS();

   FLUSHLOG();
   CLEANUPLOG();

   return exitCode;
}

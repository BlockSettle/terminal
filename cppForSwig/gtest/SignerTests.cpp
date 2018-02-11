////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                                                                            //
//  Copyright (C) 2016-17, goatpig                                            //            
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                   
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "TestUtils.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class SignerTest : public ::testing::Test
{
protected:
   BlockDataManagerThread *theBDMt_ = nullptr;
   Clients* clients_ = nullptr;

   void initBDM(void)
   {
      ScrAddrFilter::init();
      theBDMt_ = new BlockDataManagerThread(config);
      iface_ = theBDMt_->bdm()->getIFace();

      auto mockedShutdown = [](void)->void {};
      clients_ = new Clients(theBDMt_, mockedShutdown);
   }

   /////////////////////////////////////////////////////////////////////////////
   virtual void SetUp()
   {
      LOGDISABLESTDOUT();
      magic_ = READHEX(MAINNET_MAGIC_BYTES);
      ghash_ = READHEX(MAINNET_GENESIS_HASH_HEX);
      gentx_ = READHEX(MAINNET_GENESIS_TX_HASH_HEX);
      zeros_ = READHEX("00000000");

      blkdir_ = string("./blkfiletest");
      homedir_ = string("./fakehomedir");
      ldbdir_ = string("./ldbtestdir");

      rmdir(blkdir_);
      rmdir(homedir_);
      rmdir(ldbdir_);

      mkdir(blkdir_);
      mkdir(homedir_);
      mkdir(ldbdir_);

      // Put the first 5 blocks into the blkdir
      blk0dat_ = BtcUtils::getBlkFilename(blkdir_, 0);
      TestUtils::setBlocks({ "0", "1", "2", "3", "4", "5" }, blk0dat_);

      BlockDataManagerConfig::setDbType(ARMORY_DB_BARE);
      config.blkFileLocation_ = blkdir_;
      config.dbDir_ = ldbdir_;
      config.threadCount_ = 3;

      config.genesisBlockHash_ = ghash_;
      config.genesisTxHash_ = gentx_;
      config.magicBytes_ = magic_;
      config.nodeType_ = Node_UnitTest;

      wallet1id = BinaryData("wallet1");
      wallet2id = BinaryData("wallet2");
      LB1ID = BinaryData(TestChain::lb1B58ID);
      LB2ID = BinaryData(TestChain::lb2B58ID);
   }

   /////////////////////////////////////////////////////////////////////////////
   virtual void TearDown(void)
   {
      if (clients_ != nullptr)
      {
         clients_->exitRequestLoop();
         clients_->shutdown();
      }

      delete clients_;
      delete theBDMt_;

      theBDMt_ = nullptr;
      clients_ = nullptr;

      rmdir(blkdir_);
      rmdir(homedir_);

#ifdef _MSC_VER
      rmdir("./ldbtestdir");
      mkdir("./ldbtestdir");
#else
      string delstr = ldbdir_ + "/*";
      rmdir(delstr);
#endif
      LOGENABLESTDOUT();
      CLEANUP_ALL_TIMERS();
   }

   BlockDataManagerConfig config;

   LMDBBlockDatabase* iface_;
   BinaryData magic_;
   BinaryData ghash_;
   BinaryData gentx_;
   BinaryData zeros_;

   string blkdir_;
   string homedir_;
   string ldbdir_;
   string blk0dat_;

   BinaryData wallet1id;
   BinaryData wallet2id;
   BinaryData LB1ID;
   BinaryData LB2ID;
};

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, CheckChain_Test)
{
   //this test fails because the p2sh tx in our unit test chain are botched
   //(the input script has opcode when it should only be push data)

   config.threadCount_ = 1;
   config.checkChain_ = true;

   BlockDataManager bdm(config);

   try
   {
      bdm.doInitialSyncOnLoad(TestUtils::nullProgress);
   }
   catch (exception&)
   {
      //signify the failure
      EXPECT_TRUE(false);
   }

   EXPECT_EQ(bdm.getCheckedTxCount(), 20);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, Signer_Test)
{
   TestUtils::setBlocks({ "0", "1", "2" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);

   //// spend 2 from wlt to scrAddrF, rest back to scrAddrA ////
   auto spendVal = 2 * COIN;
   Signer signer;

   //instantiate resolver feed overloaded object
   auto feed = make_shared<ResolverUtils::TestResolverFeed>();

   auto addToFeed = [feed](const BinaryData& key)->void
   {
      auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
      feed->h160ToPubKey_.insert(datapair);
      feed->pubKeyToPrivKey_[datapair.second] = key;
   };

   addToFeed(TestChain::privKeyAddrA);
   addToFeed(TestChain::privKeyAddrB);
   addToFeed(TestChain::privKeyAddrC);
   addToFeed(TestChain::privKeyAddrD);
   addToFeed(TestChain::privKeyAddrE);

   //get utxo list for spend value
   auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

   //create script spender objects
   auto getSpenderPtr = [feed](
      const UnspentTxOut& utxo)->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   uint64_t total = 0;
   for (auto& utxo : unspentVec)
   {
      total += utxo.getValue();
      signer.addSpender(getSpenderPtr(utxo));
   }

   //add spend to addr F, use P2PKH
   auto recipientF = make_shared<Recipient_P2PKH>(
      TestChain::scrAddrF.getSliceCopy(1, 20), spendVal);
   signer.addRecipient(recipientF);

   if (total > spendVal)
   {
      //deal with change, no fee
      auto changeVal = total - spendVal;
      auto recipientA = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrA.getSliceCopy(1, 20), changeVal);
      signer.addRecipient(recipientA);
   }

   signer.sign();
   EXPECT_TRUE(signer.verify());
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_P2PKH)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a r value
      SecureBinaryData(),
      5); //set lookup computation to 5 entries

   //register with db
   vector<BinaryData> addrVec;

   auto hashSet = assetWlt->getAddrHashSet();
   vector<BinaryData> hashVec;
   hashVec.insert(hashVec.begin(), hashSet.begin(), hashSet.end());

   DBTestUtils::regWallet(clients_, bdvID, hashVec, assetWlt->getID());
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto dbAssetWlt = bdvPtr->getWalletOrLockbox(assetWlt->getID());


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   for (auto& scripthash : hashSet)
   {
      scrObj = dbAssetWlt->getScrAddrObjByKey(scripthash);
      EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   }

   {
      ////spend 27 from wlt to assetWlt's first 2 unused addresses
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to first address
      auto addr0 = assetWlt->getNewAddress();
      signer.addRecipient(addr0->getRecipient(12 * COIN));
      addrVec.push_back(addr0->getPrefixedHash());

      //spend 15 to addr 1, use P2PKH
      auto addr1 = assetWlt->getNewAddress();
      signer.addRecipient(addr1->getRecipient(15 * COIN));
      addrVec.push_back(addr1->getPrefixedHash());

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //add op_return output for coverage
      BinaryData opreturn_msg("testing op_return");
      signer.addRecipient(make_shared<Recipient_OPRETURN>(opreturn_msg));

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);

   {
      ////spend 18 back to scrAddrB, with change to addr[2]

      auto spendVal = 18 * COIN;
      Signer signer2;

      //get utxo list for spend value
      auto&& unspentVec = dbAssetWlt->getSpendableTxOutListZC();

      //create feed from asset wallet
      auto assetFeed = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt);

      //create spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed));
      }

      //creates outputs
      //spend 18 to addr 0, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
      signer2.addRecipient(recipient2);

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         auto addr2 = assetWlt->getNewAddress();
         signer2.addRecipient(addr2->getRecipient(changeVal));
         addrVec.push_back(addr2->getPrefixedHash());
      }

      //sign, verify & broadcast
      {
         auto&& lock = assetWlt->lockDecryptedContainer();
         signer2.sign();
      }

      EXPECT_TRUE(signer2.verify());

      DBTestUtils::ZcVector zcVec2;
      zcVec2.push_back(signer2.serialize(), 15000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec2);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[2]);
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_P2WPKH)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      5); //set lookup computation to 3 entries

   //register with db
   vector<shared_ptr<AddressEntry>> addrVec;
   addrVec.push_back(assetWlt->getNewAddress(AddressEntryType_P2WPKH));
   addrVec.push_back(assetWlt->getNewAddress(AddressEntryType_P2WPKH));
   addrVec.push_back(assetWlt->getNewAddress(AddressEntryType_P2WPKH));

   vector<BinaryData> hashVec;
   for (auto addrPtr : addrVec)
      hashVec.push_back(addrPtr->getPrefixedHash());

   DBTestUtils::regWallet(clients_, bdvID, hashVec, assetWlt->getID());
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto dbAssetWlt = bdvPtr->getWalletOrLockbox(assetWlt->getID());


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   for (auto& addrPtr : addrVec)
   {
      scrObj = dbAssetWlt->getScrAddrObjByKey(addrPtr->getPrefixedHash());
      EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   }

   {
      ////spend 27 from wlt to assetWlt's first 2 unused addresses
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResovlerUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to addr0, use P2WPKH
      signer.addRecipient(addrVec[0]->getRecipient(12 * COIN));

      //spend 15 to addr1, use P2WPKH
      signer.addRecipient(addrVec[1]->getRecipient(15 * COIN));

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[2]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 18 back to scrAddrB, with change to addr2

      auto spendVal = 18 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //get utxo list for spend value
      auto&& unspentVec = dbAssetWlt->getSpendableTxOutListZC();

      //create feed from asset wallet
      auto assetFeed = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt);

      //create spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed));
      }

      //creates outputs
      //spend 18 to scrAddrB, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
      signer2.addRecipient(recipient2);

      if (total > spendVal)
      {
         //change to addr2, use P2WPKH
         auto changeVal = total - spendVal;
         auto addr2 = assetWlt->getNewAddress(AddressEntryType_P2WPKH);
         signer2.addRecipient(addrVec[2]->getRecipient(changeVal));
      }

      //sign, verify & broadcast
      {
         auto&& lock = assetWlt->lockDecryptedContainer();
         signer2.sign();
      }
      EXPECT_TRUE(signer2.verify());

      DBTestUtils::ZcVector zcVec2;
      zcVec2.push_back(signer2.serialize(), 15000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec2);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[2]->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_MultipleSigners_1of3)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create 3 assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt_1 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   wltRoot = move(SecureBinaryData().GenerateRandom(32));
   auto assetWlt_2 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   wltRoot = move(SecureBinaryData().GenerateRandom(32));
   auto assetWlt_3 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   //create 1-of-3 multisig asset entry from 3 different wallets
   map<BinaryData, shared_ptr<AssetEntry>> asset_single_map;
   auto asset1 = assetWlt_1->getMainAccountAssetForIndex(0);
   BinaryData wltid1_bd(assetWlt_1->getID());
   asset_single_map.insert(make_pair(wltid1_bd, asset1));

   auto asset2 = assetWlt_2->getMainAccountAssetForIndex(0);
   BinaryData wltid2_bd(assetWlt_2->getID());
   asset_single_map.insert(make_pair(wltid2_bd, asset2));

   auto asset3 = assetWlt_3->getMainAccountAssetForIndex(0);
   BinaryData wltid3_bd(assetWlt_3->getID());
   asset_single_map.insert(make_pair(wltid3_bd, asset3));

   auto ae_ms = make_shared<AssetEntry_Multisig>(0, BinaryData("test"),
      asset_single_map, 1, 3);
   auto addr_ms_raw = make_shared<AddressEntry_Multisig>(ae_ms, true);
   auto addr_p2wsh = make_shared<AddressEntry_P2WSH>(addr_ms_raw);
   auto addr_ms = make_shared<AddressEntry_P2SH>(addr_p2wsh);

   //register with db
   vector<BinaryData> addrVec;
   addrVec.push_back(addr_ms->getPrefixedHash());

   DBTestUtils::regWallet(clients_, bdvID, addrVec, "ms_entry");
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto ms_wlt = bdvPtr->getWalletOrLockbox(BinaryData("ms_entry"));


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 27 from wlt to ms_wlt only address
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 27 nested p2wsh script hash
      signer.addRecipient(addr_ms->getRecipient(27 * COIN));

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //add op_return output for coverage
      BinaryData opreturn_msg("testing op_return 0123");
      signer.addRecipient(make_shared<Recipient_OPRETURN>(opreturn_msg));

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 27 * COIN);

   //lambda to sign with each wallet
   auto signPerWallet = [&](shared_ptr<AssetWallet_Single> wltPtr)->BinaryData
   {
      ////spend 18 back to scrAddrB, with change to self

      auto spendVal = 18 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //get utxo list for spend value
      auto&& unspentVec =
         ms_wlt->getSpendableTxOutListZC();

      //create feed from asset wallet
      auto feed = make_shared<ResolverFeed_AssetWalletSingle_ForMultisig>(wltPtr);
      auto assetFeed = make_shared<ResolverUtils::CustomFeed>(addr_ms, feed);

      //create spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed));
      }

      //creates outputs
      //spend 18 to addr 0, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
      signer2.addRecipient(recipient2);

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         signer2.addRecipient(addr_ms->getRecipient(changeVal));
      }

      //add op_return output for coverage
      BinaryData opreturn_msg("testing op_return 0123");
      signer2.addRecipient(make_shared<Recipient_OPRETURN>(opreturn_msg));

      //sign, verify & return signed tx
      {
         auto lock = wltPtr->lockDecryptedContainer();
         signer2.sign();
      }
      EXPECT_TRUE(signer2.verify());

      return signer2.serialize();
   };

   //call lambda with each wallet
   auto&& tx1 = signPerWallet(assetWlt_1);
   auto&& tx2 = signPerWallet(assetWlt_2);
   auto&& tx3 = signPerWallet(assetWlt_3);

   //broadcast the last one
   DBTestUtils::ZcVector zcVec;
   zcVec.push_back(tx3, 15000000);

   DBTestUtils::pushNewZc(theBDMt_, zcVec);
   DBTestUtils::waitOnNewZcSignal(clients_, bdvID);

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_MultipleSigners_2of3_NativeP2WSH)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create 3 assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt_1 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   wltRoot = move(SecureBinaryData().GenerateRandom(32));
   auto assetWlt_2 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   wltRoot = move(SecureBinaryData().GenerateRandom(32));
   auto assetWlt_3 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   //create 2-of-3 multisig asset entry from 3 different wallets
   map<BinaryData, shared_ptr<AssetEntry>> asset_single_map;
   auto asset1 = assetWlt_1->getMainAccountAssetForIndex(0);
   BinaryData wltid1_bd(assetWlt_1->getID());
   asset_single_map.insert(make_pair(wltid1_bd, asset1));

   auto asset2 = assetWlt_2->getMainAccountAssetForIndex(0);
   BinaryData wltid2_bd(assetWlt_2->getID());
   asset_single_map.insert(make_pair(wltid2_bd, asset2));

   auto asset4_singlesig = assetWlt_2->getNewAddress();

   auto asset3 = assetWlt_3->getMainAccountAssetForIndex(0);
   BinaryData wltid3_bd(assetWlt_3->getID());
   asset_single_map.insert(make_pair(wltid3_bd, asset3));

   auto ae_ms = make_shared<AssetEntry_Multisig>(0, BinaryData("test"),
      asset_single_map, 2, 3);
   auto addr_ms_raw = make_shared<AddressEntry_Multisig>(ae_ms, true);
   auto addr_p2wsh = make_shared<AddressEntry_P2WSH>(addr_ms_raw);


   //register with db
   vector<BinaryData> addrVec;
   addrVec.push_back(addr_p2wsh->getPrefixedHash());

   vector<BinaryData> addrVec_singleSig;
   auto&& addrSet = assetWlt_2->getAddrHashSet();
   for (auto& addr : addrSet)
      addrVec_singleSig.push_back(addr);

   DBTestUtils::regWallet(clients_, bdvID, addrVec, "ms_entry");
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");
   DBTestUtils::regWallet(clients_, bdvID, addrVec_singleSig, assetWlt_2->getID());

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto ms_wlt = bdvPtr->getWalletOrLockbox(BinaryData("ms_entry"));
   auto wlt_singleSig = bdvPtr->getWalletOrLockbox(BinaryData(assetWlt_2->getID()));


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 27 from wlt to ms_wlt only address
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 20 to nested p2wsh script hash
      signer.addRecipient(addr_p2wsh->getRecipient(20 * COIN));

      //spend 7 to assetWlt_2
      signer.addRecipient(asset4_singlesig->getRecipient(7 * COIN));

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());
      auto&& zcHash = signer.getTxId();

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);

	   //grab ZC from DB and verify it again
      auto&& zc_from_db = DBTestUtils::getTxObjByHash(clients_, bdvID, zcHash);
      auto&& raw_tx = zc_from_db.serialize();
      auto bctx = BCTX::parse(raw_tx);
      TransactionVerifier tx_verifier(*bctx, utxoVec);

      ASSERT_TRUE(tx_verifier.evaluateState().isValid());
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 20 * COIN);
   scrObj = wlt_singleSig->getScrAddrObjByKey(asset4_singlesig->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 7 * COIN);

   auto spendVal = 18 * COIN;
   Signer signer2;
   signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

   //get utxo list for spend value
   auto&& unspentVec =
      ms_wlt->getSpendableTxOutListZC();

   auto&& unspentVec_singleSig = wlt_singleSig->getSpendableTxOutListZC();

   unspentVec.insert(unspentVec.end(),
      unspentVec_singleSig.begin(), unspentVec_singleSig.end());

   //create feed from asset wallet 1
   auto feed_ms = make_shared<ResolverFeed_AssetWalletSingle_ForMultisig>(assetWlt_1);
   auto assetFeed = make_shared<ResolverUtils::CustomFeed>(addr_p2wsh, feed_ms);

   //create spenders
   uint64_t total = 0;
   for (auto& utxo : unspentVec)
   {
      total += utxo.getValue();
      signer2.addSpender(getSpenderPtr(utxo, assetFeed));
   }

   //creates outputs
   //spend 18 to addr 0, use P2PKH
   auto recipient2 = make_shared<Recipient_P2PKH>(
      TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
   signer2.addRecipient(recipient2);

   if (total > spendVal)
   {
      //deal with change, no fee
      auto changeVal = total - spendVal;
      signer2.addRecipient(addr_p2wsh->getRecipient(changeVal));
   }

   //sign, verify & return signed tx
   auto&& signerState = signer2.evaluateSignedState();

   {
      EXPECT_EQ(signerState.getEvalMapSize(), 2);

      auto&& txinEval = signerState.getSignedStateForInput(0);
      auto& pubkeyMap = txinEval.getPubKeyMap();
      EXPECT_EQ(pubkeyMap.size(), 3);
      for (auto& pubkeyState : pubkeyMap)
         EXPECT_FALSE(pubkeyState.second);

      txinEval = signerState.getSignedStateForInput(1);
      auto& pubkeyMap_2 = txinEval.getPubKeyMap();
      EXPECT_EQ(pubkeyMap_2.size(), 0);
   }

   {
      auto lock = assetWlt_1->lockDecryptedContainer();
      signer2.sign();
   }

   try
   {
      signer2.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   {
      //signer state with 1 sig
      EXPECT_FALSE(signer2.isValid());
      signerState = signer2.evaluateSignedState();

      EXPECT_EQ(signerState.getEvalMapSize(), 2);

      auto&& txinEval = signerState.getSignedStateForInput(0);
      EXPECT_EQ(txinEval.getSigCount(), 1);

      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset1);
      ASSERT_NE(asset_single, nullptr);
      ASSERT_TRUE(txinEval.isSignedForPubKey(asset_single->getPubKey()->getCompressedKey()));
   }

   Signer signer3;
   //create feed from asset wallet 2
   auto feed_ms3 = make_shared<ResolverFeed_AssetWalletSingle_ForMultisig>(assetWlt_2);
   auto assetFeed3 = make_shared<ResolverUtils::CustomFeed>(addr_p2wsh, feed_ms3);
   signer3.deserializeState(signer2.serializeState());

   {
      //make sure sig was properly carried over with state
      EXPECT_FALSE(signer3.isValid());
      signerState = signer3.evaluateSignedState();

      EXPECT_EQ(signerState.getEvalMapSize(), 2);
      auto&& txinEval = signerState.getSignedStateForInput(0);
      EXPECT_EQ(txinEval.getSigCount(), 1);

      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset1);
      ASSERT_NE(asset_single, nullptr);
      ASSERT_TRUE(txinEval.isSignedForPubKey(asset_single->getPubKey()->getCompressedKey()));
   }

   signer3.setFeed(assetFeed3);

   {
      auto lock = assetWlt_2->lockDecryptedContainer();
      signer3.sign();
   }

   {
      auto assetFeed4 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_2);
      signer3.resetFeeds();
      signer3.setFeed(assetFeed4);
      auto lock = assetWlt_2->lockDecryptedContainer();
      signer3.sign();
   }


   ASSERT_TRUE(signer3.isValid());
   try
   {
      signer3.verify();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   {
      //should have 2 sigs now
      EXPECT_TRUE(signer3.isValid());
      signerState = signer3.evaluateSignedState();

      EXPECT_EQ(signerState.getEvalMapSize(), 2);
      auto&& txinEval = signerState.getSignedStateForInput(0);
      EXPECT_EQ(txinEval.getSigCount(), 2);

      auto asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset1);
      ASSERT_NE(asset_single, nullptr);
      ASSERT_TRUE(txinEval.isSignedForPubKey(asset_single->getPubKey()->getCompressedKey()));

      asset_single = dynamic_pointer_cast<AssetEntry_Single>(asset2);
      ASSERT_NE(asset_single, nullptr);
      ASSERT_TRUE(txinEval.isSignedForPubKey(asset_single->getPubKey()->getCompressedKey()));
   }

   auto&& tx1 = signer3.serialize();
   auto&& zcHash = signer3.getTxId();

   //broadcast the last one
   DBTestUtils::ZcVector zcVec;
   zcVec.push_back(tx1, 15000000);

   DBTestUtils::pushNewZc(theBDMt_, zcVec);
   DBTestUtils::waitOnNewZcSignal(clients_, bdvID);

   //grab ZC from DB and verify it again
   auto&& zc_from_db = DBTestUtils::getTxObjByHash(clients_, bdvID, zcHash);
   auto&& raw_tx = zc_from_db.serialize();
   auto bctx = BCTX::parse(raw_tx);
   TransactionVerifier tx_verifier(*bctx, unspentVec);

   ASSERT_TRUE(tx_verifier.evaluateState().isValid());


   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = ms_wlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
   scrObj = wlt_singleSig->getScrAddrObjByKey(asset4_singlesig->getPrefixedHash());
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_MultipleSigners_DifferentInputs)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create 2 assetWlt ////

   //create a root private key
   auto assetWlt_1 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      SecureBinaryData().GenerateRandom(32), //root as rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   auto assetWlt_2 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(SecureBinaryData().GenerateRandom(32)), //root as rvalue
      SecureBinaryData(),
      3); //set lookup computation to 3 entries

   //register with db
   vector<shared_ptr<AddressEntry>> addrVec_1;
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());

   vector<BinaryData> hashVec_1;
   for (auto addrPtr : addrVec_1)
      hashVec_1.push_back(addrPtr->getPrefixedHash());

   vector<shared_ptr<AddressEntry>> addrVec_2;
   addrVec_2.push_back(assetWlt_2->getNewAddress());
   addrVec_2.push_back(assetWlt_2->getNewAddress());
   addrVec_2.push_back(assetWlt_2->getNewAddress());

   vector<BinaryData> hashVec_2;
   for (auto addrPtr : addrVec_2)
      hashVec_2.push_back(addrPtr->getPrefixedHash());

   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");
   DBTestUtils::regWallet(clients_, bdvID, hashVec_1, assetWlt_1->getID());
   DBTestUtils::regWallet(clients_, bdvID, hashVec_2, assetWlt_2->getID());

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto wlt_1 = bdvPtr->getWalletOrLockbox(assetWlt_1->getID());
   auto wlt_2 = bdvPtr->getWalletOrLockbox(assetWlt_2->getID());

   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 12 to wlt_1, 15 to wlt_2 from wlt
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to p2pkh script hash
      signer.addRecipient(addrVec_1[0]->getRecipient(12 * COIN));

      //spend 15 to p2pkh script hash
      signer.addRecipient(addrVec_2[0]->getRecipient(15 * COIN));

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);

   //spend 18 back to wlt, split change among the 2

   //get utxo list for spend value
   auto&& unspentVec_1 =
      wlt_1->getSpendableTxOutListZC();
   auto&& unspentVec_2 =
      wlt_2->getSpendableTxOutListZC();

   BinaryData serializedSignerState;

   auto assetFeed2 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_1);
   auto assetFeed3 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_2);

   {
      auto spendVal = 8 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //create feed from asset wallet 1

      //create wlt_1 spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec_1)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed2));
      }

      //spend 18 to addrB, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), 18 * COIN);
      signer2.addRecipient(recipient2);

      //change back to wlt_1
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer2.addRecipient(addrVec_1[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer2.serializeState());
   }

   {
      //serialize signer 2, deser with signer3 and populate
      auto spendVal = 10 * COIN;
      Signer signer3;
      signer3.deserializeState(serializedSignerState);

      //add spender from wlt_2
      uint64_t total = 0;
      for (auto& utxo : unspentVec_2)
      {
         total += utxo.getValue();
         signer3.addSpender(getSpenderPtr(utxo, assetFeed3));
      }

      //set change
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer3.addRecipient(addrVec_2[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer3.serializeState());
   }


   //sign, verify & return signed tx
   Signer signer4;
   signer4.deserializeState(serializedSignerState);
   signer4.setFeed(assetFeed2);

   {
      auto lock = assetWlt_1->lockDecryptedContainer();
      signer4.sign();
   }

   try
   {
      signer4.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   EXPECT_FALSE(signer4.isValid());

   Signer signer5;
   signer5.deserializeState(signer4.serializeState());
   signer5.setFeed(assetFeed3);

   {
      auto lock = assetWlt_2->lockDecryptedContainer();
      signer5.sign();
   }

   ASSERT_TRUE(signer5.isValid());
   try
   {
      signer5.verify();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   auto&& tx1 = signer5.serialize();

   //broadcast the last one
   DBTestUtils::ZcVector zcVec;
   zcVec.push_back(tx1, 15000000);

   DBTestUtils::pushNewZc(theBDMt_, zcVec);
   DBTestUtils::waitOnNewZcSignal(clients_, bdvID);

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 4 * COIN);

   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, SpendTest_MultipleSigners_ParallelSigning)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create 2 assetWlt ////

   //create a root private key
   auto assetWlt_1 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      SecureBinaryData().GenerateRandom(32), //root as rvalue
      SecureBinaryData(), //empty passphrase
      3); //set lookup computation to 3 entries

   auto assetWlt_2 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(SecureBinaryData().GenerateRandom(32)), //root as rvalue
      SecureBinaryData(), //empty passphrase
      3); //set lookup computation to 3 entries

   //register with db
   vector<shared_ptr<AddressEntry>> addrVec_1;
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());

   vector<BinaryData> hashVec_1;
   for (auto addrPtr : addrVec_1)
      hashVec_1.push_back(addrPtr->getPrefixedHash());

   vector<shared_ptr<AddressEntry>> addrVec_2;
   addrVec_2.push_back(assetWlt_2->getNewAddress());
   addrVec_2.push_back(assetWlt_2->getNewAddress());
   addrVec_2.push_back(assetWlt_2->getNewAddress());

   vector<BinaryData> hashVec_2;
   for (auto addrPtr : addrVec_2)
      hashVec_2.push_back(addrPtr->getPrefixedHash());

   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");
   DBTestUtils::regWallet(clients_, bdvID, hashVec_1, assetWlt_1->getID());
   DBTestUtils::regWallet(clients_, bdvID, hashVec_2, assetWlt_2->getID());

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto wlt_1 = bdvPtr->getWalletOrLockbox(assetWlt_1->getID());
   auto wlt_2 = bdvPtr->getWalletOrLockbox(assetWlt_2->getID());

   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 12 to wlt_1, 15 to wlt_2 from wlt
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to p2pkh script hash
      signer.addRecipient(addrVec_1[0]->getRecipient(12 * COIN));

      //spend 15 to p2pkh script hash
      signer.addRecipient(addrVec_2[0]->getRecipient(15 * COIN));

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);

   //spend 18 back to wlt, split change among the 2

   //get utxo list for spend value
   auto&& unspentVec_1 =
      wlt_1->getSpendableTxOutListZC();
   auto&& unspentVec_2 =
      wlt_2->getSpendableTxOutListZC();

   BinaryData serializedSignerState;

   {
      //create first signer, set outpoint from wlt_1 and change to wlt_1
      auto spendVal = 8 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //create feed from asset wallet 1

      //create wlt_1 spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec_1)
      {
         total += utxo.getValue();
         signer2.addSpender(
            make_shared<ScriptSpender>(
            utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      }

      //spend 18 to addrB, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), 18 * COIN);
      signer2.addRecipient(recipient2);

      //change back to wlt_1
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer2.addRecipient(addrVec_1[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer2.serializeState());
   }

   {
      //serialize signer 2, deser with signer3 and populate with outpoint and 
      //change from wlt_2
      auto spendVal = 10 * COIN;
      Signer signer3;
      signer3.deserializeState(serializedSignerState);

      //add spender from wlt_2
      uint64_t total = 0;
      for (auto& utxo : unspentVec_2)
      {
         total += utxo.getValue();
         signer3.addSpender(
            make_shared<ScriptSpender>(
            utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      }

      //set change
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer3.addRecipient(addrVec_2[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer3.serializeState());
   }

   auto assetFeed2 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_1);
   auto assetFeed3 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_2);

   //deser to new signer, this time populate with feed and utxo from wlt_1
   Signer signer4;
   for (auto& utxo : unspentVec_1)
   {
      signer4.addSpender(getSpenderPtr(utxo, assetFeed2));
   }

   signer4.deserializeState(serializedSignerState);

   {
      auto lock = assetWlt_1->lockDecryptedContainer();
      signer4.sign();
   }

   try
   {
      signer4.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   EXPECT_FALSE(signer4.isValid());

   //deser from same state into wlt_2 signer
   Signer signer5;

   //in this case, we can't set the utxos first then deser the state, as it would break
   //utxo ordering. we have to deser first, then populate utxos
   signer5.deserializeState(serializedSignerState);

   for (auto& utxo : unspentVec_2)
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      signer5.populateUtxo(entry);
   }

   //finally set the feed
   signer5.setFeed(assetFeed3);

   {
      auto lock = assetWlt_2->lockDecryptedContainer();
      signer5.sign();
   }

   try
   {
      signer5.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   //now serialize both signers into the final signer, verify and broadcast
   Signer signer6;
   signer6.deserializeState(signer4.serializeState());
   signer6.deserializeState(signer5.serializeState());

   ASSERT_TRUE(signer6.isValid());
   try
   {
      signer6.verify();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   //try again in the opposite order, that should not matter
   Signer signer7;
   signer7.deserializeState(signer5.serializeState());
   signer7.deserializeState(signer4.serializeState());

   ASSERT_TRUE(signer7.isValid());
   try
   {
      signer7.verify();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   auto&& tx1 = signer7.serialize();

   //broadcast the last one
   DBTestUtils::ZcVector zcVec;
   zcVec.push_back(tx1, 15000000);

   DBTestUtils::pushNewZc(theBDMt_, zcVec);
   DBTestUtils::waitOnNewZcSignal(clients_, bdvID);

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 4 * COIN);

   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, GetUnsignedTxId)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create 2 assetWlt ////

   //create a root private key
   auto assetWlt_1 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      SecureBinaryData().GenerateRandom(32), //root as rvalue
      SecureBinaryData(), //empty passphrase
      3); //set lookup computation to 3 entries

   auto assetWlt_2 = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(SecureBinaryData().GenerateRandom(32)), //root as rvalue
      SecureBinaryData(), //empty passphrase
      3); //set lookup computation to 3 entries

   //register with db
   vector<shared_ptr<AddressEntry>> addrVec_1;
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());
   addrVec_1.push_back(assetWlt_1->getNewAddress());

   vector<BinaryData> hashVec_1;
   for (auto addrPtr : addrVec_1)
      hashVec_1.push_back(addrPtr->getPrefixedHash());

   vector<shared_ptr<AddressEntry>> addrVec_2;
   auto addr_type_nested_p2wsh = AddressEntryType(AddressEntryType_P2WPKH | AddressEntryType_P2SH);
   addrVec_2.push_back(assetWlt_2->getNewAddress(addr_type_nested_p2wsh));
   addrVec_2.push_back(assetWlt_2->getNewAddress(addr_type_nested_p2wsh));
   addrVec_2.push_back(assetWlt_2->getNewAddress(addr_type_nested_p2wsh));

   vector<BinaryData> hashVec_2;
   for (auto addrPtr : addrVec_2)
      hashVec_2.push_back(addrPtr->getPrefixedHash());

   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");
   DBTestUtils::regWallet(clients_, bdvID, hashVec_1, assetWlt_1->getID());
   DBTestUtils::regWallet(clients_, bdvID, hashVec_2, assetWlt_2->getID());

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto wlt_1 = bdvPtr->getWalletOrLockbox(assetWlt_1->getID());
   auto wlt_2 = bdvPtr->getWalletOrLockbox(assetWlt_2->getID());

   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   {
      ////spend 12 to wlt_1, 15 to wlt_2 from wlt
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to p2pkh script hash
      signer.addRecipient(addrVec_1[0]->getRecipient(12 * COIN));

      //spend 15 to p2pkh script hash
      signer.addRecipient(addrVec_2[0]->getRecipient(15 * COIN));

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      try
      {
         //shouldn't be able to get txid on legacy unsigned tx
         signer.getTxId();
         EXPECT_TRUE(false);
      }
      catch (exception&)
      {
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = wlt_1->getScrAddrObjByKey(hashVec_1[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = wlt_2->getScrAddrObjByKey(hashVec_2[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);


   auto&& unspentVec_1 =
      wlt_1->getSpendableTxOutListZC();
   auto&& unspentVec_2 =
      wlt_2->getSpendableTxOutListZC();

   BinaryData serializedSignerState;

   {
      //create first signer, set outpoint from wlt_1 and change to wlt_1
      auto spendVal = 18 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //create feed from asset wallet 1

      //create wlt_1 spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec_1)
      {
         total += utxo.getValue();
         signer2.addSpender(
            make_shared<ScriptSpender>(
            utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      }

      //spend 18 to addrB, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), 18 * COIN);
      signer2.addRecipient(recipient2);

      //change back to wlt_1
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer2.addRecipient(addrVec_1[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer2.serializeState());
   }

   {
      //serialize signer 2, deser with signer3 and populate with outpoint and 
      //change from wlt_2
      auto spendVal = 10 * COIN;
      Signer signer3;
      signer3.deserializeState(serializedSignerState);

      //add spender from wlt_2
      uint64_t total = 0;
      for (auto& utxo : unspentVec_2)
      {
         total += utxo.getValue();
         signer3.addSpender(
            make_shared<ScriptSpender>(
            utxo.getTxHash(), utxo.getTxOutIndex(), utxo.getValue()));
      }

      //set change
      if (total > spendVal)
      {
         //spend 4 to p2pkh script hash
         signer3.addRecipient(addrVec_2[1]->getRecipient(total - spendVal));
      }

      serializedSignerState = move(signer3.serializeState());
   }

   auto assetFeed2 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_1);
   auto assetFeed3 = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt_2);

   //deser to new signer, this time populate with feed and utxo from wlt_1
   Signer signer4;
   for (auto& utxo : unspentVec_1)
   {
      signer4.addSpender(getSpenderPtr(utxo, assetFeed2));
   }

   signer4.deserializeState(serializedSignerState);

   {
      auto lock = assetWlt_1->lockDecryptedContainer();
      signer4.sign();
   }

   try
   {
      signer4.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   EXPECT_FALSE(signer4.isValid());

   //should fail to get txid
   try
   {
      signer4.getTxId();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   //deser from same state into wlt_2 signer
   Signer signer5;

   //in this case, we can't set the utxos first then deser the state, as it would break
   //utxo ordering. we have to deser first, then populate utxos
   signer5.deserializeState(signer4.serializeState());

   //should fail since we lack the utxos
   try
   {
      signer5.getTxId();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   for (auto& utxo : unspentVec_2)
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      signer5.populateUtxo(entry);
   }

   //finally set the feed
   signer5.setFeed(assetFeed3);

   //tx should be unsigned
   try
   {
      signer5.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   //should produce valid txid without signing
   BinaryData txid;
   try
   {
      txid = signer5.getTxId();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   //producing a txid should not change the signer status from unsigned to signed
   try
   {
      signer5.verify();
      EXPECT_TRUE(false);
   }
   catch (...)
   {
   }

   {
      auto lock = assetWlt_2->lockDecryptedContainer();
      signer5.sign();
   }

   try
   {
      signer5.verify();
   }
   catch (...)
   {
      EXPECT_TRUE(false);
   }

   //check txid pre sig with txid post sig
   EXPECT_EQ(txid, signer5.getTxId());
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, Wallet_SpendTest_Nested_P2WPKH)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a r value
      SecureBinaryData(),
      3); //lookup computation

   //register with db
   vector<BinaryData> addrVec;

   auto hashSet = assetWlt->getAddrHashSet();
   vector<BinaryData> hashVec;
   hashVec.insert(hashVec.begin(), hashSet.begin(), hashSet.end());

   DBTestUtils::regWallet(clients_, bdvID, hashVec, assetWlt->getID());
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto dbAssetWlt = bdvPtr->getWalletOrLockbox(assetWlt->getID());


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   for (auto& scripthash : hashSet)
   {
      scrObj = dbAssetWlt->getScrAddrObjByKey(scripthash);
      EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   }

   {
      ////spend 27 from wlt to assetWlt's first 2 unused addresses
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to addr0, nested P2WPKH
      auto addr0 = assetWlt->getNewAddress(
         AddressEntryType(AddressEntryType_P2WPKH | AddressEntryType_P2SH));
      signer.addRecipient(addr0->getRecipient(12 * COIN));
      addrVec.push_back(addr0->getPrefixedHash());

      //spend 15 to addr1, nested P2WPKH
      auto addr1 = assetWlt->getNewAddress(
         AddressEntryType(AddressEntryType_P2WPKH | AddressEntryType_P2SH));
      signer.addRecipient(addr1->getRecipient(15 * COIN));
      addrVec.push_back(addr1->getPrefixedHash());

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);

   {
      ////spend 18 back to scrAddrB, with change to addr[2]

      auto spendVal = 18 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //get utxo list for spend value
      auto&& unspentVec =
         dbAssetWlt->getSpendableTxOutListZC();

      //create feed from asset wallet
      auto assetFeed = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt);

      //create spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed));
      }

      //creates outputs
      //spend 18 to addr 0, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
      signer2.addRecipient(recipient2);

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         auto addr2 = assetWlt->getNewAddress(
            AddressEntryType(AddressEntryType_P2WPKH | AddressEntryType_P2SH));
         signer2.addRecipient(addr2->getRecipient(changeVal));
         addrVec.push_back(addr2->getPrefixedHash());
      }

      //sign, verify & broadcast
      {
         auto lock = assetWlt->lockDecryptedContainer();
         signer2.sign();
      }

      EXPECT_TRUE(signer2.verify());

      DBTestUtils::ZcVector zcVec2;
      zcVec2.push_back(signer2.serialize(), 15000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec2);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[2]);
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(SignerTest, Wallet_SpendTest_Nested_P2PK)
{
   //create spender lamba
   auto getSpenderPtr = [](
      const UnspentTxOut& utxo,
      shared_ptr<ResolverFeed> feed)
      ->shared_ptr<ScriptSpender>
   {
      UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
         move(utxo.txHash_), move(utxo.script_));

      return make_shared<ScriptSpender>(entry, feed);
   };

   //
   TestUtils::setBlocks({ "0", "1", "2", "3" }, blk0dat_);

   initBDM();

   theBDMt_->start(config.initMode_);
   auto&& bdvID = DBTestUtils::registerBDV(clients_, magic_);

   vector<BinaryData> scrAddrVec;
   scrAddrVec.push_back(TestChain::scrAddrA);
   scrAddrVec.push_back(TestChain::scrAddrB);
   scrAddrVec.push_back(TestChain::scrAddrC);
   scrAddrVec.push_back(TestChain::scrAddrD);
   scrAddrVec.push_back(TestChain::scrAddrE);

   //// create assetWlt ////

   //create a root private key
   auto&& wltRoot = SecureBinaryData().GenerateRandom(32);
   auto assetWlt = AssetWallet_Single::createFromPrivateRoot_Armory135(
      homedir_,
      move(wltRoot), //root as a r value
      SecureBinaryData(),
      3); //lookup computation

   //register with db
   vector<BinaryData> addrVec;

   auto hashSet = assetWlt->getAddrHashSet();
   vector<BinaryData> hashVec;
   hashVec.insert(hashVec.begin(), hashSet.begin(), hashSet.end());

   DBTestUtils::regWallet(clients_, bdvID, hashVec, assetWlt->getID());
   DBTestUtils::regWallet(clients_, bdvID, scrAddrVec, "wallet1");

   auto bdvPtr = DBTestUtils::getBDV(clients_, bdvID);

   //wait on signals
   DBTestUtils::goOnline(clients_, bdvID);
   DBTestUtils::waitOnBDMReady(clients_, bdvID);
   auto wlt = bdvPtr->getWalletOrLockbox(wallet1id);
   auto dbAssetWlt = bdvPtr->getWalletOrLockbox(assetWlt->getID());


   //check balances
   const ScrAddrObj* scrObj;
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 5 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);

   //check new wallet balances
   for (auto& scripthash : hashSet)
   {
      scrObj = dbAssetWlt->getScrAddrObjByKey(scripthash);
      EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   }

   {
      ////spend 27 from wlt to assetWlt's first 2 unused addresses
      ////send rest back to scrAddrA

      auto spendVal = 27 * COIN;
      Signer signer;

      //instantiate resolver feed overloaded object
      auto feed = make_shared<ResolverUtils::TestResolverFeed>();

      auto addToFeed = [feed](const BinaryData& key)->void
      {
         auto&& datapair = DBTestUtils::getAddrAndPubKeyFromPrivKey(key);
         feed->h160ToPubKey_.insert(datapair);
         feed->pubKeyToPrivKey_[datapair.second] = key;
      };

      addToFeed(TestChain::privKeyAddrA);
      addToFeed(TestChain::privKeyAddrB);
      addToFeed(TestChain::privKeyAddrC);
      addToFeed(TestChain::privKeyAddrD);
      addToFeed(TestChain::privKeyAddrE);

      //get utxo list for spend value
      auto&& unspentVec = wlt->getSpendableTxOutListForValue(spendVal);

      vector<UnspentTxOut> utxoVec;
      uint64_t tval = 0;
      auto utxoIter = unspentVec.begin();
      while (utxoIter != unspentVec.end())
      {
         tval += utxoIter->getValue();
         utxoVec.push_back(*utxoIter);

         if (tval > spendVal)
            break;

         ++utxoIter;
      }

      //create script spender objects
      uint64_t total = 0;
      for (auto& utxo : utxoVec)
      {
         total += utxo.getValue();
         signer.addSpender(getSpenderPtr(utxo, feed));
      }

      //spend 12 to addr0, nested P2WPKH
      auto addr0 = assetWlt->getNewAddress(
         AddressEntryType(
         AddressEntryType_P2PK | AddressEntryType_P2SH | AddressEntryType_Compressed));
      signer.addRecipient(addr0->getRecipient(12 * COIN));
      addrVec.push_back(addr0->getPrefixedHash());

      //spend 15 to addr1, nested P2WPKH
      auto addr1 = assetWlt->getNewAddress(
         AddressEntryType(
         AddressEntryType_P2PK | AddressEntryType_P2SH | AddressEntryType_Compressed));
      signer.addRecipient(addr1->getRecipient(15 * COIN));
      addrVec.push_back(addr1->getPrefixedHash());

      if (total > spendVal)
      {
         //change to scrAddrD, no fee
         auto changeVal = total - spendVal;
         auto recipientChange = make_shared<Recipient_P2PKH>(
            TestChain::scrAddrD.getSliceCopy(1, 20), changeVal);
         signer.addRecipient(recipientChange);
      }

      //sign, verify then broadcast
      signer.sign();
      EXPECT_TRUE(signer.verify());

      DBTestUtils::ZcVector zcVec;
      zcVec.push_back(signer.serialize(), 14000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 30 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 12 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 15 * COIN);

   {
      ////spend 18 back to scrAddrB, with change to addr[2]

      auto spendVal = 18 * COIN;
      Signer signer2;
      signer2.setFlags(SCRIPT_VERIFY_SEGWIT);

      //get utxo list for spend value
      auto&& unspentVec =
         dbAssetWlt->getSpendableTxOutListZC();

      //create feed from asset wallet
      auto assetFeed = make_shared<ResolverFeed_AssetWalletSingle>(assetWlt);

      //create spenders
      uint64_t total = 0;
      for (auto& utxo : unspentVec)
      {
         total += utxo.getValue();
         signer2.addSpender(getSpenderPtr(utxo, assetFeed));
      }

      //creates outputs
      //spend 18 to addr 0, use P2PKH
      auto recipient2 = make_shared<Recipient_P2PKH>(
         TestChain::scrAddrB.getSliceCopy(1, 20), spendVal);
      signer2.addRecipient(recipient2);

      if (total > spendVal)
      {
         //deal with change, no fee
         auto changeVal = total - spendVal;
         auto addr2 = assetWlt->getNewAddress(
            AddressEntryType(
            AddressEntryType_P2PK | AddressEntryType_P2SH | AddressEntryType_Compressed));
         signer2.addRecipient(addr2->getRecipient(changeVal));
         addrVec.push_back(addr2->getPrefixedHash());
      }

      //add opreturn for coverage
      BinaryData opreturn_msg("op_return message testing");
      signer2.addRecipient(make_shared<Recipient_OPRETURN>(opreturn_msg));

      //sign, verify & broadcast
      {
         auto lock = assetWlt->lockDecryptedContainer();
         signer2.sign();
      }
      EXPECT_TRUE(signer2.verify());

      DBTestUtils::ZcVector zcVec2;
      zcVec2.push_back(signer2.serialize(), 15000000);

      DBTestUtils::pushNewZc(theBDMt_, zcVec2);
      DBTestUtils::waitOnNewZcSignal(clients_, bdvID);
   }

   //check balances
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrA);
   EXPECT_EQ(scrObj->getFullBalance(), 50 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrB);
   EXPECT_EQ(scrObj->getFullBalance(), 48 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrC);
   EXPECT_EQ(scrObj->getFullBalance(), 55 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrD);
   EXPECT_EQ(scrObj->getFullBalance(), 8 * COIN);
   scrObj = wlt->getScrAddrObjByKey(TestChain::scrAddrE);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);

   //check new wallet balances
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[0]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[1]);
   EXPECT_EQ(scrObj->getFullBalance(), 0 * COIN);
   scrObj = dbAssetWlt->getScrAddrObjByKey(addrVec[2]);
   EXPECT_EQ(scrObj->getFullBalance(), 9 * COIN);
}

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

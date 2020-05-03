#include "TestCC.h"
#include "CheckRecipSigner.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

/***
All tests here are disabled. Use TestCCoin instead with new CC logic.
This file is kept just for reference - until anyone needs it and
maintains its compilability.
***/

TestCC::TestCC()
   : QObject(nullptr)
{}

void TestCC::mineBlocks(unsigned count)
{
   auto curHeight = envPtr_->armoryConnection()->topBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());
   envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
}

void TestCC::sendTo(uint64_t value, bs::Address& addr)
{
   //create spender
   auto iter = coinbaseHashes_.begin();
   for (unsigned i = 0; i < coinbaseCounter_; i++)
      ++iter;
   ++coinbaseCounter_;

   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto fullUtxoScript = coinbaseRecipient.getSerializedScript();
   auto utxoScript = fullUtxoScript.getSliceCopy(9, fullUtxoScript.getSize() - 9);
   UTXO utxo(50 * COIN, iter->first, 0, 0, iter->second, utxoScript);
      auto spendPtr = std::make_shared<ScriptSpender>(utxo);

   //craft tx off of a single utxo
   Signer signer;

   signer.addSpender(spendPtr);

   signer.addRecipient(addr.getRecipient(bs::XBTAmount{ value }));
   signer.setFeed(coinbaseFeed_);

   //sign & send
   signer.sign();
   envPtr_->armoryInstance()->pushZC(signer.serializeSignedTx());
   envPtr_->blockMonitor()->waitForZC();
}

void TestCC::SetUp()
{
   UnitTestWalletACT::clear();

   passphrase_ = SecureBinaryData::fromString("pass");
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ = 
      std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();

   mineBlocks(101);

   const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };

   const auto priWallet = envPtr_->walletsMgr()->createWallet("Primary", "",
      bs::core::wallet::Seed(SecureBinaryData::fromString("test seed"), NetworkType::TestNet),
      envPtr_->armoryInstance()->homedir_, pd, true);

   if (!priWallet)
      return;
   const auto ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   if (!ccGroup) 
      return;

   {
      const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
      ccSignWallet_ = ccGroup->createLeaf(AddressEntryType_Default, 0x00667675/*"BLK"*/);
      if (!ccSignWallet_)
         return;
   }

   const auto xbtGroup = priWallet->getGroup(priWallet->getXBTGroupType());
   if (!xbtGroup)
      return;

   const bs::hd::Path xbtPath({ bs::hd::Purpose::Native, priWallet->getXBTGroupType(), 0 });
   const auto xbtLeaf = xbtGroup->getLeafByPath(xbtPath);
   if (!xbtLeaf) {
      return;
   }
   {
      const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
      xbtSignWallet_ = xbtGroup->createLeaf(AddressEntryType_Default, 1);
      if (!xbtSignWallet_)
         return;
   }

   auto inprocSigner = std::make_shared<InprocSigner>(priWallet, envPtr_->logger());
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();

   auto syncWallet = syncMgr_->getHDWalletById(priWallet->walletId());

   syncWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncWallet->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   ccWallet_ = syncMgr_->getWalletById(ccSignWallet_->walletId());

   xbtWallet_ = syncMgr_->getDefaultWallet();
   if (!xbtWallet_)
      return;

   auto promGenAddr = std::make_shared<std::promise<bs::Address>>();
   auto futGenAddr = promGenAddr->get_future();
   const auto &cbGenAddr = [promGenAddr](const bs::Address &addr) {
      promGenAddr->set_value(addr);
   };
   xbtWallet_->getNewExtAddress(cbGenAddr);
   genesisAddr_ = futGenAddr.get();
   resolver_ = std::make_shared<CCResolver>(envPtr_->assetMgr()->getCCLotSize("BLK"), genesisAddr_);
   auto ccLeaf = std::dynamic_pointer_cast<bs::sync::hd::CCLeaf>(ccWallet_);
   ASSERT_NE(ccLeaf, nullptr);
   ccLeaf->setCCDataResolver(resolver_);
   sendTo(initialAmount_ * COIN, genesisAddr_);
   mineBlocks(6);

   auto promRecvAddr = std::make_shared<std::promise<bs::Address>>();
   auto futRecvAddr = promRecvAddr->get_future();
   const auto &cbRecvAddr = [promRecvAddr](const bs::Address &addr) {
      promRecvAddr->set_value(addr);
   };
   xbtWallet_->getNewExtAddress(cbRecvAddr);
   recvAddr_ = futRecvAddr.get();

   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };
   ccWallet_->getNewExtAddress(cbAddr);
   const auto addr = futAddr.get();
   auto promFund = std::make_shared<std::promise<bool>>();
   auto futFund = promFund->get_future();
   const auto &cbInputs = [this, priWallet, xbtLeaf, addr, promFund]
      (std::vector<UTXO> inputs)
   {
      try 
      {
         const auto fundingTxReq = xbtWallet_->createTXRequest(
            inputs, 
            { addr.getRecipient(bs::XBTAmount{uint64_t(ccFundingAmount_ * ccLotSize_)}) },
            987, false, genesisAddr_);

         BinaryData fundingTx;
         {
            const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
            fundingTx = xbtLeaf->signTXRequest(fundingTxReq);
         }

         Tx txObj(fundingTx);
         envPtr_->armoryInstance()->pushZC(fundingTx);
         auto&& zcvec = envPtr_->blockMonitor()->waitForZC();

         const bool zcVecOk = (zcvec.size() == 1);
         EXPECT_EQ(zcvec[0].txHash, txObj.getThisHash());

         mineBlocks(6);
         promFund->set_value(zcVecOk);
      }
      catch (const std::exception &e) 
      {
         envPtr_->logger()->error("[TestCC] failed to create CC funding TX: {}", e.what());
         throw e;
      }
   };
   xbtWallet_->getSpendableTxOutList(cbInputs, UINT64_MAX, true);
   ASSERT_TRUE(futFund.get());

   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   auto ctrPtr = std::make_shared<std::atomic<unsigned>>(0);

   auto waitOnBalance = [ctrPtr, promPtr](void)->void
   {
      if (ctrPtr->fetch_add(1) == 1)
         promPtr->set_value(true);
   };
   xbtWallet_->updateBalances(waitOnBalance);
   ccWallet_->updateBalances(waitOnBalance);
   fut.wait();
}

void TestCC::TearDown()
{
   envPtr_->walletsMgr()->deleteWalletFile(envPtr_->walletsMgr()->getPrimaryWallet());
   UnitTestWalletACT::clear();
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestCC, DISABLED_Initial_balance)
{
   ASSERT_NE(envPtr_->walletsMgr()->getPrimaryWallet(), nullptr);
   ASSERT_NE(ccWallet_, nullptr);
   ASSERT_FALSE(genesisAddr_.empty());
   EXPECT_EQ(ccWallet_->type(), bs::core::wallet::Type::ColorCoin);
   EXPECT_GE(ccWallet_->getUsedAddressCount(), 1);
   EXPECT_EQ(ccWallet_->getTotalBalance(), ccFundingAmount_);
   EXPECT_EQ(ccWallet_->getSpendableBalance(), ccFundingAmount_);

   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   EXPECT_TRUE(ccWallet_->getSpendableTxOutList(
      [promPtr](std::vector<UTXO>) { promPtr->set_value(true); }, UINT64_MAX, true));
  
    auto balances = ccWallet_->getAddrBalance(ccWallet_->getUsedAddressList()[0]);
   EXPECT_EQ(balances[0], ccFundingAmount_);
   
   fut.wait();

   auto totalLeft = 
      initialAmount_ * COIN - (ccFundingAmount_ * ccLotSize_) - 987;
   double totalD = (double)totalLeft / (double)COIN;

   EXPECT_DOUBLE_EQ(xbtWallet_->getTotalBalance(), totalD);
}

TEST_F(TestCC, DISABLED_TX_buy)
{
   const float feePerByte = 7.5;
   const double qtyCC = 100;
   auto promRecvAddr = std::make_shared<std::promise<bs::Address>>();
   auto futRecvAddr = promRecvAddr->get_future();
   const auto &cbRecvAddr = [promRecvAddr](const bs::Address &addr) {
      promRecvAddr->set_value(addr);
   };
   ccWallet_->getNewExtAddress(cbRecvAddr);
   const auto ccRecvAddr = futRecvAddr.get();
   const double price = 0.005;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;

   // dealer starts first in case of buy
   BinaryData txHash;
   const auto &cbTxOutList1 =
      [this, qtyCC, spendVal2, ccRecvAddr, feePerByte, &txHash](std::vector<UTXO> inputs1)
   {
      const uint64_t spendVal1 = qtyCC * ccLotSize_;
      const auto recipient1 = ccRecvAddr.getRecipient(bs::XBTAmount{ spendVal1 });
      ASSERT_NE(recipient1, nullptr);

      auto promChange1Addr = std::make_shared<std::promise<bs::Address>>();
      auto futChange1Addr = promChange1Addr->get_future();
      const auto &cbChange1Addr = [promChange1Addr](const bs::Address &addr) {
         promChange1Addr->set_value(addr);
      };
      ccWallet_->getNewChangeAddress(cbChange1Addr);
      const auto changeAddr1 = futChange1Addr.get();
      auto txReq1 = ccWallet_->createPartialTXRequest(spendVal1, inputs1, changeAddr1, 0, { recipient1 });

      // requester uses dealer's TX
      const auto &cbTxOutList2 =
         [this, spendVal2, txReq1, feePerByte, &txHash](std::vector<UTXO> inputs2) {
         const auto recipient2 = recvAddr_.getRecipient(bs::XBTAmount{ spendVal2 });
         ASSERT_NE(recipient2, nullptr);
         auto promChange2Addr = std::make_shared<std::promise<bs::Address>>();
         auto futChange2Addr = promChange2Addr->get_future();
         const auto &cbChange2Addr = [promChange2Addr](const bs::Address &addr) {
            promChange2Addr->set_value(addr);
         };
         xbtWallet_->getNewChangeAddress(cbChange2Addr);
         const auto changeAddr2 = futChange2Addr.get();
         const bs::core::wallet::OutputSortOrder &outSortOrder = { bs::core::wallet::OutputOrderType::PrevState
               , bs::core::wallet::OutputOrderType::Recipients, bs::core::wallet::OutputOrderType::Change };
         auto txReq2 = xbtWallet_->createPartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
            , { recipient2 }, outSortOrder, txReq1.serializeState());

         bs::CheckRecipSigner checkSigner(envPtr_->armoryConnection());
         checkSigner.deserializeState(txReq1.serializeState());

         auto promPtr = std::make_shared<std::promise<bool>>();
         auto fut = promPtr->get_future();
         const auto &cbGACheck = [promPtr](bool result) {
            ASSERT_TRUE(result);
            promPtr->set_value(true);
         };
         checkSigner.hasInputAddress(genesisAddr_, cbGACheck, ccLotSize_);

         // dealer uses requester's TX
         bs::core::wallet::TXSignRequest txReq3;
         txReq3.prevStates = { txReq2.serializeState() };
         txReq3.populateUTXOs = true;
         txReq3.inputs = txReq1.inputs;

         const auto priWallet = envPtr_->walletsMgr()->getPrimaryWallet();

         BinaryData signed1;
         {
            auto ccLeaf =
               std::dynamic_pointer_cast<bs::core::hd::Leaf>(ccSignWallet_);
            const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
            signed1 = ccSignWallet_->signPartialTXRequest(txReq3);
            ASSERT_FALSE(signed1.empty());
         }

         BinaryData signed2;
         {
            auto xbtLeaf =
               std::dynamic_pointer_cast<bs::core::hd::Leaf>(xbtSignWallet_);
            const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
            signed2 = xbtSignWallet_->signPartialTXRequest(txReq2);
            ASSERT_FALSE(signed2.empty());
         }

         Signer signer;                         // merge halves
         signer.deserializeState(signed1);
         signer.deserializeState(signed2);
         ASSERT_TRUE(signer.isValid());
         ASSERT_TRUE(signer.verify());
         auto tx = signer.serializeSignedTx();
         ASSERT_FALSE(tx.empty());

         Tx txObj(tx);
         txHash = txObj.getThisHash();
         envPtr_->armoryInstance()->pushZC(tx);

         fut.wait();
      };
      xbtWallet_->getSpendableTxOutList(cbTxOutList2, UINT64_MAX, true);
   };
   ccWallet_->getSpendableTxOutList(cbTxOutList1, UINT64_MAX, true);

   auto zcVec = envPtr_->blockMonitor()->waitForZCs(3);

   ASSERT_EQ(zcVec.size(), 3);
   EXPECT_EQ(zcVec[1].txHash, txHash);

   auto promBal = std::make_shared<std::promise<bool>>();
   auto futBal = promBal->get_future();
   auto ctrPtr = std::make_shared<std::atomic<unsigned>>(0);
   auto balLbd = [promBal, ctrPtr](void)->void
   {
      if (ctrPtr->fetch_add(1) == 1)
         promBal->set_value(true);
   };

   ccWallet_->updateBalances(balLbd);
   xbtWallet_->updateBalances(balLbd);
   futBal.wait();

   auto balances = ccWallet_->getAddrBalance(ccRecvAddr);
   ASSERT_GE(balances.size(), 1);
   EXPECT_EQ(balances[0], qtyCC);

   balances = xbtWallet_->getAddrBalance(recvAddr_);
   ASSERT_GE(balances.size(), 1);
   EXPECT_EQ(balances[0], spendVal2);
}

TEST_F(TestCC, DISABLED_TX_sell)
{
   const float feePerByte = 8.5;
   const double qtyCC = 100;
   auto promRecvAddr = std::make_shared<std::promise<bs::Address>>();
   auto futRecvAddr = promRecvAddr->get_future();
   const auto &cbRecvAddr = [promRecvAddr](const bs::Address &addr) {
      promRecvAddr->set_value(addr);
   };
   ccWallet_->getNewExtAddress(cbRecvAddr);
   const auto ccRecvAddr = futRecvAddr.get();
   const double price = 0.005;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;

   // requester generates the first TX without receiving address
   BinaryData txHash;
   const auto &cbTxOutList1 = 
      [this, qtyCC, spendVal2, feePerByte, ccRecvAddr, &txHash](std::vector<UTXO> inputs1) 
   {
      const uint64_t spendVal1 = qtyCC * ccLotSize_;
      auto promChange1Addr = std::make_shared<std::promise<bs::Address>>();
      auto futChange1Addr = promChange1Addr->get_future();
      const auto &cbChange1Addr = [promChange1Addr](const bs::Address &addr) {
         promChange1Addr->set_value(addr);
      };
      ccWallet_->getNewChangeAddress(cbChange1Addr);
      const auto changeAddr1 = futChange1Addr.get();
      auto txReq1 = ccWallet_->createPartialTXRequest(spendVal1, inputs1, changeAddr1);

      // dealer uses requester's TX
      const auto &cbTxOutList2 = 
         [this, inputs1, spendVal1, spendVal2, txReq1, feePerByte, ccRecvAddr, &txHash]
         (std::vector<UTXO> inputs2) 
      {
         const auto recipient2 = recvAddr_.getRecipient(bs::XBTAmount{ spendVal2 });
         ASSERT_NE(recipient2, nullptr);
         auto promChange2Addr = std::make_shared<std::promise<bs::Address>>();
         auto futChange2Addr = promChange2Addr->get_future();
         const auto &cbChange2Addr = [promChange2Addr](const bs::Address &addr) {
            promChange2Addr->set_value(addr);
         };
         xbtWallet_->getNewChangeAddress(cbChange2Addr);
         const auto changeAddr2 = futChange2Addr.get();
         const bs::core::wallet::OutputSortOrder &outSortOrder = { bs::core::wallet::OutputOrderType::PrevState
               , bs::core::wallet::OutputOrderType::Recipients, bs::core::wallet::OutputOrderType::Change };
         auto txReq2 = xbtWallet_->createPartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
            , { recipient2 }, outSortOrder, txReq1.serializeState());

         // add receiving address on requester side
         bs::core::wallet::TXSignRequest txReq3;
         const auto recipient1 = ccRecvAddr.getRecipient(bs::XBTAmount{ spendVal1 });
         ASSERT_NE(recipient1, nullptr);
         txReq3.recipients.push_back(recipient1);
         txReq3.inputs = inputs1;
         txReq3.populateUTXOs = true;
         txReq3.prevStates = { txReq2.serializeState() };

         const auto priWallet = envPtr_->walletsMgr()->getPrimaryWallet();

         BinaryData signed1, signed2;
         {
            auto ccLeaf =
               std::dynamic_pointer_cast<bs::core::hd::Leaf>(ccSignWallet_);
            const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
            signed1 = ccSignWallet_->signPartialTXRequest(txReq3);
            ASSERT_FALSE(signed1.empty());
         }

         // use full requester's half on dealer side
         txReq2.prevStates = { txReq3.serializeState() };

         {
            auto xbtLeaf =
               std::dynamic_pointer_cast<bs::core::hd::Leaf>(xbtSignWallet_);
            const bs::core::WalletPasswordScoped lock(priWallet, passphrase_);
            signed2 = xbtSignWallet_->signPartialTXRequest(txReq2);
            ASSERT_FALSE(signed2.empty());
         }

         Signer signer;                         // merge halves
         signer.deserializeState(signed1);
         signer.deserializeState(signed2);
         ASSERT_TRUE(signer.isValid());
         ASSERT_TRUE(signer.verify());
         auto tx = signer.serializeSignedTx();
         ASSERT_FALSE(tx.empty());

         Tx txObj(tx);
         txHash = txObj.getThisHash();
         envPtr_->armoryInstance()->pushZC(tx);
      };
      xbtWallet_->getSpendableTxOutList(cbTxOutList2, UINT64_MAX, true);
   };
   ccWallet_->getSpendableTxOutList(cbTxOutList1, UINT64_MAX, true);

   auto zcVec = envPtr_->blockMonitor()->waitForZCs(3);
   ASSERT_EQ(zcVec.size(), 3);
   EXPECT_EQ(zcVec[1].txHash, txHash);

   auto promBal = std::make_shared<std::promise<bool>>();
   auto futBal = promBal->get_future();
   auto ctrPtr = std::make_shared<std::atomic<unsigned>>(0);
   auto balLbd = [promBal, ctrPtr](void)->void
   {
      if (ctrPtr->fetch_add(1) == 1)
         promBal->set_value(true);
   };

   ccWallet_->updateBalances(balLbd);
   xbtWallet_->updateBalances(balLbd);
   futBal.wait();

   auto balances = ccWallet_->getAddrBalance(ccRecvAddr);
   EXPECT_EQ(balances[0], qtyCC);

   balances = xbtWallet_->getAddrBalance(recvAddr_);
   EXPECT_EQ(balances[0], spendVal2);
}

#if 0 // temporarily disabled
TEST_F(TestCC, sell_after_buy)
{
   const float feePerByte = 7.5;
   const double qtyCC = 1;
   bs::hd::Wallet priWallet("reqWallet", "Requester wallet", NetworkType::TestNet);
   const auto ccGroup = priWallet.createGroup(bs::hd::CoinType::BlockSettle_CC);
   ASSERT_NE(ccGroup, nullptr);
   const auto reqCCwallet = ccGroup->createLeaf("BLK");
   ASSERT_NE(reqCCwallet, nullptr);
   reqCCwallet->setData(TestEnv::assetMgr()->getCCLotSize("BLK"));
   reqCCwallet->setData(genesisAddr_.display<std::string>());
   const auto ccRecvAddr = reqCCwallet->GetNewExtAddress();
   priWallet.RegisterWallet(TestEnv::armory());
   TestEnv::blockMonitor()->waitForWalletReady(reqCCwallet);

   // dealer starts first in case of buy
   const uint64_t spendVal1 = qtyCC * ccLotSize_;
   const auto recipient1 = ccRecvAddr.getRecipient(spendVal1);
   ASSERT_NE(recipient1, nullptr);
   const auto inputs1 = ccWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputs1.empty());
   const auto changeAddr1 = ccWallet_->GetNewChangeAddress();
   auto txReq1 = ccWallet_->CreatePartialTXRequest(spendVal1, inputs1, changeAddr1, 0, { recipient1 });

   // requester uses dealer's TX
   const double price = 0.01;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;
   const auto inputs2 = xbtWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputs2.empty());
   const auto recipient2 = recvAddr_.getRecipient(spendVal2);
   ASSERT_NE(recipient2, nullptr);
   const auto changeAddr2 = xbtWallet_->GetNewChangeAddress();
   auto txReq2 = xbtWallet_->CreatePartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
      , { recipient2 }, txReq1.serializeState());

   bs::CheckRecipSigner checkSigner;
   checkSigner.deserializeState(txReq1.serializeState());
   ASSERT_TRUE(checkSigner.hasInputAddress(genesisAddr_, ccLotSize_));

   // dealer uses requester's TX
   bs::wallet::TXSignRequest txReq3;
   txReq3.prevStates = { txReq2.serializeState() };
   txReq3.populateUTXOs = true;
   txReq3.inputs = txReq1.inputs;
   const auto signed1 = ccWallet_->SignPartialTXRequest(txReq3);
   ASSERT_FALSE(signed1.empty());
   const auto signed2 = xbtWallet_->SignPartialTXRequest(txReq2);
   ASSERT_FALSE(signed2.empty());

   Signer signer;                         // merge halves
   signer.deserializeState(signed1);
   signer.deserializeState(signed2);
   ASSERT_TRUE(signer.isValid());
   ASSERT_TRUE(signer.verify());
   auto tx = signer.serialize();
   ASSERT_FALSE(tx.empty());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(tx.toHexStr())));
   auto curHeight = TestEnv::armory()->topBlock();
   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   reqCCwallet->UpdateBalanceFromDB();
   xbtWallet_->UpdateBalanceFromDB();
   EXPECT_EQ(reqCCwallet->getAddrBalance(ccRecvAddr)[0], qtyCC);

   //--------------------------------

   const auto inputsSell = reqCCwallet->getSpendableTxOutList();
   ASSERT_FALSE(inputsSell.empty());
   const auto changeAddr3 = ccWallet_->GetNewChangeAddress();
   auto txReqSell1 = reqCCwallet->CreatePartialTXRequest(spendVal1, inputsSell, changeAddr3);

   // dealer uses requester's TX
   const double priceSell = 0.009;
   const uint64_t spendValSell = qtyCC * priceSell * BTCNumericTypes::BalanceDivider;
   const auto recvAddr = xbtWallet_->GetNewExtAddress();
   std::vector<UTXO> inputsSellXbt = xbtWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputsSellXbt.empty());
   const auto recipient4 = recvAddr.getRecipient(spendValSell);
   ASSERT_NE(recipient4, nullptr);
   const auto changeAddr4 = xbtWallet_->GetNewChangeAddress();
   auto txReqSell2 = xbtWallet_->CreatePartialTXRequest(spendValSell, inputsSellXbt, changeAddr4, feePerByte
      , { recipient4 }, txReqSell1.serializeState());

   const auto ccSellRecvAddr = ccWallet_->GetUsedAddressList()[1];
   // add receiving address on requester side
   bs::wallet::TXSignRequest txReqSell;
   const auto recipientSell = ccSellRecvAddr.getRecipient(spendVal1);
   ASSERT_NE(recipientSell, nullptr);
   txReqSell.recipients.push_back(recipientSell);
   txReqSell.inputs = inputsSell;
   txReqSell.populateUTXOs = true;
   txReqSell.prevStates = { txReqSell2.serializeState() };
   const auto signed3 = reqCCwallet->SignPartialTXRequest(txReqSell);
   ASSERT_FALSE(signed3.empty());

   // use full requester's half on dealer side
   txReqSell2.inputs = inputsSellXbt;
   txReqSell2.populateUTXOs = true;
   txReqSell2.prevStates = { txReqSell.serializeState() };
   const auto signed4 = xbtWallet_->SignPartialTXRequest(txReqSell2);
   ASSERT_FALSE(signed4.empty());

   Signer signerSell;
   signerSell.deserializeState(signed3);
   signerSell.deserializeState(signed4);
   ASSERT_TRUE(signerSell.isValid());
   ASSERT_TRUE(signerSell.verify());
   auto txSell = signerSell.serialize();
   ASSERT_FALSE(txSell.empty());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSell.toHexStr())));
   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   ccWallet_->UpdateBalanceFromDB();
   xbtWallet_->UpdateBalanceFromDB();
   EXPECT_EQ(ccWallet_->getAddrBalance(ccSellRecvAddr)[0], qtyCC);
   EXPECT_EQ(xbtWallet_->getAddrBalance(recvAddr)[0], spendValSell);
}
#endif   //0

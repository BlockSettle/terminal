#include "TestCC.h"
#include <QDebug>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "CheckRecipSigner.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "TestEnv.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


TestCC::TestCC()
   : QObject(nullptr)
{}

void TestCC::SetUp()
{
   TestEnv::requireAssets();
   const auto &cbBalance = [](double balance) {
      auto curHeight = TestEnv::armory()->topBlock();
      if (balance < 50) {
//         TestEnv::regtestControl()->GenerateBlocks(101, [](bool) {});
         TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 101);
      }
   };
//   TestEnv::regtestControl()->GetBalance(cbBalance);

   const auto priWallet = TestEnv::walletsMgr()->createWallet("Primary", "", NetworkType::TestNet
      , TestEnv::appSettings()->GetHomeDir().toStdString(), true);
   if (!priWallet) {
      return;
   }
   const auto ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   if (!ccGroup) {
      return;
   }
   ccSignWallet_ = ccGroup->createLeaf("BLK");
   if (!ccSignWallet_) {
      return;
   }
   const auto addr = ccWallet_->getNewExtAddress();
   ccWallet_->getNewExtAddress();

   const auto xbtGroup = priWallet->getGroup(priWallet->getXBTGroupType());
   if (!xbtGroup) {
      return;
   }
   const auto xbtLeaf = xbtGroup->getLeaf(0);
   if (!xbtLeaf) {
      return;
   }
   xbtSignWallet_ = xbtGroup->createLeaf(1);
   if (!xbtSignWallet_) {
      return;
   }

   auto inprocSigner = std::make_shared<InprocSigner>(priWallet, TestEnv::logger());
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();

   auto syncWallet = syncMgr_->getHDWalletById(priWallet->walletId());
   ccWallet_ = syncMgr_->getWalletById(ccWallet_->walletId());
   ccWallet_->setData(TestEnv::assetMgr()->getCCLotSize("BLK"));

   const auto wallet = syncMgr_->getDefaultWallet();
   if (!wallet) {
      return;
   }
   genesisAddr_ = wallet->getNewExtAddress(AddressEntryType_P2SH);
   ccWallet_->setData(genesisAddr_.display());

   fundingAddr_ = xbtWallet_->getNewExtAddress(AddressEntryType_P2SH);
   recvAddr_ = xbtWallet_->getNewExtAddress();
   syncWallet->registerWallet(TestEnv::armory());

//   TestEnv::regtestControl()->SendTo(1.23, fundingAddr_, [](QString) {});
//   TestEnv::regtestControl()->SendTo(initialAmount_, genesisAddr_, [](QString) {});

   auto curHeight = TestEnv::armory()->topBlock();
//   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   TestEnv::blockMonitor()->waitForWalletReady(wallet);
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);

   const auto &cbInputs = [this, xbtLeaf, wallet, addr](std::vector<UTXO> inputs) {
      try {
         const auto fundingTxReq = wallet->createTXRequest(inputs
            , { addr.getRecipient(uint64_t(ccFundingAmount_ * ccLotSize_)) }, 987);
         const auto fundingTx = xbtLeaf->signTXRequest(fundingTxReq);

         const auto &cbTx = [this](bool result) {
            if (result) {
               const auto curHeight = TestEnv::armory()->topBlock();
//               TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
               TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
            }
            else {
               TestEnv::logger()->error("[TestCC] failed to send CC funding TX");
            }
         };
//         TestEnv::regtestControl()->SendTx(QString::fromStdString(fundingTx.toHexStr()), cbTx);
      }
      catch (const std::exception &e) {
         TestEnv::logger()->error("[TestCC] failed to create CC funding TX: {}", e.what());
         throw e;
      }
   };
   auto inputs = wallet->getSpendableTxOutList(cbInputs, nullptr);

   TestEnv::blockMonitor()->waitForWalletReady(ccWallet_);
   wallet->updateBalances();
   ccWallet_->updateBalances();
}

void TestCC::TearDown()
{
   TestEnv::walletsMgr()->deleteWalletFile(TestEnv::walletsMgr()->getPrimaryWallet());
}


TEST_F(TestCC, Initial_balance)
{
   ASSERT_NE(TestEnv::walletsMgr()->getPrimaryWallet(), nullptr);
   ASSERT_NE(ccWallet_, nullptr);
   ASSERT_FALSE(genesisAddr_.isNull());
   EXPECT_EQ(ccWallet_->type(), bs::core::wallet::Type::ColorCoin);
   EXPECT_GE(ccWallet_->getUsedAddressCount(), 2);
   EXPECT_EQ(ccWallet_->getTotalBalance(), ccFundingAmount_);
   EXPECT_EQ(ccWallet_->getSpendableBalance(), ccFundingAmount_);
   EXPECT_TRUE(ccWallet_->getSpendableTxOutList([](std::vector<UTXO>){}, nullptr));
  
 
   const auto &cbBalance = [this](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], ccFundingAmount_);
   };
   ccWallet_->getAddrBalance(ccWallet_->getUsedAddressList()[0], cbBalance);
   EXPECT_DOUBLE_EQ(xbtWallet_->getTotalBalance(), 1.23);
}

TEST_F(TestCC, TX_buy)
{
   const float feePerByte = 7.5;
   const double qtyCC = 100;
   const auto ccRecvAddr = ccWallet_->getUsedAddressList()[1];
   const double price = 0.005;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;

   // dealer starts first in case of buy
   const auto &cbTxOutList1 = [this, qtyCC, spendVal2, ccRecvAddr, feePerByte](std::vector<UTXO> inputs1) {
      const uint64_t spendVal1 = qtyCC * ccLotSize_;
      const auto recipient1 = ccRecvAddr.getRecipient(spendVal1);
      ASSERT_NE(recipient1, nullptr);
      const auto changeAddr1 = ccWallet_->getNewChangeAddress();
      auto txReq1 = ccWallet_->createPartialTXRequest(spendVal1, inputs1, changeAddr1, 0, { recipient1 });

      // requester uses dealer's TX
      const auto &cbTxOutList2 = [this, qtyCC, spendVal2, txReq1, feePerByte](std::vector<UTXO> inputs2) {
         const auto recipient2 = recvAddr_.getRecipient(spendVal2);
         ASSERT_NE(recipient2, nullptr);
         const auto changeAddr2 = xbtWallet_->getNewChangeAddress();
         auto txReq2 = xbtWallet_->createPartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
            , { recipient2 }, txReq1.serializeState());

         bs::CheckRecipSigner checkSigner;
         checkSigner.deserializeState(txReq1.serializeState());

         const auto &cbGACheck = [this](bool result) {
            ASSERT_TRUE(result);
         };
         checkSigner.hasInputAddress(genesisAddr_, cbGACheck, ccLotSize_);

         // dealer uses requester's TX
         bs::core::wallet::TXSignRequest txReq3;
         txReq3.prevStates = { txReq2.serializeState() };
         txReq3.populateUTXOs = true;
         txReq3.inputs = txReq1.inputs;
         const auto signed1 = ccSignWallet_->signPartialTXRequest(txReq3);
         ASSERT_FALSE(signed1.isNull());
         const auto signed2 = xbtSignWallet_->signPartialTXRequest(txReq2);
         ASSERT_FALSE(signed2.isNull());

         Signer signer;                         // merge halves
         signer.deserializeState(signed1);
         signer.deserializeState(signed2);
         ASSERT_TRUE(signer.isValid());
         ASSERT_TRUE(signer.verify());
         auto tx = signer.serialize();
         ASSERT_FALSE(tx.isNull());

         const auto &cbTx = [](bool result) {
            ASSERT_TRUE(result);
         };
//         TestEnv::regtestControl()->SendTx(QString::fromStdString(tx.toHexStr()), cbTx);
      };
      xbtWallet_->getSpendableTxOutList(cbTxOutList2, nullptr);
   };
   ccWallet_->getSpendableTxOutList(cbTxOutList1, nullptr);

   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   ccWallet_->updateBalances();
   xbtWallet_->updateBalances();

   const auto &cbBalCC = [qtyCC](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], qtyCC);
   };
   ccWallet_->getAddrBalance(ccRecvAddr, cbBalCC);

   const auto &cbBalXbt = [spendVal2](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], spendVal2);
   };
   xbtWallet_->getAddrBalance(recvAddr_, cbBalXbt);
}

TEST_F(TestCC, TX_sell)
{
   const float feePerByte = 8.5;
   const double qtyCC = 100;
   const auto ccRecvAddr = ccWallet_->getUsedAddressList()[1];
   const double price = 0.005;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;

   // requester generates the first TX without receiving address
   const auto &cbTxOutList1 = [this, qtyCC, spendVal2, feePerByte, ccRecvAddr](std::vector<UTXO> inputs1) {
      const uint64_t spendVal1 = qtyCC * ccLotSize_;
      const auto changeAddr1 = ccWallet_->getNewChangeAddress();
      auto txReq1 = ccWallet_->createPartialTXRequest(spendVal1, inputs1, changeAddr1);

      // dealer uses requester's TX
      const auto &cbTxOutList2 = [this, inputs1, spendVal1, spendVal2, txReq1, feePerByte, ccRecvAddr]
      (std::vector<UTXO> inputs2) {
         const auto recipient2 = recvAddr_.getRecipient(spendVal2);
         ASSERT_NE(recipient2, nullptr);
         const auto changeAddr2 = xbtWallet_->getNewChangeAddress();
         auto txReq2 = xbtWallet_->createPartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
            , { recipient2 }, txReq1.serializeState());

         // add receiving address on requester side
         bs::core::wallet::TXSignRequest txReq3;
         const auto recipient1 = ccRecvAddr.getRecipient(spendVal1);
         ASSERT_NE(recipient1, nullptr);
         txReq3.recipients.push_back(recipient1);
         txReq3.inputs = inputs1;
         txReq3.populateUTXOs = true;
         txReq3.prevStates = { txReq2.serializeState() };
         const auto signed1 = ccSignWallet_->signPartialTXRequest(txReq3);
         ASSERT_FALSE(signed1.isNull());

         // use full requester's half on dealer side
         txReq2.prevStates = { txReq3.serializeState() };
         const auto signed2 = xbtSignWallet_->signPartialTXRequest(txReq2);
         ASSERT_FALSE(signed2.isNull());

         Signer signer;                         // merge halves
         signer.deserializeState(signed1);
         signer.deserializeState(signed2);
         ASSERT_TRUE(signer.isValid());
         ASSERT_TRUE(signer.verify());
         auto tx = signer.serialize();
         ASSERT_FALSE(tx.isNull());

         const auto &cbTx = [](bool result) {
            ASSERT_TRUE(result);
         };
//         TestEnv::regtestControl()->SendTx(QString::fromStdString(tx.toHexStr()), cbTx);
      };
      xbtWallet_->getSpendableTxOutList(cbTxOutList2, nullptr);
   };
   ccWallet_->getSpendableTxOutList(cbTxOutList1, nullptr);

   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   ccWallet_->updateBalances();
   xbtWallet_->updateBalances();

   const auto &cbBalCC = [qtyCC](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], qtyCC);
   };
   ccWallet_->getAddrBalance(ccRecvAddr, cbBalCC);

   const auto &cbBalXbt = [spendVal2](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], spendVal2);
   };
   xbtWallet_->getAddrBalance(recvAddr_, cbBalXbt);
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
   reqCCwallet->setData(genesisAddr_.display());
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
   ASSERT_FALSE(signed1.isNull());
   const auto signed2 = xbtWallet_->SignPartialTXRequest(txReq2);
   ASSERT_FALSE(signed2.isNull());

   Signer signer;                         // merge halves
   signer.deserializeState(signed1);
   signer.deserializeState(signed2);
   ASSERT_TRUE(signer.isValid());
   ASSERT_TRUE(signer.verify());
   auto tx = signer.serialize();
   ASSERT_FALSE(tx.isNull());
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
   ASSERT_FALSE(signed3.isNull());

   // use full requester's half on dealer side
   txReqSell2.inputs = inputsSellXbt;
   txReqSell2.populateUTXOs = true;
   txReqSell2.prevStates = { txReqSell.serializeState() };
   const auto signed4 = xbtWallet_->SignPartialTXRequest(txReqSell2);
   ASSERT_FALSE(signed4.isNull());

   Signer signerSell;
   signerSell.deserializeState(signed3);
   signerSell.deserializeState(signed4);
   ASSERT_TRUE(signerSell.isValid());
   ASSERT_TRUE(signerSell.verify());
   auto txSell = signerSell.serialize();
   ASSERT_FALSE(txSell.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSell.toHexStr())));
   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   ccWallet_->UpdateBalanceFromDB();
   xbtWallet_->UpdateBalanceFromDB();
   EXPECT_EQ(ccWallet_->getAddrBalance(ccSellRecvAddr)[0], qtyCC);
   EXPECT_EQ(xbtWallet_->getAddrBalance(recvAddr)[0], spendValSell);
}
#endif   //0

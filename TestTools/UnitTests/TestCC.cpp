#include "TestCC.h"
#include <QDebug>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "HDWallet.h"
#include "HDLeaf.h"
#include "TestEnv.h"
#include "WalletsManager.h"


TestCC::TestCC()
   : QObject(nullptr)
{}

void TestCC::SetUp()
{
   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (TestEnv::regtestControl()->GetBalance() < 50) {
      TestEnv::regtestControl()->GenerateBlocks(101);
      curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 101);
   }
   const auto priWallet = TestEnv::walletsMgr()->CreateWallet("Primary", "", {}, true);
   if (!priWallet) {
      return;
   }
   const auto ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   if (!ccGroup) {
      return;
   }
   ccWallet_ = ccGroup->createLeaf("BLK");
   if (!ccWallet_) {
      return;
   }
   ccWallet_->setData(TestEnv::assetMgr()->getCCLotSize("BLK"));
   const auto addr = ccWallet_->GetNewExtAddress();
   ccWallet_->GetNewExtAddress();

   const auto wallet = TestEnv::walletsMgr()->GetDefaultWallet();
   if (!wallet) {
      return;
   }
   genesisAddr_ = wallet->GetNewExtAddress(AddressEntryType_P2SH);
   ccWallet_->setData(genesisAddr_.display<std::string>());

   const auto xbtGroup = priWallet->getGroup(priWallet->getXBTGroupType());
   if (!xbtGroup) {
      return;
   }
   xbtWallet_ = xbtGroup->createLeaf(1);
   if (!xbtWallet_) {
      return;
   }
   fundingAddr_ = xbtWallet_->GetNewExtAddress(AddressEntryType_P2SH);
   recvAddr_ = xbtWallet_->GetNewExtAddress();
   priWallet->RegisterWallet(PyBlockDataManager::instance());

   if (TestEnv::regtestControl()->SendTo(1.23, fundingAddr_).isNull()) {
      throw std::runtime_error("failed to send TX to funding address");
   }

   if (TestEnv::regtestControl()->SendTo(initialAmount_, genesisAddr_).isNull()) {
      throw std::runtime_error("failed to send TX to genesis address");
   }
   if (!TestEnv::regtestControl()->GenerateBlocks(6)) {
      throw std::runtime_error("failed to generate blocks");
   }
   TestEnv::blockMonitor()->waitForWalletReady(wallet);
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   auto inputs = wallet->getSpendableTxOutList();
   if (inputs.empty()) {
      inputs = wallet->getSpendableZCList();
   }
   if (inputs.empty()) {
      throw std::runtime_error("no spendable inputs");
   }

   try {
      const auto fundingTxReq = wallet->CreateTXRequest(inputs
         , { addr.getRecipient(uint64_t(ccFundingAmount_ * ccLotSize_)) }, 987);
      const auto fundingTx = wallet->SignTXRequest(fundingTxReq);
      if (TestEnv::regtestControl()->SendTx(QString::fromStdString(fundingTx.toHexStr()))) {
         TestEnv::regtestControl()->GenerateBlocks(6);
         curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      }
   }
   catch (const std::exception &e) {
      TestEnv::logger()->error("[TestCC] failed to send CC funding TX: {}", e.what());
      throw e;
   }

   TestEnv::blockMonitor()->waitForWalletReady(ccWallet_);
   wallet->UpdateBalanceFromDB();
   ccWallet_->UpdateBalanceFromDB();
}

void TestCC::TearDown()
{
   TestEnv::walletsMgr()->DeleteWalletFile(TestEnv::walletsMgr()->GetPrimaryWallet());
}


TEST_F(TestCC, Initial_balance)
{
   ASSERT_TRUE(TestEnv::walletsMgr()->HasPrimaryWallet());
   ASSERT_NE(ccWallet_, nullptr);
   EXPECT_EQ(ccWallet_->GetType(), bs::wallet::Type::ColorCoin);
   EXPECT_GE(ccWallet_->GetUsedAddressCount(), 2);
   EXPECT_EQ(ccWallet_->GetTotalBalance(), ccFundingAmount_);
   EXPECT_EQ(ccWallet_->GetSpendableBalance(), ccFundingAmount_);
   EXPECT_FALSE(ccWallet_->getSpendableTxOutList().empty());
   EXPECT_EQ(ccWallet_->getAddrBalance(ccWallet_->GetUsedAddressList()[0])[0], ccFundingAmount_);
   EXPECT_DOUBLE_EQ(xbtWallet_->GetTotalBalance(), 1.23);
}

TEST_F(TestCC, TX_buy)
{
   const float feePerByte = 7.5;
   const double qtyCC = 100;
   const auto ccRecvAddr = ccWallet_->GetUsedAddressList()[1];

   // dealer starts first in case of buy
   const uint64_t spendVal1 = qtyCC * ccLotSize_;
   const auto recipient1 = ccRecvAddr.getRecipient(spendVal1);
   ASSERT_NE(recipient1, nullptr);
   const auto inputs1 = ccWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputs1.empty());
   const auto changeAddr1 = ccWallet_->GetNewChangeAddress();
   auto txReq1 = ccWallet_->CreatePartialTXRequest(spendVal1, inputs1, changeAddr1, 0, { recipient1 });

   // requester uses dealer's TX
   const double price = 0.005;
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
   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   PyBlockDataManager::instance()->updateWalletsLedgerFilter({xbtWallet_->GetWalletId(), ccWallet_->GetWalletId()});
   ccWallet_->UpdateBalanceFromDB();
   xbtWallet_->UpdateBalanceFromDB();
   EXPECT_EQ(ccWallet_->getAddrBalance(ccRecvAddr)[0], qtyCC);
   EXPECT_EQ(xbtWallet_->getAddrBalance(recvAddr_)[0], spendVal2);
}

TEST_F(TestCC, TX_sell)
{
   const float feePerByte = 8.5;
   const double qtyCC = 100;
   const auto ccRecvAddr = ccWallet_->GetUsedAddressList()[1];

   // requester generates the first TX without receiving address
   const uint64_t spendVal1 = qtyCC * ccLotSize_;
   const auto inputs1 = ccWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputs1.empty());
   const auto changeAddr1 = ccWallet_->GetNewChangeAddress();
   auto txReq1 = ccWallet_->CreatePartialTXRequest(spendVal1, inputs1, changeAddr1);

   // dealer uses requester's TX
   const double price = 0.005;
   const uint64_t spendVal2 = qtyCC * price * BTCNumericTypes::BalanceDivider;
   const auto inputs2 = xbtWallet_->getSpendableTxOutList();
   ASSERT_FALSE(inputs2.empty());
   const auto recipient2 = recvAddr_.getRecipient(spendVal2);
   ASSERT_NE(recipient2, nullptr);
   const auto changeAddr2 = xbtWallet_->GetNewChangeAddress();
   auto txReq2 = xbtWallet_->CreatePartialTXRequest(spendVal2, inputs2, changeAddr2, feePerByte
      , { recipient2 }, txReq1.serializeState());

   // add receiving address on requester side
   bs::wallet::TXSignRequest txReq3;
   const auto recipient1 = ccRecvAddr.getRecipient(spendVal1);
   ASSERT_NE(recipient1, nullptr);
   txReq3.recipients.push_back(recipient1);
   txReq3.inputs = inputs1;
   txReq3.populateUTXOs = true;
   txReq3.prevStates = { txReq2.serializeState() };
   const auto signed1 = ccWallet_->SignPartialTXRequest(txReq3);
   ASSERT_FALSE(signed1.isNull());

   // use full requester's half on dealer side
   txReq2.prevStates = { txReq3.serializeState() };
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
   EXPECT_TRUE(TestEnv::blockMonitor()->waitForZC());
   PyBlockDataManager::instance()->updateWalletsLedgerFilter({ xbtWallet_->GetWalletId(), ccWallet_->GetWalletId() });
   ccWallet_->UpdateBalanceFromDB();
   xbtWallet_->UpdateBalanceFromDB();
   EXPECT_EQ(ccWallet_->getAddrBalance(ccRecvAddr)[0], qtyCC);
   EXPECT_EQ(xbtWallet_->getAddrBalance(recvAddr_)[0], spendVal2);
}

TEST_F(TestCC, sell_after_buy)
{
   const float feePerByte = 7.5;
   const double qtyCC = 1;
   bs::hd::Wallet priWallet("reqWallet", "Requester wallet", false, NetworkType::TestNet);
   const auto ccGroup = priWallet.createGroup(bs::hd::CoinType::BlockSettle_CC);
   ASSERT_NE(ccGroup, nullptr);
   const auto reqCCwallet = ccGroup->createLeaf("BLK");
   ASSERT_NE(reqCCwallet, nullptr);
   reqCCwallet->setData(TestEnv::assetMgr()->getCCLotSize("BLK"));
   reqCCwallet->setData(genesisAddr_.display<std::string>());
   const auto ccRecvAddr = reqCCwallet->GetNewExtAddress();
   priWallet.RegisterWallet(PyBlockDataManager::instance());
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
   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   TestEnv::regtestControl()->GenerateBlocks(6);
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

#include "TestSettlement.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <spdlog/spdlog.h>
#include "HDWallet.h"
#include "TestEnv.h"
#include "PyBlockDataManager.h"
#include "RegtestController.h"
#include "TransactionData.h"
#include "WalletsManager.h"


TestSettlement::TestSettlement()
   : QObject(nullptr)
   , receivedPayIn_(false), receivedPayOut_(false)
   , settlWalletReady_(false)
{}

void TestSettlement::SetUp()
{
   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (TestEnv::regtestControl()->GetBalance() < 50) {
      TestEnv::regtestControl()->GenerateBlocks(101);
      curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 101);
   }
   const auto amount = initialTransferAmount_;

   for (size_t i = 0; i < nbParties_; i++) {
      auto hdWallet = std::make_shared<bs::hd::Wallet>("Primary" + std::to_string(i), "", true);
      auto grp = hdWallet->createGroup(hdWallet->getXBTGroupType());
      auto leaf = grp->createLeaf(0);
      auto addr = leaf->GetNewExtAddress(AddressEntryType_P2SH);
      TestEnv::logger()->debug("[TestSettlement] sending {} to {}: {}", amount, addr.display<std::string>()
         , TestEnv::regtestControl()->SendTo(amount, addr).toStdString());

      hdWallet->setUserId(SecureBinaryData().GenerateRandom(32));
      auto authGrp = hdWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto authLeaf = authGrp->createLeaf(0);
      auto authAddr = authLeaf->GetNewExtAddress();

      hdWallet->RegisterWallet(PyBlockDataManager::instance());

      hdWallet_.emplace_back(hdWallet);
      wallet_.emplace_back(leaf);
      authWallet_.emplace_back(authLeaf);
      authAddr_.emplace_back(authAddr);
      fundAddr_.emplace_back(addr);
      TestEnv::logger()->debug("[TestSettlement] {} fundAddr={}, authAddr={}", wallet_[i]->GetWalletName(), addr.display<std::string>(), authAddr.display<std::string>());
   }

   TestEnv::regtestControl()->GenerateBlocks(6);
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   for (size_t i = 0; i < nbParties_; i++) {
      TestEnv::blockMonitor()->waitForWalletReady(wallet_[i]);
      wallet_[i]->UpdateBalanceFromDB();
      TestEnv::logger()->debug("[TestSettlement] {} balance = {}/{}", wallet_[i]->GetWalletName(), wallet_[i]->GetSpendableBalance(), wallet_[i]->GetTotalBalance());
   }

   if (!TestEnv::walletsMgr()->GetSettlementWallet()) {
      TestEnv::walletsMgr()->CreateSettlementWallet();
   }
   TestEnv::blockMonitor()->waitForWalletReady(TestEnv::walletsMgr()->GetSettlementWallet());
   TestEnv::walletsMgr()->GetSettlementWallet()->UpdateBalanceFromDB();
   connect(TestEnv::walletsMgr()->GetSettlementWallet().get(), &bs::Wallet::walletReady, this, &TestSettlement::onWalletReady);
   settlementId_ = SecureBinaryData().GenerateRandom(32);
}

void TestSettlement::TearDown()
{
   wallet_.clear();
   authWallet_.clear();
   authAddr_.clear();
   fundAddr_.clear();
   hdWallet_.clear();
   userId_.clear();
}

void TestSettlement::onWalletReady(const QString &id)
{
   if (id.toStdString() == TestEnv::walletsMgr()->GetSettlementWallet()->GetWalletId()) {
      settlWalletReady_ = true;
   }
}

TEST_F(TestSettlement, Initial_balances)
{
   ASSERT_FALSE(wallet_.empty());
   for (size_t i = 0; i < nbParties_; i++) {
      ASSERT_NE(wallet_[i], nullptr);
      ASSERT_GE(wallet_[i]->GetSpendableBalance(), initialTransferAmount_);
   }
   EXPECT_GE(TestEnv::walletsMgr()->estimatedFeePerByte(1), 5);

   ASSERT_NE(TestEnv::walletsMgr()->GetSettlementWallet(), nullptr);
   EXPECT_DOUBLE_EQ(TestEnv::walletsMgr()->GetSettlementWallet()->GetTotalBalance(), 0);
}

TEST_F(TestSettlement, SpotXBT_sell)
{
   const auto feePerByte = TestEnv::walletsMgr()->estimatedFeePerByte(1);
   const auto &settlWallet = TestEnv::walletsMgr()->GetSettlementWallet();
   uint32_t curHeight = 0;

   for (int i = 0; i < nbParties_; i++) {
      ASSERT_FALSE(authAddr_[i].isNull());
      ASSERT_FALSE(authWallet_[i]->GetPubChainedKeyFor(authAddr_[i]).isNull());
   }
   const auto settlementAddr = settlWallet->newAddress(settlementId_
      , authWallet_[0]->GetPubChainedKeyFor(authAddr_[0]), authWallet_[1]->GetPubChainedKeyFor(authAddr_[1]));
   ASSERT_NE(settlementAddr, nullptr);
   EXPECT_TRUE(waitForSettlWallet());

   // Requester's side
   TransactionData reqTxData([] {});
   reqTxData.SetFeePerByte(feePerByte);
   reqTxData.SetWallet(wallet_[1]);
   const auto reqRecip = reqTxData.RegisterNewRecipient();
   reqTxData.UpdateRecipientAddress(reqRecip, settlementAddr);
   reqTxData.UpdateRecipientAmount(reqRecip, 0.1);
   ASSERT_TRUE(reqTxData.IsTransactionValid());
   ASSERT_GE(reqTxData.GetTransactionSummary().selectedBalance, 0.1);

   auto monitor = settlWallet->createMonitor(settlementAddr, TestEnv::logger());
   ASSERT_NE(monitor, nullptr);
   connect(monitor.get(), &bs::SettlementMonitor::payInDetected, [this] { receivedPayIn_ = true; });
   connect(monitor.get(), &bs::SettlementMonitor::payOutDetected, [this] (int nbConf, bs::PayoutSigner::Type poType) {
      qDebug() << "poType=" << poType << "nbConf=" << nbConf;
      receivedPayOut_ = true;
      poType_ = poType;
   });
   monitor->start();

   auto txPayInReq = reqTxData.CreateTXRequest();
   const auto txPayIn = reqTxData.GetWallet()->SignTXRequest(txPayInReq);
   const auto txHex = QString::fromStdString(txPayIn.toHexStr());
   ASSERT_FALSE(txPayIn.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(txHex));

   ASSERT_TRUE(waitForPayIn());
   settlWallet->UpdateBalanceFromDB();
   EXPECT_DOUBLE_EQ(settlWallet->GetUnconfirmedBalance(), 0.1);

   // Responder's side
   const auto authKeys = authWallet_[0]->GetKeyPairFor(authAddr_[0], {});
   ASSERT_FALSE(authKeys.privKey.isNull());
   ASSERT_FALSE(authKeys.pubKey.isNull());

   ASSERT_FALSE(settlWallet->getSpendableZCList().empty());
   const auto payInHash = Tx(txPayIn).getThisHash();
   auto txReq = settlWallet->CreatePayoutTXRequest(settlWallet->GetInputFor(settlementAddr)
      , fundAddr_[0], feePerByte);
   ASSERT_FALSE(txReq.inputs.empty());
   const auto txPayOut = settlWallet->SignPayoutTXRequest(txReq, authKeys, settlementAddr->getAsset()->settlementId()
      , settlementAddr->getAsset()->buyAuthPubKey(), settlementAddr->getAsset()->sellAuthPubKey());
   ASSERT_FALSE(txPayOut.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txPayOut.toHexStr())));

//   ASSERT_TRUE(waitForPayOut());
   curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   TestEnv::regtestControl()->GenerateBlocks(1);
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 1);
   EXPECT_TRUE(waitForPayOut());
   EXPECT_EQ(poType_, bs::PayoutSigner::Type::SignedByBuyer);

   curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   TestEnv::regtestControl()->GenerateBlocks(6);
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   settlWallet->UpdateBalanceFromDB();
   wallet_[0]->UpdateBalanceFromDB();
   wallet_[1]->UpdateBalanceFromDB();
   EXPECT_GT(wallet_[0]->GetTotalBalance(), initialTransferAmount_ + 0.1 - 0.01);   // buyer (dealer)
   EXPECT_LT(wallet_[1]->GetTotalBalance(), initialTransferAmount_ - 0.1);          // seller (requester)
   EXPECT_DOUBLE_EQ(settlWallet->GetTotalBalance(), 0);
   monitor = nullptr;
}

TEST_F(TestSettlement, SpotXBT_buy)
{
   const auto feePerByte = TestEnv::walletsMgr()->estimatedFeePerByte(1);
   const auto &settlWallet = TestEnv::walletsMgr()->GetSettlementWallet();
   uint32_t curHeight = 0;
   const double amount = 0.1;

   for (int i = 0; i < nbParties_; i++) {
      ASSERT_FALSE(authAddr_[i].isNull());
      ASSERT_FALSE(authWallet_[i]->GetPubChainedKeyFor(authAddr_[i]).isNull());
   }
   const auto settlementAddr = settlWallet->newAddress(settlementId_
      , authWallet_[0]->GetPubChainedKeyFor(authAddr_[0]), authWallet_[1]->GetPubChainedKeyFor(authAddr_[1]));
   ASSERT_NE(settlementAddr, nullptr);
   EXPECT_TRUE(waitForSettlWallet());

   // Dealer's side
   TransactionData dealerTxData([] {});
   dealerTxData.SetFeePerByte(feePerByte);
   dealerTxData.SetWallet(wallet_[1]);
   const auto dealerRecip = dealerTxData.RegisterNewRecipient();
   dealerTxData.UpdateRecipientAddress(dealerRecip, settlementAddr);
   dealerTxData.UpdateRecipientAmount(dealerRecip, amount);
   ASSERT_TRUE(dealerTxData.IsTransactionValid());
   ASSERT_GE(dealerTxData.GetTransactionSummary().selectedBalance, amount);
   bs::wallet::TXSignRequest unsignedTxReq;
   if (dealerTxData.GetTransactionSummary().hasChange) {
      const auto changeAddr = dealerTxData.GetWallet()->GetNewChangeAddress();
      unsignedTxReq = dealerTxData.CreateUnsignedTransaction(false, changeAddr);
   }
   else {
      unsignedTxReq = dealerTxData.CreateUnsignedTransaction();
   }
   ASSERT_TRUE(unsignedTxReq.isValid());
   const auto dealerPayInHash = unsignedTxReq.txId();
   EXPECT_FALSE(dealerPayInHash.isNull());

   // Requester's side
   const auto reqTxReq = settlWallet->CreatePayoutTXRequest(settlWallet->GetInputFromTX(settlementAddr, dealerPayInHash, amount)
      , fundAddr_[0], feePerByte);
   const auto authKeys = authWallet_[0]->GetKeyPairFor(authAddr_[0], {});
   ASSERT_FALSE(authKeys.privKey.isNull());
   ASSERT_FALSE(authKeys.pubKey.isNull());
   const auto txPayOut = settlWallet->SignPayoutTXRequest(reqTxReq, authKeys, settlementAddr->getAsset()->settlementId()
      , settlementAddr->getAsset()->buyAuthPubKey(), settlementAddr->getAsset()->sellAuthPubKey());
   ASSERT_FALSE(txPayOut.isNull());

   auto monitor = settlWallet->createMonitor(settlementAddr, TestEnv::logger());
   ASSERT_NE(monitor, nullptr);
   connect(monitor.get(), &bs::SettlementMonitor::payInDetected, [this] { receivedPayIn_ = true; });
   connect(monitor.get(), &bs::SettlementMonitor::payOutDetected, [this](int nbConf, bs::PayoutSigner::Type poType) {
      receivedPayOut_ = true;
      poType_ = poType;
   });
   monitor->start();

   // Back to dealer
   const auto dealerTxReq = dealerTxData.GetSignTXRequest();
   const auto txPayIn = dealerTxData.GetWallet()->SignTXRequest(dealerTxReq);
   ASSERT_FALSE(txPayIn.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txPayIn.toHexStr())));
   ASSERT_TRUE(waitForPayIn());
   settlWallet->UpdateBalanceFromDB();
   EXPECT_DOUBLE_EQ(settlWallet->GetUnconfirmedBalance(), amount);

   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txPayOut.toHexStr())));
   ASSERT_TRUE(waitForPayOut());
   EXPECT_EQ(poType_, bs::PayoutSigner::Type::SignedByBuyer);

   curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   TestEnv::regtestControl()->GenerateBlocks(6);
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   settlWallet->UpdateBalanceFromDB();
   wallet_[0]->UpdateBalanceFromDB();
   wallet_[1]->UpdateBalanceFromDB();
   EXPECT_GT(wallet_[0]->GetTotalBalance(), initialTransferAmount_ + amount - 0.01);   // buyer (requester)
   EXPECT_LT(wallet_[1]->GetTotalBalance(), initialTransferAmount_ - amount);          // seller (dealer)
   EXPECT_DOUBLE_EQ(settlWallet->GetTotalBalance(), 0);
   monitor = nullptr;
}

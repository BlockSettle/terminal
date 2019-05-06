#include "TestSettlement.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "TestEnv.h"
#include "TransactionData.h"
#include "Wallets/SyncSettlementWallet.h"
#include "Wallets/SyncWalletsManager.h"


TestSettlement::TestSettlement()
   : QObject(nullptr)
   , receivedPayIn_(false), receivedPayOut_(false)
   , settlWalletReady_(false)
{}

void TestSettlement::SetUp()
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

   const auto amount = initialTransferAmount_;

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(TestEnv::logger());
   walletsMgr_->createSettlementWallet(NetworkType::TestNet, {});

   for (size_t i = 0; i < nbParties_; i++) {
      auto hdWallet = std::make_shared<bs::core::hd::Wallet>("Primary" + std::to_string(i), ""
         , NetworkType::TestNet, TestEnv::logger(), true);
      auto grp = hdWallet->createGroup(hdWallet->getXBTGroupType());
      auto leaf = grp->createLeaf(0);
      auto addr = leaf->getNewExtAddress(AddressEntryType_P2SH);

      const auto &cbSend = [amount, addr](QString result) {
         TestEnv::logger()->debug("[TestSettlement] sending {} to {}: {}", amount, addr.display()
            , result.toStdString());
      };
//      TestEnv::regtestControl()->SendTo(amount, addr, cbSend);

      hdWallet->setChainCode(CryptoPRNG::generateRandom(32));
      auto authGrp = hdWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto authLeaf = authGrp->createLeaf(0);
      auto authAddr = authLeaf->getNewExtAddress();

      walletsMgr_->addWallet(hdWallet);
      signWallet_.emplace_back(leaf);
      authSignWallet_.emplace_back(authLeaf);
      authAddr_.emplace_back(authAddr);
      fundAddr_.emplace_back(addr);
      TestEnv::logger()->debug("[TestSettlement] {} fundAddr={}, authAddr={}", hdWallet->name()
         , addr.display(), authAddr.display());
   }

   auto inprocSigner = std::make_shared<InprocSigner>(walletsMgr_, TestEnv::logger(), "", NetworkType::TestNet);
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();
   syncMgr_->registerWallets();

   auto curHeight = TestEnv::armory()->topBlock();
//   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   for (const auto &wallet : syncMgr_->getAllWallets()) {
      wallet->updateBalances();
   }

   const auto settlWallet = syncMgr_->getWalletById(walletsMgr_->getSettlementWallet()->walletId());
   TestEnv::blockMonitor()->waitForWalletReady(settlWallet);
   settlWallet->updateBalances();
   connect(settlWallet.get(), &bs::sync::Wallet::walletReady, this, &TestSettlement::onWalletReady);
   settlementId_ = CryptoPRNG::generateRandom(32);
}

void TestSettlement::TearDown()
{
   walletsMgr_->reset();
   signWallet_.clear();
   authSignWallet_.clear();
   authWallet_.clear();
   authAddr_.clear();
   fundAddr_.clear();
   hdWallet_.clear();
   userId_.clear();
}

void TestSettlement::onWalletReady(const QString &id)
{
   if (id.toStdString() == syncMgr_->getSettlementWallet()->walletId()) {
      settlWalletReady_ = true;
   }
}

TEST_F(TestSettlement, Initial_balances)
{
   ASSERT_FALSE(signWallet_.empty());
   for (size_t i = 0; i < nbParties_; i++) {
      ASSERT_NE(signWallet_[i], nullptr);
      const auto syncWallet = syncMgr_->getWalletById(signWallet_[i]->walletId());
      ASSERT_NE(syncWallet, nullptr);
      ASSERT_GE(syncWallet->getSpendableBalance(), initialTransferAmount_);
   }

   const auto &cbFee = [](float feePerByte) {
      EXPECT_GE(feePerByte, 5);
   };
   syncMgr_->estimatedFeePerByte(1, cbFee);

   ASSERT_NE(syncMgr_->getSettlementWallet(), nullptr);
   EXPECT_DOUBLE_EQ(syncMgr_->getSettlementWallet()->getTotalBalance(), 0);
}

#if 0    //temporarily disabled
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
#endif   //0

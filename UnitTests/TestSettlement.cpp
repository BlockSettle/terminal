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
#include "Wallets/SyncWalletsManager.h"

using std::make_unique;

void TestSettlement::mineBlocks(unsigned count)
{
   auto curHeight = envPtr_->armoryConnection()->topBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());
   envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
}

void TestSettlement::sendTo(uint64_t value, bs::Address& addr)
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

   signer.addRecipient(addr.getRecipient(value));
   signer.setFeed(coinbaseFeed_);

   //sign & send
   signer.sign();
   envPtr_->armoryInstance()->pushZC(signer.serialize());
}

TestSettlement::TestSettlement()
{}

void TestSettlement::SetUp()
{
   passphrase_ = SecureBinaryData("pass");
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ =
      std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();

   mineBlocks(101);

   auto logger = envPtr_->logger();
   const auto amount = initialTransferAmount_ * COIN;

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
//   walletsMgr_->createSettlementWallet(NetworkType::TestNet, {});

   for (size_t i = 0; i < nbParties_; i++) {
      auto hdWallet = std::make_shared<bs::core::hd::Wallet>(
         "Primary" + std::to_string(i), ""
         , NetworkType::TestNet, passphrase_
         , envPtr_->armoryInstance()->homedir_, logger);

      std::shared_ptr<bs::core::hd::Leaf> leaf;
      bs::Address addr;
      auto grp = hdWallet->createGroup(hdWallet->getXBTGroupType());
      {
         auto lock = hdWallet->lockForEncryption(passphrase_);
         leaf = grp->createLeaf(AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH), 0);
         addr = leaf->getNewExtAddress();
      }

      sendTo(amount, addr);

      std::shared_ptr<bs::core::hd::Leaf> authLeaf;
      bs::Address authAddr;
      auto grpPtr = hdWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto authGrp = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(grpPtr);
      authGrp->setSalt(CryptoPRNG::generateRandom(32));
      {
         auto lock = hdWallet->lockForEncryption(passphrase_);
         authLeaf = authGrp->createLeaf(AddressEntryType_Default, 0);
         authAddr = authLeaf->getNewExtAddress();
      }

      walletsMgr_->addWallet(hdWallet);
      signWallet_.emplace_back(leaf);
      authSignWallet_.emplace_back(authLeaf);
      authAddr_.emplace_back(authAddr);
      fundAddr_.emplace_back(addr);
      logger->debug("[TestSettlement] {} fundAddr={}, authAddr={}", hdWallet->name()
         , addr.display(), authAddr.display());
   }

   auto inprocSigner = std::make_shared<InprocSigner>(
      walletsMgr_, logger, "", NetworkType::TestNet);
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(logger
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();
//!   EXPECT_TRUE(syncMgr_->createSettlementWallet());

   syncMgr_->registerWallets();
//!   ASSERT_TRUE(envPtr_->blockMonitor()->waitForWalletReady(regIDs));

   auto curHeight = envPtr_->armoryConnection()->topBlock();
   mineBlocks(6);

   auto wltCount = syncMgr_->getAllWallets().size();
   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   auto ctrPtr = std::make_shared<std::atomic<unsigned>>(0);
      

   auto balLBD = [promPtr, ctrPtr, wltCount](void)->void
   {
      if (ctrPtr->fetch_add(1) == wltCount)
         promPtr->set_value(true);
   };

   for (const auto &wallet : syncMgr_->getAllWallets())
      wallet->updateBalances(balLBD);

/*!   const auto settlWallet = syncMgr_->getSettlementWallet();
   settlWallet->updateBalances(balLBD);*/
   settlementId_ = CryptoPRNG::generateRandom(32);

   fut.wait();
}

void TestSettlement::TearDown()
{
   if(walletsMgr_ != nullptr)
      walletsMgr_->reset();
   signWallet_.clear();
   authSignWallet_.clear();
   authWallet_.clear();
   authAddr_.clear();
   fundAddr_.clear();
   hdWallet_.clear();
   userId_.clear();
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

//!   ASSERT_NE(syncMgr_->getSettlementWallet(), nullptr);
//!   EXPECT_DOUBLE_EQ(syncMgr_->getSettlementWallet()->getTotalBalance(), 0);
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

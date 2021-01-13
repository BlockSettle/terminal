/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "HeadlessContainer.h"
#include "InprocSigner.h"
#include "TestEnv.h"
#include "TransactionData.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncPlainWallet.h"
#include "CheckRecipSigner.h"

using std::make_unique;
using namespace std::chrono_literals;
using namespace ArmorySigner;

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
   for (unsigned i = 0; i < coinbaseCounter_; i++) {
      ++iter;
   }
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
}

TestSettlement::TestSettlement()
{}

void TestSettlement::SetUp()
{
   passphrase_ = SecureBinaryData::fromString("pass");
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ =
      std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();
//   act_ = std::make_unique<SingleUTWalletACT>(envPtr_->armoryConnection().get());

   mineBlocks(101);

   const auto logger = envPtr_->logger();
   const auto amount = (initialTransferAmount_ + 10) * COIN;

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
   const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };

   for (size_t i = 0; i < nbParties_; i++) {
      auto hdWallet = std::make_shared<bs::core::hd::Wallet>(
         "Primary" + std::to_string(i), ""
         , NetworkType::TestNet, pd
         , envPtr_->armoryInstance()->homedir_, logger);

      std::shared_ptr<bs::core::hd::Leaf> leaf;
      bs::Address addr;
      auto grp = hdWallet->createGroup(hdWallet->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(hdWallet, passphrase_);
         leaf = grp->createLeaf(AddressEntryType_P2SH, 0);
         addr = leaf->getNewExtAddress();
      }

      sendTo(amount, addr);

      std::shared_ptr<bs::core::hd::Leaf> authLeaf, settlLeaf;
      bs::Address authAddr;
      SecureBinaryData authKey;
      auto grpPtr = hdWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto authGrp = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(grpPtr);
      authGrp->setSalt(CryptoPRNG::generateRandom(32));
      {
         const bs::core::WalletPasswordScoped lock(hdWallet, passphrase_);
         authLeaf = authGrp->createLeaf(AddressEntryType_Default, 0);
         authAddr = authLeaf->getNewExtAddress();

         settlLeaf = hdWallet->createSettlementLeaf(authAddr);
         const auto assetPtr = settlLeaf->getRootAsset();
         const auto assetSingle = std::dynamic_pointer_cast<AssetEntry_Single>(assetPtr);
         if (assetSingle) {
            authKey =  assetSingle->getPubKey()->getCompressedKey();
         }
      }

      logger->debug("[TestSettlement] {} fundAddr={}, authAddr={}, authKey={}"
         , hdWallet->name(), addr.display(), authAddr.display(), authKey.toHexStr());
      walletsMgr_->addWallet(hdWallet);
      xbtWallet_.emplace_back(leaf);
      authWallet_.push_back(authLeaf);
      fundAddrs_.emplace_back(addr);
      settlLeafMap_[authAddr] = settlLeaf;
      authAddrs_.emplace_back(authAddr);
      authKeys_.emplace_back(std::move(authKey));
      hdWallet_.push_back(hdWallet);
   }

   hct_ = std::make_shared<QtHCT>(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(
      walletsMgr_, logger, hct_.get(), "", NetworkType::TestNet);
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(logger
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   syncMgr_->syncWallets([promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   });
   futSync.wait();

   UnitTestWalletACT::clear();

   for (const auto &hdWallet : syncMgr_->hdWallets()) {
      hdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   }

   const auto regIDs = syncMgr_->registerWallets();
   UnitTestWalletACT::waitOnRefresh(regIDs);

//   auto curHeight = envPtr_->armoryConnection()->topBlock();
   mineBlocks(6);

   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   auto ctrPtr = std::make_shared<std::atomic<unsigned>>(0);
   auto wltCount = syncMgr_->getAllWallets().size() - 2;

   auto balLBD = [promPtr, ctrPtr, wltCount](void)->void
   {
      ctrPtr->fetch_add(1);
      if (*ctrPtr == wltCount)
         promPtr->set_value(true);
   };

   for (const auto &wallet : syncMgr_->getAllWallets()) {
      wallet->updateBalances(balLBD);
   }

   settlementId_ = CryptoPRNG::generateRandom(32);

   fut.wait();
}

void TestSettlement::TearDown()
{
   xbtWallet_.clear();
   authWallet_.clear();
   authAddrs_.clear();
   fundAddrs_.clear();
   hdWallet_.clear();
   userId_.clear();
   syncMgr_.reset();
}

TestSettlement::~TestSettlement()
{}

TEST_F(TestSettlement, Initial_balances)
{
   ASSERT_FALSE(xbtWallet_.empty());
   for (size_t i = 0; i < nbParties_; i++) {
      ASSERT_NE(xbtWallet_[i], nullptr);
      const auto syncWallet = syncMgr_->getWalletById(xbtWallet_[i]->walletId());
      ASSERT_NE(syncWallet, nullptr);
      ASSERT_GE(syncWallet->getSpendableBalance(), initialTransferAmount_);
   }

   //auto promPtr = std::make_shared<std::promise<float>>();
   //auto fut = promPtr->get_future();
   //
   //const auto &cbFee = [promPtr](float feePerByte) {
   //   promPtr->set_value(feePerByte);
   //};
   //syncMgr_->estimatedFeePerByte(1, cbFee);

   //fut.wait();

   //EXPECT_GE(fut.get(), 5);
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
#endif   //0

#if 0
TEST_F(TestSettlement, SpotXBT_buy)
{
   auto logger = StaticLogger::loggerPtr;

   const float feePerByte = 1.05;
   ASSERT_GE(authAddrs_.size(), 2);
   const auto settlWallet1 = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(settlLeafMap_[authAddrs_[0]]);
   ASSERT_NE(settlWallet1, nullptr);
   const auto settlWallet2 = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(settlLeafMap_[authAddrs_[1]]);
   ASSERT_NE(settlWallet2, nullptr);
   uint32_t curHeight = 0;
   const double amount = 0.1;

   settlWallet1->addSettlementID(settlementId_);
   settlWallet1->getNewExtAddress();
   settlWallet2->addSettlementID(settlementId_);
   settlWallet2->getNewExtAddress();

   ASSERT_GE(authKeys_.size(), 2);
   ASSERT_FALSE(authKeys_[0].isNull());
   ASSERT_FALSE(authKeys_[1].isNull());

   const bs::core::wallet::SettlementData settlDataBuy{ settlementId_, authKeys_[1], true };
   const bs::core::wallet::SettlementData settlDataSell{ settlementId_, authKeys_[0], false };
   const auto settlementAddr = hdWallet_[0]->getSettlementPayinAddress(settlDataBuy);
   ASSERT_FALSE(settlementAddr.isNull());
   EXPECT_EQ(settlementAddr, hdWallet_[1]->getSettlementPayinAddress(settlDataSell));

   const auto dummyWalletId = CryptoPRNG::generateRandom(8).toHexStr();
   auto syncSettlWallet = std::make_shared<bs::sync::PlainWallet>(dummyWalletId
      , "temporary", "Dummy settlement wallet", nullptr, StaticLogger::loggerPtr);
   syncSettlWallet->addAddress(settlementAddr, {}, false);
   UnitTestWalletACT::clear();
   const auto settlRegId = syncSettlWallet->registerWallet(envPtr_->armoryConnection(), true);
   UnitTestWalletACT::waitOnRefresh(settlRegId);

   const auto syncLeaf1 = syncMgr_->getWalletById(xbtWallet_[0]->walletId());
   const auto syncLeaf2 = syncMgr_->getWalletById(xbtWallet_[1]->walletId());

   auto promUtxo2 = std::make_shared<std::promise<UTXO>>();
   auto futUtxo2 = promUtxo2->get_future();
   const auto &cbTxOutList2 = [this, promUtxo2]
   (const std::vector<UTXO> &inputs)->void
   {
      if (inputs.size() != 1) {
         promUtxo2->set_value({});
      }
      else {
         promUtxo2->set_value(inputs.front());
      }
   };
   ASSERT_TRUE(syncLeaf2->getSpendableTxOutList(cbTxOutList2, UINT64_MAX, true));
   const auto input2 = futUtxo2.get();
   ASSERT_TRUE(input2.isInitialized());

   std::vector<UTXO> payinInputs = { input2 };

   // Dealer's side
   TransactionData dealerTxData([] {}, envPtr_->logger());
   dealerTxData.setFeePerByte(feePerByte);
   dealerTxData.setWalletAndInputs(syncLeaf2, payinInputs, envPtr_->armoryConnection()->topBlock());
   const auto dealerRecip = dealerTxData.RegisterNewRecipient();
   dealerTxData.UpdateRecipientAddress(dealerRecip, settlementAddr);
   dealerTxData.UpdateRecipientAmount(dealerRecip, amount);
   ASSERT_TRUE(dealerTxData.IsTransactionValid());
   ASSERT_GE(dealerTxData.GetTransactionSummary().selectedBalance, amount);

   bs::core::wallet::TXSignRequest unsignedTxReq;
   if (dealerTxData.GetTransactionSummary().hasChange) {
      const auto changeAddr = xbtWallet_[1]->getNewChangeAddress();
      unsignedTxReq = dealerTxData.createUnsignedTransaction(false, changeAddr);
   }
   else {
      unsignedTxReq = dealerTxData.createUnsignedTransaction();
   }
   ASSERT_TRUE(unsignedTxReq.isValid());

   auto payinResolver = xbtWallet_[1]->getPublicResolver();
   const auto dealerPayInHash = unsignedTxReq.txId(payinResolver);
   EXPECT_FALSE(dealerPayInHash.isNull());

   const auto serializedPayinRequest = unsignedTxReq.serializeState(payinResolver);

   ASSERT_FALSE(serializedPayinRequest.isNull());

   bs::CheckRecipSigner deserializedSigner{serializedPayinRequest , envPtr_->armoryConnection() };

   auto inputs = deserializedSigner.getTxInsData();

   auto spenders = deserializedSigner.spenders();
   auto recipients = deserializedSigner.recipients();

   logger->debug("Settlement address: {}", settlementAddr.display());
   auto settlementAddressRecipient = settlementAddr.getRecipient(bs::XBTAmount{ amount });
   auto settlementRecipientScript = settlementAddressRecipient->getSerializedScript();

   auto settAddressString = settlementAddr.display();

   bool recipientFound = false;

   // amount to settlement address ( single output )
   for (const auto recipient : recipients) {
      auto amount = recipient->getValue();
      auto serializedRecipient = recipient->getSerializedScript();

      BtcUtils::pprintScript(serializedRecipient);
      std::cout << '\n';

      if (serializedRecipient == settlementRecipientScript) {
         recipientFound = true;
         break;
      }
   }

   for (const auto recipient : recipients) {
      logger->debug("{} : {}", bs::CheckRecipSigner::getRecipientAddress(recipient).display(), recipient->getValue());
   }

   // fee amount
   // get all inputs amount
   uint64_t totalInputAmount = 0;
   for (const auto& input : spenders) {
      totalInputAmount += input->getValue();
   }

   // get all outputs amount
   uint64_t totalOutputValue = 0;
   for (const auto& output : recipients) {
      totalOutputValue += output->getValue();
   }

   try {
      Tx tx{ serializedPayinRequest };

      // XXX check that payin is not rbf
      ASSERT_FALSE(tx.isRBF());

      auto txOutCount = tx.getNumTxOut();

      for (unsigned i = 0; i < txOutCount; ++i) {
         auto txOut = tx.getTxOutCopy(i);

      }

      auto txHash = tx.getThisHash();

      StaticLogger::loggerPtr->debug("TX serialized");
   } catch (...) {
      StaticLogger::loggerPtr->error("Failed to serialize TX");
   }

   // Requester's side
   StaticLogger::loggerPtr->debug("[{}] payin hash: {}", __func__, dealerPayInHash.toHexStr(true));
   const auto payinInput = bs::SettlementMonitor::getInputFromTX(settlementAddr, dealerPayInHash
      , bs::XBTAmount{ amount });
   const auto payoutTxReq = bs::SettlementMonitor::createPayoutTXRequest(payinInput
      , fundAddrs_[0], feePerByte, envPtr_->armoryConnection()->topBlock());
   ASSERT_TRUE(payoutTxReq.isValid());

   BinaryData txPayOut;
   {
      bs::core::WalletPasswordScoped passLock(hdWallet_[0], passphrase_);
      txPayOut = hdWallet_[0]->signSettlementTXRequest(payoutTxReq, settlDataBuy);
   }
   ASSERT_FALSE(txPayOut.isNull());

   // Back to dealer
   const auto payinTxReq = dealerTxData.getSignTxRequest();
   BinaryData txPayIn;
   {
      bs::core::WalletPasswordScoped passLock(hdWallet_[1], passphrase_);
      txPayIn = xbtWallet_[1]->signTXRequest(payinTxReq);
   }
   ASSERT_FALSE(txPayIn.isNull());
   Tx txPayinObj(txPayIn);
   EXPECT_EQ(txPayinObj.getThisHash(), dealerPayInHash);

   // get fee size. check against rate
   const auto totalFee = totalInputAmount - totalOutputValue;
   auto correctedFPB = feePerByte;
   const auto estimatedFee = deserializedSigner.estimateFee(correctedFPB);

   UnitTestWalletACT::clear();
   StaticLogger::loggerPtr->debug("[{}] payin TX: {}", __func__, txPayIn.toHexStr());
   envPtr_->armoryInstance()->pushZC(txPayIn);
   const auto& zcVecPayin = UnitTestWalletACT::waitOnZC();
   ASSERT_GE(zcVecPayin.size(), 1);
   EXPECT_EQ(zcVecPayin[0].txHash, txPayinObj.getThisHash());

   auto promUtxo = std::make_shared<std::promise<UTXO>>();
   auto futUtxo = promUtxo->get_future();
   const auto &cbZCList = [this, promUtxo]
   (const std::vector<UTXO> &inputs)->void
   {
      if (inputs.size() != 1) {
         promUtxo->set_value({});
      } else {
         promUtxo->set_value(inputs.front());
      }
   };
   ASSERT_TRUE(syncSettlWallet->getSpendableZCList(cbZCList));
   const auto zcPayin = futUtxo.get();
   ASSERT_TRUE(zcPayin.isInitialized());
   EXPECT_EQ(zcPayin, payinInput);
   EXPECT_EQ(zcPayin.getScript(), payinInput.getScript());
   EXPECT_EQ(zcPayin.getTxOutIndex(), payinInput.getTxOutIndex());
   std::this_thread::sleep_for(20ms);
   UnitTestWalletACT::clear();

   Tx txPayoutObj(txPayOut);
   ASSERT_TRUE(txPayoutObj.isInitialized());
   ASSERT_EQ(txPayoutObj.getNumTxIn(), 1);
   ASSERT_EQ(txPayoutObj.getNumTxOut(), 1);

   StaticLogger::loggerPtr->debug("[{}] payout TX: {}", __func__, txPayOut.toHexStr());
   envPtr_->armoryInstance()->pushZC(txPayOut);
   const auto& zcVecPayout = UnitTestWalletACT::waitOnZC(true);
   ASSERT_GE(zcVecPayout.size(), 1);
   EXPECT_EQ(zcVecPayout[0].txHash, txPayoutObj.getThisHash());
   std::this_thread::sleep_for(150ms); // have no idea yet, why it's required
}
#endif

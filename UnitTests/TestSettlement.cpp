/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TestSettlement.h"
#include <spdlog/spdlog.h>
#include "CoreHDLeaf.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "MessageUtils.h"
#include "MockTerminal.h"
#include "TestAdapters.h"
#include "TestEnv.h"
#include "TradesUtils.h"
#include "TradesVerification.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace ArmorySigner;
using namespace bs::message;
using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;

constexpr auto kFutureWaitTimeout = std::chrono::seconds(5);
constexpr auto kLongWaitTimeout = std::chrono::seconds(15);

void TestSettlement::mineBlocks(unsigned count, bool wait)
{
//   auto curHeight = envPtr_->armoryConnection()->topBlock();
   auto curHeight = envPtr_->armoryInstance()->getCurrentTopBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());
   if (wait) { // don't use if armoryConnection is not ready
      envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
   }
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
{
   passphrase_ = SecureBinaryData::fromString("pass");
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ =
      std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireArmory(false);

   mineBlocks(101, false);
}

void TestSettlement::SetUp()
{
   const auto logger = envPtr_->logger();
   const auto amount = initialTransferAmount_ * COIN;

   const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };

   for (size_t i = 0; i < nbParties_; i++) {
      walletsMgr_.push_back(std::make_shared<bs::core::WalletsManager>(logger));
      auto hdWallet = std::make_shared<bs::core::hd::Wallet>(
         "Primary" + std::to_string(i), ""
         , NetworkType::TestNet, pd
         , envPtr_->armoryInstance()->homedir_, logger);

      std::shared_ptr<bs::core::hd::Leaf> leaf;
      bs::Address addr;
      auto grp = hdWallet->createGroup(hdWallet->getXBTGroupType());
      {
         const bs::core::WalletPasswordScoped lock(hdWallet, passphrase_);
         leaf = grp->createLeaf(AddressEntryType_P2WPKH, 0);
      }

      addr = leaf->getNewExtAddress();
      sendTo(amount, addr);

      recvAddrs_.push_back(leaf->getNewExtAddress());
      changeAddrs_.push_back(leaf->getNewIntAddress());

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
      walletsMgr_[i]->addWallet(hdWallet);
      xbtWallet_.emplace_back(leaf);
      authWallet_.push_back(authLeaf);
      fundAddrs_.emplace_back(addr);
      settlLeafMap_[authAddr] = settlLeaf;
      authAddrs_.emplace_back(authAddr);
      authKeys_.emplace_back(std::move(authKey));
      hdWallet_.push_back(hdWallet);

      inprocSigner_.push_back(std::make_shared<InprocSigner>(
         walletsMgr_.at(i), logger, this, "", NetworkType::TestNet
         , [this, hdWallet](const std::string&) {
         return std::make_unique<bs::core::WalletPasswordScoped>(hdWallet, passphrase_);
      }));
      inprocSigner_.at(i)->Start();
   }
   mineBlocks(6);
}

void TestSettlement::TearDown()
{
   xbtWallet_.clear();
   authWallet_.clear();
   authAddrs_.clear();
   fundAddrs_.clear();
   hdWallet_.clear();
   walletsMgr_.clear();
   inprocSigner_.clear();
}

TestSettlement::~TestSettlement()
{
   envPtr_->armoryInstance()->shutdown();
}

TEST_F(TestSettlement, Initial_balances)
{
   ASSERT_FALSE(xbtWallet_.empty());
   ASSERT_EQ(nbParties_, 2);
   for (size_t i = 0; i < nbParties_; i++) {
      ASSERT_NE(xbtWallet_.at(i), nullptr);
   }
   MockTerminal t1(StaticLogger::loggerPtr, "T1", inprocSigner_.at(0), envPtr_->armoryConnection());
   MockTerminal t2(StaticLogger::loggerPtr, "T2", inprocSigner_.at(1), envPtr_->armoryConnection());
   const auto& sup1 = std::make_shared<TestSupervisor>(t1.name());
   t1.bus()->addAdapter(sup1);
   const auto& sup2 = std::make_shared<TestSupervisor>(t2.name());
   t2.bus()->addAdapter(sup2);

   const auto& walletReady = [](const auto& walletId)
   {
      return [walletId](const bs::message::Envelope& env)
      {
         if (env.request ||
            (env.sender->value<TerminalUsers>() != TerminalUsers::Wallets)) {
            return false;
         }
         WalletsMessage msg;
         if (msg.ParseFromString(env.message)) {
            if (msg.data_case() == WalletsMessage::kWalletReady) {
               return (msg.wallet_ready() == walletId);
            }
         }
         return false;
      };
   };
   auto fut1 = sup1->waitFor(walletReady(xbtWallet_.at(0)->walletId()));
   auto fut2 = sup2->waitFor(walletReady(xbtWallet_.at(1)->walletId()));
   t1.start();
   t2.start();
   ASSERT_EQ(fut1.wait_for(kFutureWaitTimeout), std::future_status::ready);
   ASSERT_EQ(fut2.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const auto& walletBalance = [](const std::string& walletId, double expectedBal)
   {
      return [walletId, expectedBal](const bs::message::Envelope& env)
      {
         if (!env.receiver || (env.receiver->value<TerminalUsers>() != TerminalUsers::API)
            || (env.sender->value<TerminalUsers>() != TerminalUsers::Wallets)) {
            return false;
         }
         WalletsMessage msg;
         if (msg.ParseFromString(env.message)) {
            if (msg.data_case() == WalletsMessage::kWalletBalances) {
               return ((msg.wallet_balances().wallet_id() == walletId)
                  && qFuzzyCompare(msg.wallet_balances().spendable_balance(), expectedBal));
            }
         }
         return false;
      };
   };
   fut1 = sup1->waitFor(walletBalance(xbtWallet_.at(0)->walletId()
      , initialTransferAmount_));
   fut2 = sup2->waitFor(walletBalance(xbtWallet_.at(1)->walletId()
      , initialTransferAmount_));
   WalletsMessage msgWlt;
   msgWlt.set_get_wallet_balances(xbtWallet_.at(0)->walletId());
   sup1->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   msgWlt.set_get_wallet_balances(xbtWallet_.at(1)->walletId());
   sup2->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   ASSERT_EQ(fut1.wait_for(kFutureWaitTimeout), std::future_status::ready);
   ASSERT_EQ(fut2.wait_for(kFutureWaitTimeout), std::future_status::ready);
}

TEST_F(TestSettlement, SpotFX_sell)
{
   ASSERT_GE(inprocSigner_.size(), 2);
   const std::string& email1 = "aaa@example.com";
   const std::string& email2 = "bbb@example.com";
   MockTerminal t1(StaticLogger::loggerPtr, "T1", inprocSigner_.at(0), envPtr_->armoryConnection());
   MockTerminal t2(StaticLogger::loggerPtr, "T2", inprocSigner_.at(1), envPtr_->armoryConnection());
   const auto& sup1 = std::make_shared<TestSupervisor>(t1.name());
   t1.bus()->addAdapter(sup1);
   const auto& sup2 = std::make_shared<TestSupervisor>(t2.name());
   t2.bus()->addAdapter(sup2);
   const auto& m1 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T1"
      , email1, envPtr_->armoryInstance());
   const auto& m2 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T2"
      , email2, envPtr_->armoryInstance());
   m1->link(m2);
   m2->link(m1);
   t1.bus()->addAdapter(m1);
   t2.bus()->addAdapter(m2);
   t1.start();
   t2.start();

   const auto& rfqId = CryptoPRNG::generateRandom(5).toHexStr();
   const double qty = 123;
   const auto& quoteReqNotif = [this, qty, rfqId](const Envelope& env)
   {
      if (env.receiver || (env.receiver && !env.receiver->isBroadcast()) ||
         (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement)) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message)) {
         if (msg.data_case() == SettlementMessage::kQuoteReqNotif) {
            const auto& rfq = msg.quote_req_notif().rfq();
            return ((rfq.security() == fxSecurity_) && (rfq.product() == fxProduct_)
               && !rfq.buy() && (rfq.quantity() == qty) && (rfq.id() == rfqId));
         }
      }
      return false;
   };
   auto fut = sup2->waitFor(quoteReqNotif);

   SettlementMessage msgSettl;
   auto msgSendRFQ = msgSettl.mutable_send_rfq();
   auto msgRFQ = msgSendRFQ->mutable_rfq();
   msgRFQ->set_id(rfqId);
   msgRFQ->set_security(fxSecurity_);
   msgRFQ->set_product(fxProduct_);
   msgRFQ->set_asset_type((int)bs::network::Asset::SpotFX);
   msgRFQ->set_buy(false);
   msgRFQ->set_quantity(qty);
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const double replyPrice = 1.23;
   const auto& quoteReply = [this, replyPrice, qty, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kQuote)) {
         return ((msg.quote().security() == fxSecurity_) && (msg.quote().product() == fxProduct_)
            && (msg.quote().request_id() == rfqId) && (msg.quote().price() == replyPrice)
            && (msg.quote().quantity() == qty) && msg.quote().buy());
      }
      return false;
   };
   SettlementMessage inMsg;
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& qrn = fromMsg(inMsg.quote_req_notif());
   bs::network::QuoteNotification qn(qrn, {}, replyPrice, {});
   qn.validityInS = 5;
   toMsg(qn, msgSettl.mutable_reply_to_rfq());
   fut = sup1->waitFor(quoteReply);
   sup2->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quote = fromMsg(inMsg.quote());
   const auto& pendingOrder = [this, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kPendingSettlement)) {
         return (msg.pending_settlement().ids().rfq_id() == rfqId);
      }
      return false;
   };
   fut = sup1->waitFor(pendingOrder);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quoteId = inMsg.pending_settlement().ids().quote_id();
   ASSERT_EQ(quoteId, quote.quoteId);

   const auto& filledOrder = [this, rfqId, quoteId, replyPrice](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kMatchedQuote)) {
         const auto& matched = msg.matched_quote();
         return ((matched.rfq_id() == rfqId) && (matched.quote_id() == quoteId)
            && (matched.price() == replyPrice));
      }
      return false;
   };
   fut = sup2->waitFor(filledOrder);
   auto msgAccept = msgSettl.mutable_accept_rfq();
   msgAccept->set_rfq_id(rfqId);
   toMsg(quote, msgAccept->mutable_quote());
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);
}

TEST_F(TestSettlement, SpotFX_buy)
{
   ASSERT_GE(inprocSigner_.size(), 2);
   const std::string& email1 = "aaa@example.com";
   const std::string& email2 = "bbb@example.com";
   MockTerminal t1(StaticLogger::loggerPtr, "T1", inprocSigner_.at(0), envPtr_->armoryConnection());
   MockTerminal t2(StaticLogger::loggerPtr, "T2", inprocSigner_.at(1), envPtr_->armoryConnection());
   const auto& sup1 = std::make_shared<TestSupervisor>(t1.name());
   t1.bus()->addAdapter(sup1);
   const auto& sup2 = std::make_shared<TestSupervisor>(t2.name());
   t2.bus()->addAdapter(sup2);
   const auto& m1 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T1"
      , email1, envPtr_->armoryInstance());
   const auto& m2 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T2"
      , email2, envPtr_->armoryInstance());
   m1->link(m2);
   m2->link(m1);
   t1.bus()->addAdapter(m1);
   t2.bus()->addAdapter(m2);
   t1.start();
   t2.start();

   const auto& rfqId = CryptoPRNG::generateRandom(5).toHexStr();
   const double qty = 234;
   const auto& quoteReqNotif = [this, qty, rfqId](const Envelope& env)
   {
      if (env.receiver || (env.receiver && !env.receiver->isBroadcast()) ||
         (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement)) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message)) {
         if (msg.data_case() == SettlementMessage::kQuoteReqNotif) {
            const auto& rfq = msg.quote_req_notif().rfq();
            return ((rfq.security() == fxSecurity_) && (rfq.product() == fxProduct_)
               && rfq.buy() && (rfq.quantity() == qty) && (rfq.id() == rfqId));
         }
      }
      return false;
   };
   auto fut = sup2->waitFor(quoteReqNotif);

   SettlementMessage msgSettl;
   auto msgSendRFQ = msgSettl.mutable_send_rfq();
   auto msgRFQ = msgSendRFQ->mutable_rfq();
   msgRFQ->set_id(rfqId);
   msgRFQ->set_security(fxSecurity_);
   msgRFQ->set_product(fxProduct_);
   msgRFQ->set_asset_type((int)bs::network::Asset::SpotFX);
   msgRFQ->set_buy(true);
   msgRFQ->set_quantity(qty);
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const double replyPrice = 1.21;
   const auto& quoteReply = [this, replyPrice, qty, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kQuote)) {
         return ((msg.quote().security() == fxSecurity_) && (msg.quote().product() == fxProduct_)
            && (msg.quote().request_id() == rfqId) && (msg.quote().price() == replyPrice)
            && (msg.quote().quantity() == qty) && !msg.quote().buy());
      }
      return false;
   };
   SettlementMessage inMsg;
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& qrn = fromMsg(inMsg.quote_req_notif());
   bs::network::QuoteNotification qn(qrn, {}, replyPrice, {});
   qn.validityInS = 5;
   toMsg(qn, msgSettl.mutable_reply_to_rfq());
   fut = sup1->waitFor(quoteReply);
   sup2->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quote = fromMsg(inMsg.quote());
   const auto& pendingOrder = [this, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kPendingSettlement)) {
         return (msg.pending_settlement().ids().rfq_id() == rfqId);
      }
      return false;
   };
   fut = sup1->waitFor(pendingOrder);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quoteId = inMsg.pending_settlement().ids().quote_id();
   ASSERT_EQ(quoteId, quote.quoteId);

   const auto& filledOrder = [this, rfqId, quoteId, replyPrice](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kMatchedQuote)) {
         const auto& matched = msg.matched_quote();
         return ((matched.rfq_id() == rfqId) && (matched.quote_id() == quoteId)
            && (matched.price() == replyPrice));
      }
      return false;
   };
   fut = sup2->waitFor(filledOrder);
   auto msgAccept = msgSettl.mutable_accept_rfq();
   msgAccept->set_rfq_id(rfqId);
   toMsg(quote, msgAccept->mutable_quote());
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);
}

TEST_F(TestSettlement, SpotXBT_sell)
{
   ASSERT_FALSE(xbtWallet_.empty());
   ASSERT_GE(nbParties_, 2);
   ASSERT_GE(inprocSigner_.size(), 2);
   const float fpbRate = 2.3;
   const std::string& email1 = "aaa@example.com";
   const std::string& email2 = "bbb@example.com";
   MockTerminal t1(StaticLogger::loggerPtr, "T1", inprocSigner_.at(0), envPtr_->armoryConnection());
   MockTerminal t2(StaticLogger::loggerPtr, "T2", inprocSigner_.at(1), envPtr_->armoryConnection());
   const auto& sup1 = std::make_shared<TestSupervisor>(t1.name());
   t1.bus()->addAdapter(sup1);
   const auto& sup2 = std::make_shared<TestSupervisor>(t2.name());
   t2.bus()->addAdapter(sup2);
   const auto& m1 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T1"
      , email1, envPtr_->armoryInstance());
   const auto& m2 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T2"
      , email2, envPtr_->armoryInstance());
   m1->link(m2);
   m2->link(m1);
   t1.bus()->addAdapter(m1);
   t2.bus()->addAdapter(m2);

   const auto& walletReady = [](const auto& walletId)
   {
      return [walletId](const bs::message::Envelope& env)
      {
         if (env.request || env.receiver ||
            (env.sender->value<TerminalUsers>() != TerminalUsers::Wallets)) {
            return false;
         }
         WalletsMessage msg;
         if (msg.ParseFromString(env.message)) {
            if (msg.data_case() == WalletsMessage::kWalletReady) {
               return (msg.wallet_ready() == walletId);
            }
         }
         return false;
      };
   };
   auto fut1 = sup1->waitFor(walletReady(xbtWallet_.at(0)->walletId()));
   auto fut2 = sup2->waitFor(walletReady(xbtWallet_.at(1)->walletId()));
   t1.start();
   t2.start();

   WalletsMessage msgWlt;
   msgWlt.set_set_settlement_fee(fpbRate);
   sup1->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   sup2->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   ASSERT_EQ(fut1.wait_for(kFutureWaitTimeout), std::future_status::ready);
   ASSERT_EQ(fut2.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const auto& rfqId = CryptoPRNG::generateRandom(5).toHexStr();
   const double qty = 0.23;

   auto msgReq = msgWlt.mutable_reserve_utxos();
   msgReq->set_id(rfqId);
   msgReq->set_sub_id(xbtWallet_.at(0)->walletId());
   msgReq->set_amount(bs::XBTAmount(qty).GetValue());
   sup1->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);

   const auto& quoteReqNotif = [this, qty, rfqId](const Envelope& env)
   {
      if (env.receiver || (env.receiver && !env.receiver->isBroadcast()) ||
         (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement)) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message)) {
         if (msg.data_case() == SettlementMessage::kQuoteReqNotif) {
            const auto& rfq = msg.quote_req_notif().rfq();
            return ((rfq.security() == xbtSecurity_) && (rfq.product() == bs::network::XbtCurrency)
               && !rfq.buy() && (rfq.quantity() == qty) && (rfq.id() == rfqId));
         }
      }
      return false;
   };
   auto fut = sup2->waitFor(quoteReqNotif);

   SettlementMessage msgSettl;
   auto msgSendRFQ = msgSettl.mutable_send_rfq();
   msgSendRFQ->set_reserve_id(rfqId);
   auto msgRFQ = msgSendRFQ->mutable_rfq();
   msgRFQ->set_id(rfqId);
   msgRFQ->set_security(xbtSecurity_);
   msgRFQ->set_product(bs::network::XbtCurrency);
   msgRFQ->set_asset_type((int)bs::network::Asset::SpotXBT);
   msgRFQ->set_buy(false);
   msgRFQ->set_quantity(qty);
   msgRFQ->set_auth_pub_key(authKeys_.at(0).toHexStr());
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const double replyPrice = 12345.67;
   const auto& quoteReply = [this, replyPrice, qty, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kQuote)) {
         return ((msg.quote().security() == xbtSecurity_) && (msg.quote().product() == bs::network::XbtCurrency)
            && (msg.quote().request_id() == rfqId) && (msg.quote().price() == replyPrice)
            && (msg.quote().quantity() == qty) && msg.quote().buy());
      }
      return false;
   };
   SettlementMessage inMsg;
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& qrn = fromMsg(inMsg.quote_req_notif());
   bs::network::QuoteNotification qn(qrn, authKeys_.at(1).toHexStr(), replyPrice, {});
   qn.receiptAddress = recvAddrs_.at(1).display();
   qn.validityInS = 5;
   toMsg(qn, msgSettl.mutable_reply_to_rfq());
   fut = sup1->waitFor(quoteReply);
   sup2->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const auto& settlementId = BinaryData::CreateFromHex(qrn.settlementId);
   ASSERT_FALSE(settlementId.empty());
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quote = fromMsg(inMsg.quote());

   const auto& pendingOrder = [rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kPendingSettlement)) {
         return (msg.pending_settlement().ids().rfq_id() == rfqId);
      }
      return false;
   };
   fut = sup1->waitFor(pendingOrder);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quoteId = inMsg.pending_settlement().ids().quote_id();
   ASSERT_EQ(quoteId, quote.quoteId);

   const auto& settlementComplete = [rfqId, quoteId, settlementId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kSettlementComplete)) {
         return ((msg.settlement_complete().rfq_id() == rfqId) &&
            (msg.settlement_complete().quote_id() == quoteId) &&
            (msg.settlement_complete().settlement_id() == settlementId.toBinStr()));
      }
      return false;
   };
   fut = sup2->waitFor(settlementComplete);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
}

TEST_F(TestSettlement, SpotXBT_buy)
{
   ASSERT_FALSE(xbtWallet_.empty());
   ASSERT_GE(nbParties_, 2);
   ASSERT_GE(inprocSigner_.size(), 2);
   const float fpbRate = 2.3;
   const std::string& email1 = "aaa@example.com";
   const std::string& email2 = "bbb@example.com";
   MockTerminal t1(StaticLogger::loggerPtr, "T1", inprocSigner_.at(0), envPtr_->armoryConnection());
   MockTerminal t2(StaticLogger::loggerPtr, "T2", inprocSigner_.at(1), envPtr_->armoryConnection());
   const auto& sup1 = std::make_shared<TestSupervisor>(t1.name());
   t1.bus()->addAdapter(sup1);
   const auto& sup2 = std::make_shared<TestSupervisor>(t2.name());
   t2.bus()->addAdapter(sup2);
   const auto& m1 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T1"
      , email1, envPtr_->armoryInstance());
   const auto& m2 = std::make_shared<MatchingMock>(StaticLogger::loggerPtr, "T2"
      , email2, envPtr_->armoryInstance());
   m1->link(m2);
   m2->link(m1);
   t1.bus()->addAdapter(m1);
   t2.bus()->addAdapter(m2);

   const auto& walletReady = [](const auto& walletId)
   {
      return [walletId](const bs::message::Envelope& env)
      {
         if (env.request || env.receiver ||
            (env.sender->value<TerminalUsers>() != TerminalUsers::Wallets)) {
            return false;
         }
         WalletsMessage msg;
         if (msg.ParseFromString(env.message)) {
            if (msg.data_case() == WalletsMessage::kWalletReady) {
               return (msg.wallet_ready() == walletId);
            }
         }
         return false;
      };
   };
   auto fut1 = sup1->waitFor(walletReady(xbtWallet_.at(0)->walletId()));
   auto fut2 = sup2->waitFor(walletReady(xbtWallet_.at(1)->walletId()));
   t1.start();
   t2.start();

   WalletsMessage msgWlt;
   msgWlt.set_set_settlement_fee(fpbRate);
   sup1->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   sup2->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   ASSERT_EQ(fut1.wait_for(kFutureWaitTimeout), std::future_status::ready);
   ASSERT_EQ(fut2.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const auto& rfqId = CryptoPRNG::generateRandom(5).toHexStr();
   const double qty = 0.237;

   const auto& quoteReqNotif = [this, qty, rfqId](const Envelope& env)
   {
      if (env.receiver || (env.receiver && !env.receiver->isBroadcast()) ||
         (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement)) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message)) {
         if (msg.data_case() == SettlementMessage::kQuoteReqNotif) {
            const auto& rfq = msg.quote_req_notif().rfq();
            return ((rfq.security() == xbtSecurity_) && (rfq.product() == bs::network::XbtCurrency)
               && rfq.buy() && (rfq.quantity() == qty) && (rfq.id() == rfqId));
         }
      }
      return false;
   };
   auto fut = sup2->waitFor(quoteReqNotif);

   SettlementMessage msgSettl;
   auto msgSendRFQ = msgSettl.mutable_send_rfq();
   msgSendRFQ->set_reserve_id(rfqId);
   auto msgRFQ = msgSendRFQ->mutable_rfq();
   msgRFQ->set_id(rfqId);
   msgRFQ->set_security(xbtSecurity_);
   msgRFQ->set_product(bs::network::XbtCurrency);
   msgRFQ->set_asset_type((int)bs::network::Asset::SpotXBT);
   msgRFQ->set_buy(true);
   msgRFQ->set_quantity(qty);
   msgRFQ->set_auth_pub_key(authKeys_.at(0).toHexStr());
   msgRFQ->set_receipt_address(recvAddrs_.at(0).display());
   sup1->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   auto msgReq = msgWlt.mutable_reserve_utxos();
   msgReq->set_id(rfqId);
   msgReq->set_sub_id(xbtWallet_.at(1)->walletId());
   msgReq->set_amount(bs::XBTAmount(qty).GetValue());
   sup2->send(TerminalUsers::API, TerminalUsers::Wallets, msgWlt.SerializeAsString(), true);
   const double replyPrice = 12345.78;

   const auto& quoteReply = [this, replyPrice, qty, rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kQuote)) {
         return ((msg.quote().security() == xbtSecurity_) && (msg.quote().product() == bs::network::XbtCurrency)
            && (msg.quote().request_id() == rfqId) && (msg.quote().price() == replyPrice)
            && (msg.quote().quantity() == qty) && !msg.quote().buy());
      }
      return false;
   };
   SettlementMessage inMsg;
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& qrn = fromMsg(inMsg.quote_req_notif());
   bs::network::QuoteNotification qn(qrn, authKeys_.at(1).toHexStr(), replyPrice, {});
   //qn.receiptAddress = recvAddrs_.at(1).display();
   qn.validityInS = 5;
   toMsg(qn, msgSettl.mutable_reply_to_rfq());
   fut = sup1->waitFor(quoteReply);
   sup2->send(TerminalUsers::API, TerminalUsers::Settlement, msgSettl.SerializeAsString(), true);
   ASSERT_EQ(fut.wait_for(kFutureWaitTimeout), std::future_status::ready);

   const auto& settlementId = BinaryData::CreateFromHex(qrn.settlementId);
   ASSERT_FALSE(settlementId.empty());
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quote = fromMsg(inMsg.quote());

   const auto& pendingOrder = [rfqId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kPendingSettlement)) {
         return (msg.pending_settlement().ids().rfq_id() == rfqId);
      }
      return false;
   };
   fut = sup1->waitFor(pendingOrder);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
   ASSERT_TRUE(inMsg.ParseFromString(fut.get().message));
   const auto& quoteId = inMsg.pending_settlement().ids().quote_id();
   ASSERT_EQ(quoteId, quote.quoteId);

   const auto& settlementComplete = [rfqId, quoteId, settlementId](const Envelope& env)
   {
      if (env.sender->value<TerminalUsers>() != TerminalUsers::Settlement) {
         return false;
      }
      SettlementMessage msg;
      if (msg.ParseFromString(env.message) && (msg.data_case() == SettlementMessage::kSettlementComplete)) {
         return ((msg.settlement_complete().rfq_id() == rfqId) &&
            (msg.settlement_complete().quote_id() == quoteId) &&
            (msg.settlement_complete().settlement_id() == settlementId.toBinStr()));
      }
      return false;
   };
   fut = sup2->waitFor(settlementComplete);
   ASSERT_EQ(fut.wait_for(kLongWaitTimeout), std::future_status::ready);
}

TEST_F(TestSettlement, easy_XBT)
{
   StaticLogger::loggerPtr->debug("[{}] start", __func__);
   ASSERT_GE(xbtWallet_.size(), 2);
   ASSERT_GE(hdWallet_.size(), 2);

   const auto& addr1 = authAddrs_.at(0);
   const auto& pubKey1 = authKeys_.at(0);
   ASSERT_FALSE(addr1.empty());
   ASSERT_FALSE(pubKey1.empty());

   const auto& addr2 = xbtWallet_.at(1)->getNewExtAddress();
   auto pubKey2 = xbtWallet_.at(1)->getPublicKeyFor(addr2);
   ASSERT_FALSE(addr2.empty());
   ASSERT_FALSE(pubKey2.empty());
   ASSERT_NE(addr1, addr2);
   ASSERT_NE(pubKey1, pubKey2);

   //settlement address construction
   const auto& settlementId = CryptoPRNG::generateRandom(32);
   const auto& dealSettlLeaf = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(
      hdWallet_.at(0)->createSettlementLeaf(addr1));
   ASSERT_NE(dealSettlLeaf, nullptr);
   const auto& dealSettlIndex = dealSettlLeaf->addSettlementID(settlementId);
   EXPECT_FALSE(dealSettlLeaf->getNewExtAddress().empty());
   const auto& index1 = dealSettlLeaf->getIndexForSettlementID(settlementId);
   EXPECT_EQ(dealSettlIndex, index1);
   const auto& dealerAsset = dealSettlLeaf->getAssetForId(index1);
   ASSERT_NE(dealerAsset, nullptr);
   const auto& asset1 = std::make_shared<AssetEntry_Single>(0, BinaryData()
      , dealerAsset->getPubKey(), nullptr);
   /*auto dealerPubKey = CryptoECDSA::PubKeyScalarMultiply(pubKey1, settlementId);
   const auto& asset1 = std::make_shared<AssetEntry_Single>(0, BinaryData()
      , dealerPubKey, nullptr);*/
   ASSERT_NE(asset1, nullptr);

   const auto& asset2 = std::make_shared<AssetEntry_Single>(0, BinaryData(), pubKey2, nullptr);
   ASSERT_NE(asset2, nullptr);

   std::map<BinaryData, std::shared_ptr<AssetEntry>> assetMap;
   assetMap.insert({ READHEX("00"), asset1 });
   assetMap.insert({ READHEX("01"), asset2 });

   auto assetMultiSig = std::make_shared<AssetEntry_Multisig>(0, BinaryData()
      , assetMap, 1, 2);
   auto addrMultiSig = std::make_shared<AddressEntry_Multisig>(assetMultiSig, true);
   const auto &addrP2sh = std::make_shared<AddressEntry_P2SH>(addrMultiSig);
   ASSERT_NE(addrP2sh, nullptr);
   StaticLogger::loggerPtr->debug("[{}] settlement address: {}", __func__, addrP2sh->getAddress());
   const auto& settlementAddr = bs::Address::fromAddressEntry(*addrP2sh);
   EXPECT_EQ(settlementAddr.display(), addrP2sh->getAddress());

   //pay-in construction
   auto syncMgr1 = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr1->setSignContainer(inprocSigner_.at(0));
   syncMgr1->syncWallets();
   auto syncHdWallet1 = syncMgr1->getHDWalletById(hdWallet_.at(0)->walletId());
   ASSERT_NE(syncHdWallet1, nullptr);
   syncHdWallet1->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   auto regIDs = syncHdWallet1->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto syncMgr2 = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger()
      , envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr2->setSignContainer(inprocSigner_.at(1));
   syncMgr2->syncWallets();
   auto syncHdWallet2 = syncMgr2->getHDWalletById(hdWallet_.at(1)->walletId());
   ASSERT_NE(syncHdWallet2, nullptr);
   syncHdWallet2->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   regIDs = syncHdWallet2->registerWallet(envPtr_->armoryConnection());
   UnitTestWalletACT::waitOnRefresh(regIDs);

   auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut = promPtr->get_future();
   auto cbTxOutList = [promPtr](const std::vector<UTXO> &inputs)
   {
      promPtr->set_value(inputs);
   };
   ASSERT_TRUE(envPtr_->armoryConnection()->getSpendableTxOutListForValue(
      { xbtWallet_.at(0)->walletId() }, UINT64_MAX, cbTxOutList));
   const auto &inputs = fut.get();
   ASSERT_FALSE(inputs.empty());
   EXPECT_EQ(inputs.size(), 1);

   const uint64_t fee = 234;
   const uint64_t amount = 1234567;
   const auto& changeAddr1 = xbtWallet_.at(0)->getNewIntAddress();
   const auto& recip = settlementAddr.getRecipient(bs::XBTAmount{ amount });
   auto txPayinReq = bs::sync::wallet::createTXRequest({ xbtWallet_.at(0)->walletId() }
      , inputs, { recip }, true, changeAddr1, xbtWallet_.at(0)->getAddressIndex(changeAddr1), fee);

   txPayinReq.resolveSpenders(xbtWallet_.at(0)->getPublicResolver());
   const auto &payinU = bs::TradesVerification::verifyUnsignedPayin(txPayinReq.serializeState()
      , fee / txPayinReq.estimateTxVirtSize(), settlementAddr.display(), amount);
   EXPECT_TRUE(payinU->success) << payinU->errorMsg;

   BinaryData txData;
   {
      const bs::core::WalletPasswordScoped lock(hdWallet_.at(0), passphrase_);
      txData = xbtWallet_.at(0)->signTXRequest(txPayinReq);
   }
   ASSERT_FALSE(txData.empty());
   ASSERT_FALSE(envPtr_->armoryConnection()->pushZC(txData).empty());
   const Tx txPayin(txData);
   ASSERT_TRUE(txPayin.isInitialized());

   const auto& payinS = bs::TradesVerification::verifySignedPayin(txData, txPayin.getThisHash(), inputs);
   EXPECT_TRUE(payinS->success) << payinS->errorMsg;

   //pay-out construction
   ASSERT_GE(xbtWallet_.at(1)->getExtAddressCount(), 1);
   const auto recvAddr = xbtWallet_.at(1)->getExtAddressList().at(0);
   const auto &addrSingle = std::make_shared<AddressEntry_P2WPKH>(asset2);
   const auto &addrP2shSingle = std::make_shared<AddressEntry_P2SH>(addrSingle);
   const auto& payoutUtxo = UTXO(amount, UINT32_MAX, UINT32_MAX, 0
      , txPayin.getThisHash(), addrP2shSingle->getPreimage());
   const auto& txPayoutReq = bs::tradeutils::createPayoutTXRequest(payoutUtxo
      , recvAddr, 1.23, envPtr_->armoryConnection()->topBlock());
   ASSERT_TRUE(txPayoutReq.isValid());

   {
      const bs::core::WalletPasswordScoped lock(hdWallet_.at(1), passphrase_);
      txData = xbtWallet_.at(1)->signTXRequest(txPayoutReq);
   }
   ASSERT_FALSE(txData.empty());
   ASSERT_FALSE(envPtr_->armoryConnection()->pushZC(txData).empty());
   const Tx txPayout(txData);
   ASSERT_TRUE(txPayout.isInitialized());

   bs::tradeutils::PayoutVerifyArgs verifyPayout { txData, settlementAddr
      , txPayin.getThisHash(), bs::XBTAmount{amount}, payoutUtxo };
   const auto& payoutVerif = bs::tradeutils::verifySignedPayout(verifyPayout);
   ASSERT_TRUE(payoutVerif.success) << payoutVerif.errorMsg;

   const auto& payoutS = bs::TradesVerification::verifySignedPayout(txData
      , pubKey2.toHexStr(), dealerAsset->getPubKey()->getCompressedKey().toHexStr()
      , txPayin.getThisHash(), amount, 1.23, {}, settlementAddr.display(), payoutUtxo);
   EXPECT_TRUE(payoutS->success) << payoutS->errorMsg;
}

#include <QApplication>

#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "SettableField.h"
#include "StringUtils.h"
#include "TestEnv.h"
#include "TradesVerification.h"
#include "Trading/OtcClient.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "bs_proxy_terminal_pb.pb.h"

using namespace bs::network;
using namespace Blocksettle::Communication;

namespace {

   const auto kPassword = SecureBinaryData("passphrase");

   const auto kSettlementId = std::string("dc26c004d7b24f71cd5b348a254c292777586f5d9d00f60ac65dd7d5b06d0c2b");

} // namespace

class TestPeer
{
public:
   void init(TestEnv &env, const std::string &name)
   {
      name_ = name;
      bs::core::wallet::Seed seed(name, NetworkType::TestNet);

      bs::wallet::PasswordData pd;
      pd.password = kPassword;
      pd.metaData.encType = bs::wallet::EncryptionType::Password;

      wallet_ = std::make_shared<bs::core::hd::Wallet>(name, "", seed, pd, env.armoryInstance()->homedir_);
      auto group = wallet_->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto xbtGroup = wallet_->createGroup(wallet_->getXBTGroupType());

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      authGroup->setSalt(CryptoPRNG::generateRandom(32));

      {
         const bs::core::WalletPasswordScoped lock(wallet_, kPassword);
         auto authLeaf = authGroup->createLeaf(AddressEntryType_Default, 0);
         authAddress_ = authLeaf->getNewExtAddress();

         auto settlementLeaf = wallet_->createSettlementLeaf(authAddress_);
         ASSERT_TRUE(settlementLeaf);
         ASSERT_TRUE(settlementLeaf->hasExtOnlyAddresses());

         auto nativeLeaf = xbtGroup->createLeaf(AddressEntryType_P2WPKH, 0);
         nativeAddr_ = nativeLeaf->getNewExtAddress();
         ASSERT_FALSE(nativeAddr_.isNull());
         ASSERT_EQ(nativeAddr_.getType(), AddressEntryType_P2WPKH);

         auto nestedLeaf = xbtGroup->createLeaf(static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH), 0);
         ASSERT_TRUE(nestedLeaf);
         nestedAddr_ = nestedLeaf->getNewExtAddress();
         ASSERT_FALSE(nestedAddr_.isNull());
         ASSERT_EQ(nestedAddr_.getType(), static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH));
      }

      env.walletsMgr()->addWallet(wallet_);

      signer_ = std::make_shared<InprocSigner>(env.walletsMgr(), env.logger(), "", NetworkType::TestNet);
      signer_->Start();

      syncWalletMgr_ = std::make_shared<bs::sync::WalletsManager>(env.logger()
         , env.appSettings(), env.armoryConnection());
      syncWalletMgr_->setSignContainer(signer_);

      syncWalletMgr_->setSignContainer(signer_);
      auto promSync = std::promise<bool>();
      syncWalletMgr_->syncWallets([&promSync](int cur, int total) {
         if (cur == total) {
            promSync.set_value(true);
         }
      });
      promSync.get_future().wait();

      const auto regIDs = syncWalletMgr_->registerWallets();
      for (int i = 0; i < int(syncWalletMgr_->hdWalletsCount()); ++i) {
         auto hdWallet = syncWalletMgr_->getHDWallet(unsigned(i));
         hdWallet->setCustomACT<UnitTestWalletACT>(env.armoryConnection());
      }
      UnitTestWalletACT::waitOnRefresh(regIDs);

      OtcClientParams params;
      otc_ = std::make_shared<OtcClient>(env.logger(), syncWalletMgr_, env.armoryConnection(), signer_, nullptr, params);
      otc_->setOwnContactId(name);
   }

   std::string name_;
   std::shared_ptr<bs::core::hd::Wallet> wallet_;
   std::shared_ptr<bs::sync::WalletsManager> syncWalletMgr_;
   std::shared_ptr<InprocSigner> signer_;
   std::shared_ptr<OtcClient> otc_;
   bs::Address authAddress_;
   bs::Address nativeAddr_;
   bs::Address nestedAddr_;

};

class TestOtc : public ::testing::Test
{
public:
   void SetUp() override
   {
      env_ = std::make_unique<TestEnv>(StaticLogger::loggerPtr);
      env_->requireArmory();

      // There is some problem in test env, init order matters (second peer sign fails with "unresolved spender").
      // Let's always use same order then.
      peer1_.init(*env_, "test1");
      peer2_.init(*env_, "test2");

      auto processPbMessage = [this](TestPeer &peer, const std::string &data) {
         ProxyTerminalPb::Request request;
         ASSERT_TRUE(request.ParseFromString(data));

         switch (request.data_case()) {
            case ProxyTerminalPb::Request::kStartOtc: {
               ProxyTerminalPb::Response response;
               auto d = response.mutable_start_otc();
               d->set_request_id(request.start_otc().request_id());
               d->set_settlement_id(kSettlementId);
               peer.otc_->processPbMessage(response);
               break;
            }

            case ProxyTerminalPb::Request::kVerifyOtc: {
               if (request.verify_otc().is_seller()) {
                  ASSERT_FALSE(verifySeller_.isValid());
                  verifySeller_.setValue(request.verify_otc());
               } else {
                  ASSERT_FALSE(verifyBuyer_.isValid());
                  verifyBuyer_.setValue(request.verify_otc());
               }

               if (verifySeller_.isValid() && verifyBuyer_.isValid()) {
                  // verify requests
                  const auto s = verifySeller_.getValue();
                  const auto b = verifyBuyer_.getValue();
                  ASSERT_EQ(s.amount(), b.amount());
                  ASSERT_EQ(s.price(), b.price());
                  ASSERT_EQ(s.settlement_id(), b.settlement_id());
                  ASSERT_EQ(s.auth_address_seller(), b.auth_address_seller());
                  ASSERT_EQ(s.auth_address_buyer(), b.auth_address_buyer());
                  ASSERT_EQ(s.chat_id_seller(), b.chat_id_seller());
                  ASSERT_EQ(s.chat_id_buyer(), b.chat_id_buyer());
                  verifyDone_ = true;

                  auto settlementAddress = bs::TradesVerification::constructSettlementAddress(BinaryData::CreateFromHex(s.settlement_id())
                     , s.auth_address_buyer(), s.auth_address_seller());
                  ASSERT_TRUE(settlementAddress.isValid());

                  auto result = bs::TradesVerification::verifyUnsignedPayin(
                     s.unsigned_tx(), env_->armoryConnection()->testFeePerByte(), settlementAddress.display(), uint64_t(s.amount()));
                  ASSERT_TRUE(result.success);

                  totalFee_ = result.totalFee;

                  sendStateUpdate(ProxyTerminalPb::OTC_STATE_WAIT_BUYER_SIGN);
               }

               break;
            }

            case ProxyTerminalPb::Request::kProcessTx: {
               if (!payoutDone_) {
                  payoutDone_ = true;
                  ASSERT_EQ(&peer, &peer2_);

                  const auto &data = verifySeller_.getValue();

                  auto settlementAddress = bs::TradesVerification::constructSettlementAddress(BinaryData::CreateFromHex(data.settlement_id())
                     , data.auth_address_buyer(), data.auth_address_seller());
                  ASSERT_TRUE(settlementAddress.isValid());

                  auto result = bs::TradesVerification::verifySignedPayout(request.process_tx().signed_tx()
                     , bs::toHex(data.auth_address_buyer()), bs::toHex(data.auth_address_seller()), data.payin_hash()
                     , uint64_t(data.amount()), env_->armoryConnection()->testFeePerByte(), data.settlement_id(), settlementAddress.display());
                  ASSERT_TRUE(result.success);

                  sendStateUpdate(ProxyTerminalPb::OTC_STATE_WAIT_SELLER_SEAL);
               } else if (!payinDone_) {
                  payinDone_ = true;
                  ASSERT_EQ(&peer, &peer1_);

                  const auto &data = verifySeller_.getValue();

                  auto result = bs::TradesVerification::verifySignedPayin(request.process_tx().signed_tx()
                     , data.payin_hash(), env_->armoryConnection()->testFeePerByte(), totalFee_);
                  ASSERT_TRUE(result.success);

                  sendStateUpdate(ProxyTerminalPb::OTC_STATE_SUCCEED);
                  quit_ = true;
                  return;
               }

               break;
            }

            case ProxyTerminalPb::Request::kSealPayinValidity: {
               ASSERT_FALSE(payinSealDone_);
               payinSealDone_ = true;
               sendStateUpdate(ProxyTerminalPb::OTC_STATE_WAIT_SELLER_SIGN);
               break;
            }

            default:
               ASSERT_TRUE(false);
         }
      };

      QObject::connect(peer1_.otc_.get(), &OtcClient::sendContactMessage, qApp, [this](const std::string &contactId, const BinaryData &data) {
         peer2_.otc_->processContactMessage(peer1_.name_, data);
      }, Qt::QueuedConnection);
      QObject::connect(peer2_.otc_.get(), &OtcClient::sendContactMessage, qApp, [this](const std::string &contactId, const BinaryData &data) {
         peer1_.otc_->processContactMessage(peer2_.name_, data);
      }, Qt::QueuedConnection);

      QObject::connect(peer1_.otc_.get(), &OtcClient::sendPbMessage, qApp, [this, processPbMessage](const std::string &data) {
         processPbMessage(peer1_, data);
      }, Qt::QueuedConnection);
      QObject::connect(peer2_.otc_.get(), &OtcClient::sendPbMessage, qApp, [this, processPbMessage](const std::string &data) {
         processPbMessage(peer2_, data);
      }, Qt::QueuedConnection);
   }

   void sendStateUpdate(ProxyTerminalPb::OtcState state)
   {
      ProxyTerminalPb::Response response;
      auto d = response.mutable_update_otc_state();
      d->set_settlement_id(kSettlementId);
      d->set_state(state);
      d->set_timestamp_ms(QDateTime::currentDateTime().toMSecsSinceEpoch());
      peer1_.otc_->processPbMessage(response);
      peer2_.otc_->processPbMessage(response);
   }

   void mineNewBlocks(const bs::Address &dst, unsigned count)
   {
      auto curHeight = env_->armoryConnection()->topBlock();
      auto addrRecip = dst.getRecipient(bs::XBTAmount{uint64_t(1 * COIN)});
      env_->armoryInstance()->mineNewBlock(addrRecip.get(), count);
      env_->blockMonitor()->waitForNewBlocks(curHeight + count);
   }

   void mineRandomBlocks(unsigned count)
   {
      mineNewBlocks(bs::Address(CryptoPRNG::generateRandom(20), AddressEntryType_P2WPKH), count);
   }

   void doOtcTest(bool sellerOffers, bool nativeAddr)
   {
      peer1_.otc_->contactConnected(peer2_.name_);
      peer2_.otc_->contactConnected(peer1_.name_);

      auto &sender = sellerOffers ? peer1_ : peer2_;
      auto &receiver = sellerOffers ? peer2_ : peer1_;

      const auto &addr = nativeAddr ? peer1_.nativeAddr_ : peer1_.nestedAddr_;
      auto wallet = peer1_.syncWalletMgr_->getWalletByAddress(addr);
      ASSERT_TRUE(wallet);

      mineNewBlocks(addr, 1);
      mineRandomBlocks(6);

      auto promSync = std::promise<bool>();
      wallet->updateBalances([&promSync] {
         promSync.set_value(true);
      });
      promSync.get_future().wait();

      auto balance = wallet->getSpendableBalance();
      ASSERT_TRUE(balance != 0);

      // needed to be able sign pay-in and pay-out
      const bs::core::WalletPasswordScoped lock1(peer1_.wallet_, kPassword);
      const bs::core::WalletPasswordScoped lock2(peer2_.wallet_, kPassword);

      {
         bs::network::otc::Offer offer;
         offer.price = 100;
         offer.amount = 1000;
         offer.ourSide = sellerOffers ? bs::network::otc::Side::Sell : bs::network::otc::Side::Buy;
         offer.hdWalletId = sender.wallet_->walletId();
         offer.authAddress = sender.authAddress_.display();
         sender.otc_->sendOffer(sender.otc_->contact(receiver.name_), offer);
         QApplication::processEvents();
      }

      auto remotePeer = receiver.otc_->contact(sender.name_);
      ASSERT_TRUE(remotePeer);
      ASSERT_TRUE(remotePeer->state == otc::State::OfferRecv);

      {
         bs::network::otc::Offer offer;
         offer.price = 100;
         offer.amount = 1000;
         offer.ourSide = sellerOffers ? bs::network::otc::Side::Buy : bs::network::otc::Side::Sell;
         offer.hdWalletId = receiver.wallet_->walletId();
         offer.authAddress = receiver.authAddress_.display();
         receiver.otc_->acceptOffer(receiver.otc_->contact(sender.name_), offer);
         QApplication::processEvents();
      }

      auto time = std::chrono::steady_clock::now();
      while (std::chrono::steady_clock::now() - time < std::chrono::seconds(5) && !quit_) {
         QApplication::processEvents();
      }

      ASSERT_TRUE(verifyDone_);
      ASSERT_TRUE(payoutDone_);
      ASSERT_TRUE(payinSealDone_);
      ASSERT_TRUE(payinDone_);
   }

   std::unique_ptr<TestEnv> env_;
   TestPeer peer1_;
   TestPeer peer2_;
   SettableField<ProxyTerminalPb::Request::VerifyOtc> verifySeller_;
   SettableField<ProxyTerminalPb::Request::VerifyOtc> verifyBuyer_;
   uint64_t totalFee_{};
   bool verifyDone_{};
   bool payoutDone_{};
   bool payinSealDone_{};
   bool payinDone_{};
   std::atomic_bool quit_{false};
};

TEST_F(TestOtc, SellNative)
{
   doOtcTest(true, true);
}

TEST_F(TestOtc, BuyNative)
{
   doOtcTest(false, true);
}

TEST_F(TestOtc, SellNested)
{
   doOtcTest(true, false);
}

TEST_F(TestOtc, BuyNested)
{
   doOtcTest(false, false);
}

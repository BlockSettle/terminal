#include <QApplication>

#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "SettableField.h"
#include "TestEnv.h"
#include "Trading/OtcClient.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "bs_proxy_terminal_pb.pb.h"

using namespace bs::network;
using namespace Blocksettle::Communication;

class TestPeer
{
public:
   void init(TestEnv &env, const std::string &name)
   {
      name_ = name;
      bs::core::wallet::Seed seed(name, NetworkType::TestNet);

      bs::wallet::PasswordData pd;
      pd.password = SecureBinaryData("passphrase");
      pd.metaData.encType = bs::wallet::EncryptionType::Password;

      wallet_ = std::make_shared<bs::core::hd::Wallet>(name, "", seed, pd, env.armoryInstance()->homedir_);
      auto group = wallet_->createGroup(bs::hd::CoinType::BlockSettle_Auth);
      auto xbtGroup = wallet_->createGroup(wallet_->getXBTGroupType());

      auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
      authGroup->setSalt(CryptoPRNG::generateRandom(32));

      {
         const bs::core::WalletPasswordScoped lock(wallet_, pd.password);
         auto authLeaf = authGroup->createLeaf(AddressEntryType_Default, 0);
         authAddress_ = authLeaf->getNewExtAddress();

         auto settlementLeaf = wallet_->createSettlementLeaf(authAddress_);
         ASSERT_TRUE(settlementLeaf);
         ASSERT_TRUE(settlementLeaf->hasExtOnlyAddresses());

         auto xbtLeaf = xbtGroup->createLeaf(AddressEntryType_P2WPKH, 0);
         xbtAddress_ = xbtLeaf->getNewExtAddress();
         ASSERT_TRUE(!xbtAddress_.isNull());
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
   bs::Address xbtAddress_;

};

class TestOtc : public ::testing::Test
{
public:
   void SetUp() override
   {
      env_ = std::make_unique<TestEnv>(StaticLogger::loggerPtr);
      env_->requireArmory();

      peer1_.init(*env_, "test1");
      peer2_.init(*env_, "test2");

      QObject::connect(peer1_.otc_.get(), &OtcClient::sendContactMessage, [this](const std::string &contactId, const BinaryData &data) {
         peer2_.otc_->processContactMessage(peer1_.name_, data);
      });
      QObject::connect(peer2_.otc_.get(), &OtcClient::sendContactMessage, [this](const std::string &contactId, const BinaryData &data) {
         peer1_.otc_->processContactMessage(peer2_.name_, data);
      });

      auto processPbMessage = [this](TestPeer &peer, const std::string &data) {
         ProxyTerminalPb::Request request;
         ASSERT_TRUE(request.ParseFromString(data));
         ProxyTerminalPb::Response response;

         switch (request.data_case()) {
            case ProxyTerminalPb::Request::kStartOtc: {
               auto d = response.mutable_start_otc();
               d->set_request_id(request.start_otc().request_id());
               d->set_settlement_id("dc26c004d7b24f71cd5b348a254c292777586f5d9d00f60ac65dd7d5b06d0c2b");
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
               }

               break;
            }
            default:
               ASSERT_TRUE(false);
         }
      };

      QObject::connect(peer1_.otc_.get(), &OtcClient::sendPbMessage, [this, processPbMessage](const std::string &data) {
         processPbMessage(peer1_, data);
      });
      QObject::connect(peer2_.otc_.get(), &OtcClient::sendPbMessage, [this, processPbMessage](const std::string &data) {
         processPbMessage(peer2_, data);
      });
   }

   std::unique_ptr<TestEnv> env_;
   TestPeer peer1_;
   TestPeer peer2_;
   SettableField<ProxyTerminalPb::Request::VerifyOtc> verifySeller_;
   SettableField<ProxyTerminalPb::Request::VerifyOtc> verifyBuyer_;
   bool verifyDone_{false};
};

TEST_F(TestOtc, BasicTest)
{
   UnitTestWalletACT::clear();

   peer1_.otc_->contactConnected(peer2_.name_);
   peer2_.otc_->contactConnected(peer1_.name_);

   const int blockCount = 6;
   auto curHeight = env_->armoryConnection()->topBlock();
   auto addrRecip = peer1_.xbtAddress_.getRecipient(bs::XBTAmount{ (uint64_t)(50 * COIN) });
   env_->armoryInstance()->mineNewBlock(addrRecip.get(), blockCount);
   env_->blockMonitor()->waitForNewBlocks(curHeight + blockCount);

   auto wallet = peer1_.syncWalletMgr_->getDefaultWallet();
   ASSERT_TRUE(wallet);

   auto promSync = std::promise<bool>();
   wallet->updateBalances([&promSync] {
      promSync.set_value(true);
   });
   promSync.get_future().wait();

   auto balance = wallet->getSpendableBalance();
   ASSERT_TRUE(balance != 0);

   {
      bs::network::otc::Offer offer;
      offer.price = 100;
      offer.amount = 1000;
      offer.ourSide = bs::network::otc::Side::Sell;
      offer.hdWalletId = peer1_.wallet_->walletId();
      offer.authAddress = peer1_.authAddress_.display();
      peer1_.otc_->sendOffer(peer1_.otc_->contact(peer2_.name_), offer);
   }

   auto remotePeer1 = peer2_.otc_->contact(peer1_.name_);
   ASSERT_TRUE(remotePeer1);
   ASSERT_TRUE(remotePeer1->state == otc::State::OfferRecv);

   {
      auto remotePeer1 = peer2_.otc_->contact(peer1_.name_);

      bs::network::otc::Offer offer;
      offer.price = 100;
      offer.amount = 1000;
      offer.ourSide = bs::network::otc::Side::Buy;
      offer.hdWalletId = peer2_.wallet_->walletId();
      offer.authAddress = peer2_.authAddress_.display();
      peer2_.otc_->acceptOffer(remotePeer1, offer);
   }

   auto time = std::chrono::steady_clock::now();
   while (std::chrono::steady_clock::now() - time < std::chrono::seconds(1)) {
      QApplication::processEvents();
   }
   ASSERT_TRUE(verifyDone_);
}

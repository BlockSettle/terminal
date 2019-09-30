#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "TestEnv.h"
#include "Trading/OtcClient.h"
#include "Wallets/SyncWalletsManager.h"

using namespace bs::network;

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

         auto leaf = wallet_->createSettlementLeaf(authAddress_);
         ASSERT_TRUE(leaf != nullptr);
         ASSERT_TRUE(leaf->hasExtOnlyAddresses());
      }

      env.walletsMgr()->addWallet(wallet_);

      signer_ = std::make_shared<InprocSigner>(env.walletsMgr(), env.logger(), "", NetworkType::TestNet);
      signer_->Start();

      syncWalletMgr_ = std::make_shared<bs::sync::WalletsManager>(env.logger()
         , env.appSettings(), env.armoryConnection());
      syncWalletMgr_->setSignContainer(signer_);
      syncWalletMgr_->syncWallets();

      OtcClientParams params;
      otc_ = std::make_shared<OtcClient>(env.logger(), syncWalletMgr_, env.armoryConnection(), signer_, nullptr, params);
   }

   std::string name_;
   std::shared_ptr<bs::core::hd::Wallet> wallet_;
   std::shared_ptr<bs::sync::WalletsManager> syncWalletMgr_;
   std::shared_ptr<InprocSigner> signer_;
   std::shared_ptr<OtcClient> otc_;
   bs::Address authAddress_;

};

class TestOtc : public ::testing::Test
{
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
   }

public:
   std::unique_ptr<TestEnv> env_;
   TestPeer peer1_;
   TestPeer peer2_;

};

TEST_F(TestOtc, BasicTest)
{
   peer1_.otc_->contactConnected(peer2_.name_);
   peer2_.otc_->contactConnected(peer1_.name_);

   bs::network::otc::Offer offer;
   offer.price = 100;
   offer.amount = 1000;
   offer.ourSide = bs::network::otc::Side::Sell;
   offer.hdWalletId = peer1_.wallet_->walletId();
   offer.authAddress = peer1_.authAddress_.display();
   peer1_.otc_->sendOffer(peer1_.otc_->contact(peer2_.name_), offer);

   auto remotePeer1 = peer2_.otc_->contact(peer1_.name_);
   ASSERT_TRUE(remotePeer1);
   ASSERT_TRUE(remotePeer1->state == otc::State::OfferRecv);
}

/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
/*
tests to write:

- create & fund auth address
- create, fund, revoke by user
- create, fund, revoke by bs
- create, fund, revoke. fund again, should not be valid again

revocation should occur at ZC. if user broadcasts revocation ZC, BS should broadcast its own too
*/

/*
**************************
DB side:

take address
1)
find all STS parents
   flag by have UTXO children or not

2)
filter out parent parents with more than 1 outpoint

3)
return all outpoints in eligible parents, with spent STS child flag

***************************
terminal side:
1)
if no outpoint returned -> invalid

2)
if no unflagged return -> revoked

3)
check flagged returns, if any outpoint is bs auth address -> revoked

4)
check unflagged return
   if count > 1:
      invalid
   else:
      if outpoint is bs auth address -> valid
*/

#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "CheckRecipSigner.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessContainer.h"
#include "InprocSigner.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "AddressVerificator.h"

#include "TestAuth.h"

using namespace ArmorySigner;

///////////////////////////////////////////////////////////////////////////////
void TestValidationACT::onRefresh(const std::vector<BinaryData>& ids, bool online)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_Refresh);
   dbns->ids_ = ids;
   dbns->online_ = online;

   notifTestQueue_.push_back(std::move(dbns));
   auto idsCopy = ids;

   const auto callbacks = callbacks_.lock();
   if (callbacks && callbacks->onRefresh) {
      callbacks->onRefresh(idsCopy);
   }
}

////
void TestValidationACT::onZCReceived(const std::string& requestId, const std::vector<bs::TXEntry>& zcs)
{
   auto dbns = std::make_shared<DBNotificationStruct>(DBNS_ZC);
   dbns->zc_ = zcs;

   notifTestQueue_.push_back(std::move(dbns));
}

///////////////////////////////////////////////////////////////////////////////
void TestAuth::mineBlocks(unsigned count, bool wait)
{
   auto curHeight = envPtr_->armoryConnection()->topBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());

   if (!wait)
      return;

   envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
}

////
BinaryData TestAuth::sendTo(uint64_t value, bs::Address& addr)
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

   signer.addRecipient(addr.getRecipient(bs::XBTAmount{value}));
   signer.setFeed(coinbaseFeed_);

   //sign & send
   signer.sign();
   auto&& txData = signer.serializeSignedTx();
   Tx txObj(txData);
   
   envPtr_->armoryInstance()->pushZC(txData);
   return txObj.getThisHash();
}

////
bs::Address TestAuth::getNewAddress(std::shared_ptr<bs::sync::Wallet> wltPtr
   , bool ext)
{
   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };
   if (ext) {
      wltPtr->getNewExtAddress(cbAddr);
   }
   else {
      wltPtr->getNewIntAddress(cbAddr);
   }
   return futAddr.get();
};

////
void TestAuth::SetUp()
{
   passphrase_ = SecureBinaryData::fromString("pass");
   
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ =
      std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);

   validationPubKey_ = CryptoECDSA().ComputePublicKey(validationPrivKey_, true);
   validationScrAddr_ = BtcUtils::getHash160(validationPubKey_);
   validationFeed_ =
      std::make_shared<ResolverOneAddress>(validationPrivKey_, validationPubKey_);

   validationAddr_ = bs::Address::fromPubKey(validationPubKey_, AddressEntryType_P2WPKH);

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();

   mineBlocks(10);

   //set auth test act ptr
   actPtr_ = std::make_shared<TestValidationACT>(envPtr_->armoryConnection().get());

   //instantiate wallet to register validation address with. we need this to 
   //catch zc events for the validation address setup prior to setting up the 
   //validation address manager
   auto walletObj = envPtr_->armoryConnection()->instantiateWallet("testWallet");
   
   auto&& regIDs = walletObj->registerAddresses({ validationAddr_.prefixed() }, false);
   actPtr_->waitOnRefresh({ regIDs });

   //fund auth genesis address
   {
      //first UTXO, should remain untouched until validation address 
      //is revoked
      auto&& hash = sendTo(10 * COIN, validationAddr_);
      actPtr_->waitOnZC(hash);
      mineBlocks(1);

      //actual spendable UTXO for vetting user auth addresses
      hash = sendTo(10 * COIN, validationAddr_);
      actPtr_->waitOnZC(hash);
      mineBlocks(6);
   }

   const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };

   //setup user wallet
   priWallet_ = envPtr_->walletsMgr()->createWallet("Primary", "",
      bs::core::wallet::Seed(CryptoPRNG::generateRandom(32), NetworkType::TestNet),
      envPtr_->armoryInstance()->homedir_, pd, true, false);

   if (!priWallet_) {
      return;
   }
   //setup auth leaf
   const auto authGroup = priWallet_->createGroup(bs::hd::CoinType::BlockSettle_Auth);
   if (!authGroup)
      return;

   auto authGroupPtr = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(authGroup);
   if (authGroupPtr == nullptr)
      return;
   authGroupPtr->setSalt(userID_);

   {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      authSignWallet_ = authGroup->createLeaf(AddressEntryType_Default, 0x41555448);
      if (!authSignWallet_)
         return;
   }

   //setup spend wallet
   const auto xbtGroup = priWallet_->getGroup(priWallet_->getXBTGroupType());
   if (!xbtGroup)
      return;

   const bs::hd::Path xbtPath({bs::hd::Purpose::Native, priWallet_->getXBTGroupType(), 0});
   const auto xbtLeaf = xbtGroup->getLeafByPath(xbtPath);
   if (!xbtLeaf) {
      return;
   }
   {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      xbtSignWallet_ = xbtGroup->createLeaf(AddressEntryType_Default, 1);
      if (!xbtSignWallet_) {
         return;
      }
   }

   //setup sync manager
   auto inprocSigner = std::make_shared<InprocSigner>(priWallet_, this, envPtr_->logger());
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger(),
      envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
}

void TestAuth::TearDown()
{
   envPtr_->walletsMgr()->deleteWalletFile(envPtr_->walletsMgr()->getPrimaryWallet());
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, ValidationAddressManager)
{
   ASSERT_FALSE(validationAddr_.empty());

   //setup the address watcher
   ValidationAddressManager maw(envPtr_->armoryConnection());
   maw.addValidationAddress(validationAddr_);
   maw.setCustomACT(actPtr_);

   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(maw.goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //check the validation address is valid
   EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

   //add a block, check validation address is still valid
   mineBlocks(1);
   maw.update();
   EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

   for (unsigned i = 0; i < 5; i++) {
      //generate user address
      BinaryData prefixed;
      prefixed.append(AddressEntry::getPrefixByte(AddressEntryType_P2WPKH));
      prefixed.append(CryptoPRNG::generateRandom(20));
      auto userAddr = bs::Address::fromHash(prefixed);

      //vet it
      try {
         auto&& txHash = maw.vetUserAddress(userAddr, validationFeed_);
         actPtr_->waitOnZC(txHash);
      }
      catch (AuthLogicException&) {
         ASSERT_FALSE(true);
      }

      //validation address should still be valid
      maw.update();
      EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

      //validation address should have no eligible outpoints for vetting at this point
      //(it only has one atm, which is zc change)
      ASSERT_FALSE(maw.hasSpendableOutputs(validationAddr_));
      ASSERT_TRUE(maw.hasZCOutputs(validationAddr_));

      //mine a block
      mineBlocks(1);

      //validation address should still be valid
      maw.update();
      EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

      //validation address should have an eligible outpoint and no zc
      ASSERT_TRUE(maw.hasSpendableOutputs(validationAddr_));
      ASSERT_FALSE(maw.hasZCOutputs(validationAddr_));
   }

   //add a few blocks, check validation address is still valid
   mineBlocks(3);
   maw.update();
   EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

   //revoke validation address
   try
   {
      auto&& txHash = maw.revokeValidationAddress(validationAddr_, validationFeed_);
      actPtr_->waitOnZC(txHash);
   }
   catch (AuthLogicException&)
   {
      ASSERT_FALSE(true);
   }

   //should still be valid prior to update
   EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

   /*
   Should be invalid after update. There should be 2 new
   outpoints cause the revoke tx spends the coins back to
   the validation address. 
   
   In revocations, only spending the first utxo matters,
   not where it is spent to.
   */
   maw.update();
   EXPECT_FALSE(maw.isValidMasterAddress(validationAddr_));

   //same thing once the revocation is mined
   mineBlocks(1);
   maw.update();
   EXPECT_FALSE(maw.isValidMasterAddress(validationAddr_));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, ValidateUserAddress)
{
   //sync wallets
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr_->syncWallets(cbSync);
   futSync.wait();

   authWallet_ = syncMgr_->getWalletById(authSignWallet_->walletId());
   xbtWallet_ = syncMgr_->getDefaultWallet();

   ASSERT_NE(authWallet_, nullptr);
   ASSERT_FALSE(validationAddr_.empty());

   //setup the address watcher
   ValidationAddressManager vam(envPtr_->armoryConnection());
   vam.setCustomACT(actPtr_);
   vam.addValidationAddress(validationAddr_);

   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(vam.goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //check the validation address is valid
   EXPECT_TRUE(vam.isValidMasterAddress(validationAddr_));

   auto promPtr = std::make_shared<std::promise<bs::Address>>();
   auto fut = promPtr->get_future();
   auto addrCb = [&promPtr](const bs::Address& addr)->void
   {
      promPtr->set_value(addr);
   };

   authWallet_->getNewExtAddress(addrCb);
   auto&& userAddr = fut.get();

   //validation leg
   try
   {
      actPtr_->waitOnZC(vam.vetUserAddress(userAddr, validationFeed_));
   }
   catch (AuthLogicException&)
   {
      ASSERT_FALSE(true);
   }
   vam.update();

   //check auth address is valid, should fail as it needs 6 blocks
   ASSERT_FALSE(AuthAddressLogic::isValid(vam, userAddr));

   //mine 6 blocks
   mineBlocks(6);
   vam.update();

   //check auth address again, should be valid now
   ASSERT_TRUE(AuthAddressLogic::isValid(vam, userAddr));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, BadUserAddress)
{
   //sync wallets
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr_->syncWallets(cbSync);
   futSync.wait();

   authWallet_ = syncMgr_->getWalletById(authSignWallet_->walletId());
   xbtWallet_ = syncMgr_->getDefaultWallet();
   auto hdWallet = syncMgr_->getPrimaryWallet();
   hdWallet->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
   ASSERT_NE(authWallet_, nullptr);

   //register wallets
   UnitTestWalletACT::clear();
   const auto regIDs = syncMgr_->registerWallets();
   UnitTestWalletACT::waitOnRefresh(regIDs);

   ASSERT_FALSE(validationAddr_.empty());

   //setup the address watcher
   ValidationAddressManager maw(envPtr_->armoryConnection());
   maw.setCustomACT(actPtr_);
   maw.addValidationAddress(validationAddr_);

   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(maw.goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //check the validation address is valid
   EXPECT_TRUE(maw.isValidMasterAddress(validationAddr_));

   //generate user address
   ASSERT_NE(authWallet_, nullptr);

   auto getNewAuthAddress = [this](void)->bs::Address
   {
      auto promPtr = std::make_shared<std::promise<bs::Address>>();
      auto fut = promPtr->get_future();
      auto addrCb = [&promPtr](const bs::Address& addr)->void
      {
         promPtr->set_value(addr);
      };
      authWallet_->getNewExtAddress(addrCb);
      return fut.get();
   };

   auto&& userAddr1 = getNewAuthAddress();
   auto&& userAddr2 = getNewAuthAddress();
   auto&& userAddr3 = getNewAuthAddress();

   //send coins to userAddr1
   auto&& zcHash = sendTo(COIN, userAddr1);
   while (true) 
   {
      auto&& zcVec = UnitTestWalletACT::waitOnZC(true);
      if (zcVec.size() != 1)
         continue;
      
      if (zcVec[0].txHash == zcHash)
         break;
   }

   /*
   validation leg on userAddr1 should fail since it has history.
   No need to call ValidationAddressManager::update after this failed vetting,
   as it didn't lead to any blockchain event (no zc was pushed).
   */
   try {
      auto&& txHash = maw.vetUserAddress(userAddr1, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   /*
   Vetting should succeed on userAddr2. Need to update since there was a zc
   pushed to the chain.
   */
   try {
      auto&& txHash = maw.vetUserAddress(userAddr2, validationFeed_);
      maw.update();
      actPtr_->waitOnZC(txHash);
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   /*
   Trying to vet userAddr2 should fail this time around.
   */

   try {
      auto&& txHash = maw.vetUserAddress(userAddr2, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   //we dont need to call update, but it won't corrupt the state to do so
   maw.update();

   //mine a new block and check state
   mineBlocks(VALIDATION_CONF_COUNT);
   maw.update();

   {
      auto userAddr1_state = AuthAddressLogic::getAuthAddrState(maw, userAddr1);
      EXPECT_EQ(userAddr1_state, AddressVerificationState::Tainted);

      auto userAddr2_state = AuthAddressLogic::getAuthAddrState(maw, userAddr2);
      EXPECT_EQ(userAddr2_state, AddressVerificationState::Verified);

      //empty address should be invalid by default
      auto userAddr3_state = AuthAddressLogic::getAuthAddrState(maw, userAddr3);
      EXPECT_EQ(userAddr3_state, AddressVerificationState::Virgin);
   }

   try {
      auto&& txHash = maw.vetUserAddress(userAddr1, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, Revoke)
{
   ASSERT_FALSE(validationAddr_.empty());

   //we need a second validation auth address for this test
   SecureBinaryData privKey2 =
      READHEX("5555555555555555555555555555555555555555555555555555555555555555");
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);
   auto validationAddr2 = bs::Address::fromPubKey(pubkey2, AddressEntryType_P2WPKH);
   
   auto&& validationFeed2 =
      std::make_shared<ResolverOneAddress>(privKey2, pubkey2);

   {
      //first UTXO, should remain untouched until validation address 
      //is revoked
      sendTo(10 * COIN, validationAddr2);
      mineBlocks(1);

      //actual spendable UTXO for vetting user auth addresses
      sendTo(10 * COIN, validationAddr2);
      mineBlocks(6);
   }

   //sync wallets
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr_->syncWallets(cbSync);
   futSync.wait();

   authWallet_ = syncMgr_->getWalletById(authSignWallet_->walletId());
   xbtWallet_ = syncMgr_->getDefaultWallet();

   ASSERT_NE(authWallet_, nullptr);

   ValidationAddressManager vam(envPtr_->armoryConnection());
   vam.setCustomACT(actPtr_);
   vam.addValidationAddress(validationAddr_);
   vam.addValidationAddress(validationAddr2);

   //get some user addresses to vet
   auto getNewAuthAddress = [this](void)->bs::Address
   {
      auto promPtr = std::make_shared<std::promise<bs::Address>>();
      auto fut = promPtr->get_future();
      auto addrCb = [&promPtr](const bs::Address& addr)->void
      {
         promPtr->set_value(addr);
      };
      authWallet_->getNewExtAddress(addrCb);
      return fut.get();
   };

   auto&& userAddr1 = getNewAuthAddress();
   auto&& userAddr2 = getNewAuthAddress();
   auto&& userAddr3 = getNewAuthAddress();
   auto&& userAddr4 = getNewAuthAddress();
   auto&& userAddr5 = getNewAuthAddress();
   auto&& userAddr6 = getNewAuthAddress();

   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(vam.goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //vet them all
   try {
      auto&& txHash1 = vam.vetUserAddress(userAddr1, validationFeed_);
      vam.update();
      actPtr_->waitOnZC(txHash1);

      auto&& txHash2 = vam.vetUserAddress(userAddr4, validationFeed2);
      vam.update();
      actPtr_->waitOnZC(txHash2);
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   //there should be no valid utxos left for vetting at this point
   try {
      vam.vetUserAddress(userAddr2, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   try {
      vam.vetUserAddress(userAddr5, validationFeed2);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   mineBlocks(1);

   try {
      auto&& txHash1 = vam.vetUserAddress(userAddr2, validationFeed_);
      vam.update();
      actPtr_->waitOnZC(txHash1);

      auto&& txHash2 = vam.vetUserAddress(userAddr5, validationFeed2);
      vam.update();
      actPtr_->waitOnZC(txHash2);
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   mineBlocks(1);

   try {
      auto&& txHash1 = vam.vetUserAddress(userAddr3, validationFeed_);
      vam.update();
      actPtr_->waitOnZC(txHash1);

      auto&& txHash2 = vam.vetUserAddress(userAddr6, validationFeed2);
      vam.update();
      actPtr_->waitOnZC(txHash2);
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }
  
   mineBlocks(VALIDATION_CONF_COUNT);
   vam.update();

   //all these addresses should be valid now
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr1));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr2));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr3));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr4));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr5));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr6));

   //user revoke
   try {
      BinaryData txRevokeHash;
      {
         const bs::core::WalletPasswordScoped lockPass(priWallet_, passphrase_);
         const auto lock = authSignWallet_->lockDecryptedContainer();
         txRevokeHash = AuthAddressLogic::revoke(vam, userAddr1
            , authSignWallet_->getResolver());
      }
      
      /*
      Since auth address checks are a wallet agnostic process, 
      auth addresses do not need to be registered for the stack
      to run, and therefor are not. 
      
      As a result for the scope of these unit tests, no ZC 
      impacting only user auth addresses will result in a 
      notification. 

      User revokes as implemented in AuthAddressLogic burn the 
      validation UTXO as fee, therefor the test has no 
      standardized way to wait on a signal notifying of this
      revocation.
      
      We shall simply sleep the unit test thread an 
      appropriate amount of time and expect the ZC was 
      processed.

      It is the responsibility of the user to register auth 
      addresses for which he wants to receive ZC notifications. 
      Typically, users and settlement monitoring services would 
      register counterparty addresses to detect zc revokes.
      */
      std::this_thread::sleep_for(std::chrono::seconds(2));
      vam.update();

      auto userAddr1_state = 
         AuthAddressLogic::getAuthAddrState(vam, userAddr1);
      EXPECT_EQ(userAddr1_state, AddressVerificationState::Revoked);
   }
   catch (const std::exception &) {
      ASSERT_TRUE(false);
   }

   //revoke user address through its validation address
   try {
      auto&& txHash = vam.revokeUserAddress(userAddr2, validationFeed_);
      actPtr_->waitOnZC(txHash);
      vam.update();
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }

      auto userAddr2_state = 
         AuthAddressLogic::getAuthAddrState(vam, userAddr2);
      EXPECT_EQ(userAddr2_state, AddressVerificationState::Invalidated_Explicit);

   //try to revoke revoked addresses, should fail
   try {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      AuthAddressLogic::revoke(vam, userAddr1, authSignWallet_->getResolver());
      ASSERT_TRUE(false);
   }
   catch(std::exception&)
   {}

   try {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      AuthAddressLogic::revoke(vam, userAddr2, authSignWallet_->getResolver());
      ASSERT_TRUE(false);
   }
   catch (std::exception&)
   {}

   try {
      vam.revokeUserAddress(userAddr1, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   try {
      vam.revokeUserAddress(userAddr2, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   //mine a new block and check states. new block shouldn't change anything.
   mineBlocks(1);
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr1));
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr2));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr3));

   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr4));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr5));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr6));

   //try to revoke revoked addresses again, should still fail
   try {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      AuthAddressLogic::revoke(vam, userAddr1, authSignWallet_->getResolver());
      ASSERT_TRUE(false);
   }
   catch (std::exception&) {}

   try {
      const bs::core::WalletPasswordScoped lock(priWallet_, passphrase_);
      AuthAddressLogic::revoke(vam, userAddr2, authSignWallet_->getResolver());
      ASSERT_TRUE(false);
   }
   catch (std::exception&) {}

   try {
      vam.revokeUserAddress(userAddr1, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   try {
      vam.revokeUserAddress(userAddr2, validationFeed_);
      ASSERT_TRUE(false);
   }
   catch (AuthLogicException&) {}

   //revoke the validation address itself
   try {
      auto&& txHash = vam.revokeValidationAddress(validationAddr2, validationFeed2);
      actPtr_->waitOnZC(txHash);
      vam.update();
   }
   catch (AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr1));
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr2));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr3));

   {
      auto userAddr4_state = 
         AuthAddressLogic::getAuthAddrState(vam, userAddr4);
      EXPECT_EQ(userAddr4_state, AddressVerificationState::Invalidated_Implicit);

      auto userAddr5_state = 
         AuthAddressLogic::getAuthAddrState(vam, userAddr5);
      EXPECT_EQ(userAddr5_state, AddressVerificationState::Invalidated_Implicit);

      auto userAddr6_state = 
         AuthAddressLogic::getAuthAddrState(vam, userAddr6);
      EXPECT_EQ(userAddr6_state, AddressVerificationState::Invalidated_Implicit);
   }

   //mine a block and check states. new block shouldn't change anything.
   mineBlocks(1);

   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr1));
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr2));
   EXPECT_TRUE(AuthAddressLogic::isValid(vam, userAddr3));

   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr4));
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr5));
   EXPECT_FALSE(AuthAddressLogic::isValid(vam, userAddr6));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, Concurrency)
{
   ASSERT_FALSE(validationAddr_.empty());

   //we need a second validation auth address for this test
   SecureBinaryData privKey2 =
      READHEX("5555555555555555555555555555555555555555555555555555555555555555");
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);
   auto validationAddr2 = bs::Address::fromPubKey(pubkey2, AddressEntryType_P2WPKH);

   SecureBinaryData privKey3 =
      READHEX("6666666666666666666666666666666666666666666666666666666666666666");
   auto&& pubkey3 = CryptoECDSA().ComputePublicKey(privKey3, true);
   auto validationAddr3 = bs::Address::fromPubKey(pubkey3, AddressEntryType_P2WPKH);

   std::set<SecureBinaryData> authPrivKeys =
   {
      privKey2, privKey3, validationPrivKey_
   };
   auto resolverMam = std::make_shared<ResolverManyAddresses>(authPrivKeys);

   {
      //first UTXO, should remain untouched until validation address 
      //is revoked
      sendTo(10 * COIN, validationAddr2);
      sendTo(10 * COIN, validationAddr3);
      mineBlocks(1);

      //actual spendable UTXO for vetting user auth addresses
      sendTo(10 * COIN, validationAddr2);
      sendTo(10 * COIN, validationAddr3);
      mineBlocks(6);
   }

   //sync wallets
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr_->syncWallets(cbSync);
   futSync.wait();

   authWallet_ = syncMgr_->getWalletById(authSignWallet_->walletId());
   xbtWallet_ = syncMgr_->getDefaultWallet();

   ASSERT_NE(authWallet_, nullptr);

   auto vam = std::make_shared<ValidationAddressManager>(envPtr_->armoryConnection());
   vam->setCustomACT(actPtr_);
   vam->addValidationAddress(validationAddr_);
   vam->addValidationAddress(validationAddr2);
   vam->addValidationAddress(validationAddr3);

   /*validation leg*/

   //get some user addresses to vet
   auto getNewAuthAddress = [this](void)->bs::Address
   {
      auto promPtr = std::make_shared<std::promise<bs::Address>>();
      auto fut = promPtr->get_future();
      auto addrCb = [&promPtr](const bs::Address& addr)->void
      {
         promPtr->set_value(addr);
      };
      authWallet_->getNewExtAddress(addrCb);
      return fut.get();
   };

   std::vector<bs::Address> authAddresses;
   for (unsigned i = 0; i < 6; i++) {
      authAddresses.push_back(getNewAuthAddress());
   }
   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(vam->goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //start verifications in side threads, they should return
   //once the addr is valid
   auto checkAddressValid = [vam](const bs::Address& addr)->void
   {
      while (!AuthAddressLogic::isValid(*vam, addr)); 
   };
   std::vector<std::thread> threads;
   for (auto& authAddr : authAddresses) {
      threads.push_back(std::thread(checkAddressValid, authAddr));
   }

   //make sure addresses are invalid
   for (auto& addr : authAddresses) {
      EXPECT_FALSE(AuthAddressLogic::isValid(*vam, addr));
   }

   //vet all user auth addresses
   try {
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[0], resolverMam, validationAddr_));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[1], resolverMam, validationAddr2));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[2], resolverMam, validationAddr3));
   }
   catch (const AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   mineBlocks(1);
   vam->update();

   try {
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[3], resolverMam, validationAddr_));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[4], resolverMam, validationAddr2));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[5], resolverMam, validationAddr3));
   }
   catch (const AuthLogicException &) {
      ASSERT_TRUE(false);
   }

   mineBlocks(VALIDATION_CONF_COUNT);
   vam->update();

   //wait on all verification threads and check addresses are valid
   for (auto& thr : threads) {
      if (thr.joinable())
         thr.join();
   }

   for (const auto &addr : authAddresses) {
      EXPECT_TRUE(AuthAddressLogic::isValid(*vam, addr));
   }

   /*revocation leg*/

   //revoke lambda
   auto counterPtr = std::make_shared<std::atomic<unsigned>>();
   counterPtr->store(0);
   auto checkAddressRevoked = [&vam, counterPtr](const bs::Address& addr)->void
   {
      while (true) {
         if (!AuthAddressLogic::isValid(*vam, addr)) {
            counterPtr->fetch_add(1);
            return;
         }
      }
   };

   //start revoke check threads
   threads.clear();
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[0]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[2]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[4]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[5]));

   //mine some blocks, shouldnt affect state
   mineBlocks(1);
   mineBlocks(2);
   mineBlocks(1);
   vam->update();

   ASSERT_EQ(counterPtr->load(), 0);

   const auto priWallet = envPtr_->walletsMgr()->getPrimaryWallet();

   //revoke some addresses
   try {
      const bs::core::WalletPasswordScoped scopedPass(priWallet, passphrase_);
      auto lock = authSignWallet_->lockDecryptedContainer();
      AuthAddressLogic::revoke(*vam, authAddresses[0], authSignWallet_->getResolver());
   }
   catch (const std::exception &e) {
      ASSERT_FALSE(true);
   }
   actPtr_->waitOnZC(vam->revokeUserAddress(authAddresses[4], resolverMam));
   actPtr_->waitOnZC(vam->revokeValidationAddress(validationAddr3, resolverMam));
   vam->update();

   //watcher threads should have returned by now
   for (auto& thr : threads) {
      if (thr.joinable())
         thr.join();
   }

   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[0]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[1]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[2]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[3]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[4]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[5]));
   ASSERT_EQ(counterPtr->load(), 4);

   //Mine a block, check state hasn't changed.
   mineBlocks(1);
   vam->update();

   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[0]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[1]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[2]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[3]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[4]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[5]));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(TestAuth, Concurrency_WithACT)
{
   /*
   This test specifically let's the ValidationAddressManager handle db
   notifications on its own. 
   
   vam->update() should never be called manually, the maintenance 
   ValidationAdressManager event thread will be handling it.
   */

   ASSERT_FALSE(validationAddr_.empty());

   //we need a second validation auth address for this test
   SecureBinaryData privKey2 =
      READHEX("5555555555555555555555555555555555555555555555555555555555555555");
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);
   auto validationAddr2 = bs::Address::fromPubKey(pubkey2, AddressEntryType_P2WPKH);

   SecureBinaryData privKey3 =
      READHEX("6666666666666666666666666666666666666666666666666666666666666666");
   auto&& pubkey3 = CryptoECDSA().ComputePublicKey(privKey3, true);
   auto validationAddr3 = bs::Address::fromPubKey(pubkey3, AddressEntryType_P2WPKH);

   std::set<SecureBinaryData> authPrivKeys =
   {
      privKey2, privKey3, validationPrivKey_
   };
   auto resolverMam = std::make_shared<ResolverManyAddresses>(authPrivKeys);

   {
      //first UTXO, should remain untouched until validation address 
      //is revoked
      sendTo(10 * COIN, validationAddr2);
      sendTo(10 * COIN, validationAddr3);
      mineBlocks(1);

      //actual spendable UTXO for vetting user auth addresses
      sendTo(10 * COIN, validationAddr2);
      sendTo(10 * COIN, validationAddr3);
      mineBlocks(6);
   }

   //sync wallets
   auto promSync = std::make_shared<std::promise<bool>>();
   auto futSync = promSync->get_future();
   const auto &cbSync = [this, promSync](int cur, int total) {
      if (cur == total) {
         promSync->set_value(true);
      }
   };
   syncMgr_->syncWallets(cbSync);
   futSync.wait();

   authWallet_ = syncMgr_->getWalletById(authSignWallet_->walletId());
   xbtWallet_ = syncMgr_->getDefaultWallet();

   ASSERT_NE(authWallet_, nullptr);

   auto vam = std::make_shared<ValidationAddressManager>(envPtr_->armoryConnection());
   vam->addValidationAddress(validationAddr_);
   vam->addValidationAddress(validationAddr2);
   vam->addValidationAddress(validationAddr3);

   /*validation leg*/

   //get some user addresses to vet
   auto getNewAuthAddress = [this](void)->bs::Address
   {
      auto promPtr = std::make_shared<std::promise<bs::Address>>();
      auto fut = promPtr->get_future();
      auto addrCb = [&promPtr](const bs::Address& addr)->void
      {
         promPtr->set_value(addr);
      };
      authWallet_->getNewExtAddress(addrCb);
      return fut.get();
   };

   std::vector<bs::Address> authAddresses;
   for (unsigned i = 0; i < 6; i++) {
      authAddresses.push_back(getNewAuthAddress());
   }
   //go online
   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   ASSERT_TRUE(vam->goOnline([promOnline](bool result) { promOnline->set_value(result); }));
   EXPECT_TRUE(futOnline.get());

   //start verifications in side threads, they should return
   //once the address is valid
   auto checkAddressValid = [vam](const bs::Address& addr)->void
   {
      while (!AuthAddressLogic::isValid(*vam, addr));
   };
   std::vector<std::thread> threads;
   for (auto& authAddr : authAddresses) {
      threads.push_back(std::thread(checkAddressValid, authAddr));
   }

   //make sure addresses are invalid
   for (auto& addr : authAddresses) {
      EXPECT_FALSE(AuthAddressLogic::isValid(*vam, addr));
   }

   //vet all user auth addresses
   try {
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[0], resolverMam, validationAddr_));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[1], resolverMam, validationAddr2));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[2], resolverMam, validationAddr3));
   }
   catch (const AuthLogicException&) {
      ASSERT_TRUE(false);
   }

   /*
   Mine a block and sleep the main for 2 seconds to wait on the vam
   processing the notification. This is a bandaid solution, it may be
   worth the time to implement a ACT child class that passes the 
   notifications through the unit test for proper waiting.
   */
   mineBlocks(1, false);
   std::this_thread::sleep_for(std::chrono::seconds(2));

   
   try {
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[3], resolverMam, validationAddr_));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[4], resolverMam, validationAddr2));
      actPtr_->waitOnZC(vam->vetUserAddress(
         authAddresses[5], resolverMam, validationAddr3));
   }
   catch (const AuthLogicException &) {
      ASSERT_TRUE(false);
   }

   /*
   No need to wait on vam process here, the thread joins will return
   only once the addresses are valid.
   */
   mineBlocks(VALIDATION_CONF_COUNT, false);
   for (auto& thr : threads) {
      if (thr.joinable())
         thr.join();
   }

   for (const auto &addr : authAddresses) {
      EXPECT_TRUE(AuthAddressLogic::isValid(*vam, addr));
   }

   /*revocation leg*/

   //revoke lambda
   auto counterPtr = std::make_shared<std::atomic<unsigned>>();
   counterPtr->store(0);
   auto checkAddressRevoked = [&vam, counterPtr](const bs::Address& addr)->void
   {
      while (true) {
         if (!AuthAddressLogic::isValid(*vam, addr)) {
            counterPtr->fetch_add(1);
            return;
         }
      }
   };

   //start revoke check threads
   threads.clear();
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[0]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[2]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[4]));
   threads.push_back(std::thread(checkAddressRevoked, authAddresses[5]));

   //mine some blocks, shouldnt affect state
   mineBlocks(1);
   mineBlocks(2);
   mineBlocks(1);

   ASSERT_EQ(counterPtr->load(), 0);

   const auto priWallet = envPtr_->walletsMgr()->getPrimaryWallet();

   //revoke some addresses
   try {
      const bs::core::WalletPasswordScoped scopedPass(priWallet, passphrase_);
      auto lock = authSignWallet_->lockDecryptedContainer();
      AuthAddressLogic::revoke(*vam, authAddresses[0], authSignWallet_->getResolver());
   }
   catch (const std::exception &e) {
      ASSERT_FALSE(true);
   }
   vam->revokeUserAddress(authAddresses[4], resolverMam);
   vam->revokeValidationAddress(validationAddr3, resolverMam);

   //join on watcher threads
   for (auto& thr : threads) {
      if (thr.joinable())
         thr.join();
   }

   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[0]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[1]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[2]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[3]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[4]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[5]));
   ASSERT_EQ(counterPtr->load(), 4);

   //Mine a block, check state hasn't changed.
   mineBlocks(1, false);
   std::this_thread::sleep_for(std::chrono::seconds(2));

   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[0]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[1]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[2]));
   EXPECT_TRUE(AuthAddressLogic::isValid(*vam, authAddresses[3]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[4]));
   EXPECT_FALSE(AuthAddressLogic::isValid(*vam, authAddresses[5]));
}

//reorg & zc replacement test

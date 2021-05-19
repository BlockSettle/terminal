/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TestCCoinAsync.h"
#include <QDebug>
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

using namespace ArmorySigner;

TestCCoinAsync::TestCCoinAsync()
{}

void TestCCoinAsync::SetUp()
{
   // critical! clear events queue between tests run!
   UnitTestWalletACT::clear();

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();

   passphrase_ = SecureBinaryData::fromString("pass");

   // setup mining
   coinbasePubKey_ = CryptoECDSA().ComputePublicKey(coinbasePrivKey_, true);
   coinbaseScrAddr_ = BtcUtils::getHash160(coinbasePubKey_);
   coinbaseFeed_ = std::make_shared<ResolverOneAddress>(coinbasePrivKey_, coinbasePubKey_);
   const unsigned SOME_AMOUNT = 1;
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, SOME_AMOUNT * COIN);
   auto fullUtxoScript = coinbaseRecipient.getSerializedScript();
   coinbaseUtxoScript_ = fullUtxoScript.getSliceCopy(9, fullUtxoScript.getSize() - 9);

   MineBlocks(101);

   // core wallets
      const bs::wallet::PasswordData pd{ passphrase_, { bs::wallet::EncryptionType::Password } };
   {
      const auto coreWallet = envPtr_->walletsMgr()->createWallet("root", "",
         bs::core::wallet::Seed(SecureBinaryData::fromString("genesis seed"), NetworkType::TestNet),
         envPtr_->armoryInstance()->homedir_, pd, true, false); // added inside
      {
         auto grp = coreWallet->createGroup(coreWallet->getXBTGroupType());

         const bs::core::WalletPasswordScoped lock(coreWallet, passphrase_);
         rootSignWallet_ = grp->createLeaf(AddressEntryType_P2WPKH, 1);
      }

   }

   for (size_t i = 0; i < usersCount_; ++i) {
      const auto coreWallet = envPtr_->walletsMgr()->createWallet("user"+std::to_string(i), "",
         bs::core::wallet::Seed(SecureBinaryData::fromString("seed for user"+std::to_string(i)), NetworkType::TestNet),
         envPtr_->armoryInstance()->homedir_, pd, true, false); // added inside
      {
         auto grp = coreWallet->createGroup(coreWallet->getXBTGroupType());

         const bs::core::WalletPasswordScoped lock(coreWallet, passphrase_);
         userSignWallets_.emplace_back(grp->createLeaf(AddressEntryType_P2WPKH, 1));
      }
   }

   auto inprocSigner = std::make_shared<InprocSigner>(envPtr_->walletsMgr()
      , envPtr_->logger(), this, "", NetworkType::TestNet);
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger(), envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();

   const auto getAddrFromLeaf = []
      (std::shared_ptr<bs::sync::hd::Leaf> leaf, bool ext, AddressEntryType aet = AddressEntryType_Default) -> bs::Address
   {
      auto promAddr = std::make_shared<std::promise<bs::Address>>();
      auto futAddr = promAddr->get_future();
      const auto &cbAddr = [promAddr](const bs::Address &addr) {
         promAddr->set_value(addr);
      };
      if (ext) {
         leaf->getNewExtAddress(cbAddr);
      } else {
         leaf->getNewIntAddress(cbAddr);
      }
      return futAddr.get();
   };

   // sync wallets
   {
      auto syncWallet = syncMgr_->getWalletById(rootSignWallet_->walletId());
      rootWallet_ = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);


      rootWallet_->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
      auto regIDs = rootWallet_->registerWallet(envPtr_->armoryConnection());
      UnitTestWalletACT::waitOnRefresh(regIDs);

      genesisAddr_ = getAddrFromLeaf(rootWallet_, true);
      revocationAddr_ = getAddrFromLeaf(rootWallet_, true);
   }
   for (auto w : userSignWallets_)
   {
      auto syncWallet = syncMgr_->getWalletById(w->walletId());
      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);

      syncLeaf->setCustomACT<UnitTestLocalACT>(envPtr_->armoryConnection());
      auto actPtr = dynamic_cast<UnitTestLocalACT*>(syncLeaf->peekACT());

      auto regIDs = syncLeaf->registerWallet(envPtr_->armoryConnection());
      actPtr->waitOnRefresh(regIDs);

      userWallets_.emplace_back(syncLeaf);

      userCCAddresses_.emplace_back(getAddrFromLeaf(syncLeaf, true, AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)));
      userFundAddresses_.emplace_back(getAddrFromLeaf(syncLeaf, true, AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)));
   }
}

void TestCCoinAsync::TearDown()
{
   coinbaseHashes_.clear();

   rootWallet_.reset();
   rootSignWallet_.reset();
   userWallets_.clear();
   userSignWallets_.clear();

   syncMgr_.reset();
//   envPtr_->walletsMgr()->deleteWalletFile(envPtr_->walletsMgr()->getPrimaryWallet());
   envPtr_.reset();
}

void TestCCoinAsync::MineBlocks(unsigned count, bool wait)
{
   auto curHeight = envPtr_->armoryConnection()->topBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());

   if(wait)
      envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
}

void TestCCoinAsync::setReorgBranchPoint(const BinaryData& hash)
{
   envPtr_->armoryInstance()->setReorgBranchPoint(hash);
}

BinaryData TestCCoinAsync::getCurrentTopBlockHash(void) const
{
   return envPtr_->armoryInstance()->getCurrentTopBlockHash();
}

void TestCCoinAsync::UpdateBalances(std::shared_ptr<bs::sync::hd::Leaf> wallet)
{
   //update balance
   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   const auto &cbBalance = [promPtr](void)
   {
      promPtr->set_value(true);
   };

   //async, has to wait
   wallet->updateBalances(cbBalance);
   fut.wait();
}

void TestCCoinAsync::UpdateAllBalances()
{
   UpdateBalances(rootWallet_);
   for (auto && wallet : userWallets_)
      UpdateBalances(wallet);
}

void TestCCoinAsync::waitOnZc(const Tx& tx)
{
   std::vector<bs::Address> addresses;
   for (unsigned i = 0; i < tx.getNumTxOut(); i++)
   {
      auto&& txOut = tx.getTxOutCopy(i);
      addresses.push_back(bs::Address::fromHash(txOut.getScrAddressStr()));
   }

   waitOnZc(tx.getThisHash(), addresses);
}

void TestCCoinAsync::waitOnZc(
   const BinaryData& hash, const std::vector<bs::Address>& addresses)
{
   std::set<bs::Address> addrSet;
   addrSet.insert(addresses.begin(), addresses.end());

   while (true)
   {
      auto&& zcVec = envPtr_->blockMonitor()->waitForZC();
      if (zcVec.begin()->txHash != hash)
         continue;

      std::set<bs::Address> zcAddrSet;
      for (auto& zcObj : zcVec)
         zcAddrSet.insert(zcObj.addresses.begin(), zcObj.addresses.end());

      if (addrSet == zcAddrSet)
         return;
   }
}

BinaryData TestCCoinAsync::FundFromCoinbase(
   const std::vector<bs::Address> & addresses, const uint64_t & valuePerOne)
{
   TCoinbaseHashes::const_iterator iter;
   if (coinbaseLast_ == std::numeric_limits<unsigned>::max()) {
      iter = coinbaseHashes_.begin();
   }
   else {
      iter = coinbaseHashes_.find(coinbaseLast_);
      ++iter;
   }

   uint64_t funded = 0;
   const uint64_t required = valuePerOne * addresses.size();
   Signer signer;
   while (funded < required && iter != coinbaseHashes_.end()) {
      UTXO utxo(50 * COIN, iter->first, 0, 0, iter->second, coinbaseUtxoScript_);
      auto spendPtr = std::make_shared<ScriptSpender>(utxo);

      signer.addSpender(spendPtr);
      coinbaseLast_ = iter->first;
      funded += utxo.getValue();
      ++iter;
   }
   if (funded < required)
      throw std::runtime_error("Not enough cb coins! Mine more blocks!");

   for (auto && addr : addresses) {
      signer.addRecipient(addr.getRecipient(bs::XBTAmount{ (int64_t)valuePerOne }));
   }
   signer.setFeed(coinbaseFeed_);

   //sign & send
   signer.sign();
   auto signedTx = signer.serializeSignedTx();
   Tx tx(signedTx);
   
   envPtr_->armoryInstance()->pushZC(signedTx);
   waitOnZc(tx.getThisHash(), addresses);
   return signer.getTxId();
}

BinaryData TestCCoinAsync::SimpleSendMany(const bs::Address & fromAddress, const std::vector<bs::Address> & toAddresses, const uint64_t & valuePerOne)
{
   auto promPtr = std::make_shared<std::promise<BinaryData>>();
   auto fut = promPtr->get_future();

   /// @todo:
   const uint64_t fee = 1000;

   auto signWallet = envPtr_->walletsMgr()->getWalletByAddress(fromAddress);
   auto lockWallet = envPtr_->walletsMgr()->getHDRootForLeaf(signWallet->walletId());
   auto const wallet = syncMgr_->getWalletByAddress(fromAddress);

   const auto &cbTxOutList =
      [this, lockWallet, signWallet, wallet, fromAddress, toAddresses, valuePerOne, promPtr, fee] (std::vector<UTXO> inputs) -> void
      {
         std::vector<std::shared_ptr<ArmorySigner::ScriptRecipient>> recipients;
         for(const auto & addr : toAddresses) {
            recipients.push_back(addr.getRecipient(bs::XBTAmount{ (int64_t)valuePerOne }));
         }

         const uint64_t requiredValue = valuePerOne * toAddresses.size();
         std::vector<UTXO> valInputs;
         uint64_t inputsValue(0);

         for (auto &&input : inputs) {
            if (input.getRecipientScrAddr() == fromAddress.prefixed()) {
               valInputs.push_back(input);
               inputsValue += input.getValue();
               if (inputsValue >= requiredValue + fee)
                  break;
            }
         }
         if (inputsValue < requiredValue + fee)
            throw std::runtime_error("Not enough money on the source address");

         auto txReq = wallet->createTXRequest(valInputs, recipients, true, fee, false, fromAddress);
         BinaryData txSigned;
         {
            const bs::core::WalletPasswordScoped lock(lockWallet, passphrase_);
            txSigned = signWallet->signTXRequest(txReq, true);
            if (txSigned.empty())
               throw std::runtime_error("Can't sign tx");
         }

         envPtr_->armoryInstance()->pushZC(txSigned);
         Tx txObj(txSigned);
         EXPECT_TRUE(txObj.isInitialized());
         auto addresses = toAddresses;
         addresses.push_back(fromAddress);
         waitOnZc(txObj.getThisHash(), addresses);
         promPtr->set_value(txObj.getThisHash());
      };

   wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
   return fut.get();
}

Tx TestCCoinAsync::CreateCJtx(
   const std::vector<UTXO> & ccSortedInputsUserA, 
   const std::vector<UTXO> & paymentSortedInputsUserB,
   const CCoinSpender& structA, const CCoinSpender& structB, 
   unsigned blockDelay)
{
   uint64_t fee = 1000;
   uint64_t ccValue = 0;
   uint64_t xbtValue = 0;

   auto const sellerSignWallet = envPtr_->walletsMgr()->getWalletByAddress(structA.ccAddr_);
   auto const buyerSignWallet = envPtr_->walletsMgr()->getWalletByAddress(structB.xbtAddr_);
   auto const sellerWallet = syncMgr_->getWalletByAddress(structA.ccAddr_);
   auto const buyerWallet = syncMgr_->getWalletByAddress(structB.xbtAddr_);

   Signer cjSigner;
   for (auto& utxo : ccSortedInputsUserA)
   {
      auto spender = std::make_shared<ScriptSpender>(utxo);
      cjSigner.addSpender(spender);
      ccValue += utxo.getValue();
   }
   
   for (auto& utxo : paymentSortedInputsUserB)
   {
      auto spender = std::make_shared<ScriptSpender>(utxo);
      cjSigner.addSpender(spender);
      xbtValue += utxo.getValue();
   }

   //CC recipients
   cjSigner.addRecipient(structB.ccAddr_.getRecipient(bs::XBTAmount{ (int64_t)structB.ccValue_ }));
   if(ccValue - structB.ccValue_ > 0)
      cjSigner.addRecipient(structA.ccAddr_.getRecipient(bs::XBTAmount{ (int64_t)(ccValue - structB.ccValue_) }));

   //XBT recipients
   cjSigner.addRecipient(structA.xbtAddr_.getRecipient(bs::XBTAmount{ (int64_t)structA.xbtValue_ }));

   if(xbtValue - structA.xbtValue_ - fee > 0)
      cjSigner.addRecipient(structB.xbtAddr_.getRecipient(bs::XBTAmount{ (int64_t)(xbtValue - structA.xbtValue_ - fee) }));

   {
      auto leaf = envPtr_->walletsMgr()->getHDRootForLeaf(sellerSignWallet->walletId());
      const bs::core::WalletPasswordScoped passScoped(leaf, passphrase_);
      const auto&& lock = sellerSignWallet->lockDecryptedContainer();
      cjSigner.setFeed(sellerSignWallet->getResolver());
      cjSigner.sign();
   }

   {
      auto leaf = envPtr_->walletsMgr()->getHDRootForLeaf(buyerSignWallet->walletId());
      const bs::core::WalletPasswordScoped passScoped(leaf, passphrase_);
      const auto&& lock = buyerSignWallet->lockDecryptedContainer();
      cjSigner.resetFeed();
      cjSigner.setFeed(buyerSignWallet->getResolver());
      cjSigner.sign();
   }

   EXPECT_TRUE(cjSigner.isSigned());
   EXPECT_TRUE(cjSigner.verify());
   auto signedTx = cjSigner.serializeSignedTx();
   EXPECT_FALSE(signedTx.empty());

   Tx tx(signedTx);
   EXPECT_TRUE(tx.isInitialized());

   std::vector<bs::Address> addresses;
   addresses.push_back(structA.ccAddr_);
   addresses.push_back(structA.xbtAddr_);
   addresses.push_back(structB.ccAddr_);
   addresses.push_back(structB.xbtAddr_);

   envPtr_->armoryInstance()->pushZC(signedTx, blockDelay);
   waitOnZc(tx.getThisHash(), addresses);

   return tx;
}

void TestCCoinAsync::InitialFund()
{
   const uint64_t fee = 1000;
   const uint64_t required = (100 * ccLotSize_) * usersCount_ + fee + COIN;

   // fund genesis address
   FundFromCoinbase( { genesisAddr_ }, required);

   // fund "common" user addresses
   FundFromCoinbase( {userFundAddresses_}, 50 * COIN);

   MineBlocks(6);
   UpdateBalances(rootWallet_);

   EXPECT_EQ(rootWallet_->getAddrBalance(genesisAddr_)[0], required);

   // fund "CC" user addresses from genesis
   SimpleSendMany(genesisAddr_, userCCAddresses_, 100 * ccLotSize_);

   MineBlocks(6);
   UpdateAllBalances();
}

std::vector<UTXO> TestCCoinAsync::GetUTXOsFor(const bs::Address & addr, bool sortedByValue)
{
   auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut = promPtr->get_future();

   const auto &cbTxOutList = [this, addr, sortedByValue, promPtr] (std::vector<UTXO> inputs) -> void
   {
      std::vector<UTXO> result;
      for (auto && input : inputs) {
         if (input.getRecipientScrAddr() == addr.prefixed())
            result.emplace_back(input);
      }
      if (sortedByValue)
         std::sort(result.begin(), result.end(), [] (UTXO const & l, UTXO const & r) { return l.getValue() > r.getValue(); });

      promPtr->set_value(result);
   };

   auto const wallet = syncMgr_->getWalletByAddress(addr);
   wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
   return fut.get();
}

void TestCCoinAsync::revoke(const bs::Address& addr)
{
   std::vector<bs::Address> addrVec;
   addrVec.push_back(addr);
   SimpleSendMany(revocationAddr_, addrVec, 1000);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(TestCCoinAsync, Initial_balances)
{
   InitialFund();

   EXPECT_EQ(rootWallet_->getAddrBalance(genesisAddr_)[0], COIN);

   for (size_t i = 0; i < usersCount_; i++) {
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userCCAddresses_[i])[0], 100 * ccLotSize_);
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userFundAddresses_[i])[0], 50 * COIN);
   }

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<ColoredCoinTrackerAsync>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({regPair.first}, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++) {
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);
   }
}

TEST_F(TestCCoinAsync, Case_1CC_2CC)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   std::vector<UTXO> utxosA = GetUTXOsFor(userCCAddresses_[0]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 1);
   EXPECT_EQ(utxosB.size(), 1);

   const uint amountCC = 50;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.xbtAddr_ = userFundAddresses_[0];
   ccsA.xbtValue_ = COIN;

   CCoinSpender ccsB;
   ccsB.ccAddr_ = userCCAddresses_[1];
   ccsB.xbtAddr_ = userFundAddresses_[1];
   ccsB.ccValue_ = amountCC * ccLotSize_;

   auto tx = CreateCJtx(utxosA, utxosB, ccsA, ccsB);
   EXPECT_EQ(tx.getNumTxIn(), 2);
   EXPECT_EQ(tx.getNumTxOut(), 4);
   MineBlocks(6);
   UpdateAllBalances();

   auto promUpdate = std::make_shared<std::promise<std::string>>();
   auto futUpdate = promUpdate->get_future();
   const auto &cbUpdate = [promUpdate, cct](const std::set<BinaryData> &addrs)
   {
      promUpdate->set_value(cct->registerAddresses(addrs, false));
   };
   cct->update(cbUpdate);
   const auto updRegId = futUpdate.get();
   UnitTestWalletACT::waitOnRefresh({ updRegId }, false);

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

TEST_F(TestCCoinAsync, Case_MultiUnorderedCC_2CC)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for (size_t i = 10; i < 100; i += 10)
   {
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] }, i * ccLotSize_);
      MineBlocks(6);
   }
   UpdateAllBalances();

   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   const auto lbdUpdate = [cct] {
      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto cb = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cb);
      auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);
   };
   lbdUpdate();

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetUTXOsFor(userCCAddresses_[0]);
   std::swap(utxosA[0], utxosA[9]);
   std::swap(utxosA[2], utxosA[7]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 10);
   EXPECT_EQ(utxosB.size(), 1);

   const uint amountCC = 545;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.xbtAddr_ = userFundAddresses_[0];
   ccsA.xbtValue_ = COIN;

   CCoinSpender ccsB;
   ccsB.ccAddr_ = userCCAddresses_[1];
   ccsB.xbtAddr_ = userFundAddresses_[1];
   ccsB.ccValue_ = amountCC * ccLotSize_;

   auto tx = CreateCJtx(utxosA, utxosB, ccsA, ccsB);
   EXPECT_EQ(tx.getNumTxIn(), 11);
   EXPECT_EQ(tx.getNumTxOut(), 4);
   MineBlocks(6);
   UpdateAllBalances();

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 5 * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   lbdUpdate();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 5 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

TEST_F(TestCCoinAsync, Revoke)
{
   InitialFund();
   FundFromCoinbase({ revocationAddr_ }, 50 * COIN);
   MineBlocks(6);

   EXPECT_EQ(rootWallet_->getAddrBalance(genesisAddr_)[0], COIN);

   for (size_t i = 0; i < usersCount_; ++i) {
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userCCAddresses_[i])[0], 100 * ccLotSize_);
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userFundAddresses_[i])[0], 50 * COIN);
   }

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);
   cct->addRevocationAddress(revocationAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send cc from addr9 to addr0
   std::vector<UTXO> utxosA = GetUTXOsFor(userCCAddresses_[9]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[0]);

   const uint amountCC = 50;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[9];
   ccsA.xbtAddr_ = userFundAddresses_[9];
   ccsA.xbtValue_ = COIN;

   CCoinSpender ccsB;
   ccsB.ccAddr_ = userCCAddresses_[0];
   ccsB.xbtAddr_ = userFundAddresses_[0];
   ccsB.ccValue_ = amountCC * ccLotSize_;

   auto tx = CreateCJtx(utxosA, utxosB, ccsA, ccsB);

   //confirm the tx
   MineBlocks(1);

   const auto lbdUpdate = [cct] {
      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto cb = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cb);
      auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);
   };
   lbdUpdate();

   //check cc balances
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 50 * ccLotSize_);

   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //revoke addr9
   revoke(userCCAddresses_[9]);

   //confirm the tx
   MineBlocks(1);

   lbdUpdate();

   //check address has no more cc
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send more coins to addr9
   SimpleSendMany(genesisAddr_, { userCCAddresses_[9] }, 150 * ccLotSize_);
   MineBlocks(6);

   lbdUpdate();

   //check it still has no cc value
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send cc from addr8 to addr1
   std::vector<UTXO> utxosC = GetUTXOsFor(userCCAddresses_[8]);
   std::vector<UTXO> utxosD = GetUTXOsFor(userFundAddresses_[1]);

   const uint amountCC2 = 60;
   CCoinSpender ccsC;
   ccsC.ccAddr_ = userCCAddresses_[8];
   ccsC.xbtAddr_ = userFundAddresses_[8];
   ccsC.xbtValue_ = COIN;

   CCoinSpender ccsD;
   ccsD.ccAddr_ = userCCAddresses_[1];
   ccsD.xbtAddr_ = userFundAddresses_[1];
   ccsD.ccValue_ = amountCC2 * ccLotSize_;

   auto tx2 = CreateCJtx(utxosC, utxosD, ccsC, ccsD);

   //revoke addr8 within the same block
   revoke(userCCAddresses_[8]);

   //confirm the transactions
   MineBlocks(1);

   lbdUpdate();

   //check balances
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 160 * ccLotSize_);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 2; i < usersCount_ - 2; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //bootstrap fresh cct, check balances are valid (retroaction check)
   auto cct2 = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct2->addOriginAddress(genesisAddr_);
   cct2->addRevocationAddress(revocationAddr_);

   auto promOnline2 = std::make_shared<std::promise<bool>>();
   auto futOnline2 = promOnline2->get_future();
   const auto regPair2 = cct2->goOnline([promOnline2](bool result) {
      promOnline2->set_value(result);
   });
   act->addRefreshCb(regPair2.first, regPair2.second);
   UnitTestWalletACT::waitOnRefresh({ regPair2.first }, false);
   EXPECT_TRUE(futOnline2.get());

   //check balances
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[1].prefixed()), 160 * ccLotSize_);

   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[8].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 2; i < usersCount_ - 2; i++)
      EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);
}

TEST_F(TestCCoinAsync, Case_MultiUnorderedCC_NoChange)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for (size_t i = 10; i < 100; i += 10) { // 10..90.. +100init = 550 total
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] }, i * ccLotSize_);
      MineBlocks(6);
   }
   UpdateAllBalances();

   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   const auto lbdUpdate = [cct] {
      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto cb = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cb);
      auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);
   };
   lbdUpdate();

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetUTXOsFor(userCCAddresses_[0]);
   std::swap(utxosA[0], utxosA[9]);
   std::swap(utxosA[2], utxosA[7]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 10);
   EXPECT_EQ(utxosB.size(), 1);

   const uint amountCC = 550; // w/o change
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.xbtAddr_ = userFundAddresses_[0];
   ccsA.xbtValue_ = COIN;

   CCoinSpender ccsB;
   ccsB.ccAddr_ = userCCAddresses_[1];
   ccsB.xbtAddr_ = userFundAddresses_[1];
   ccsB.ccValue_ = amountCC * ccLotSize_;

   auto tx = CreateCJtx(utxosA, utxosB, ccsA, ccsB);
   EXPECT_EQ(tx.getNumTxIn(), 11);
   EXPECT_EQ(tx.getNumTxOut(), 3);
   MineBlocks(6);
   UpdateAllBalances();

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   lbdUpdate();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

TEST_F(TestCCoinAsync, ZeroConf)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for (size_t i = 10; i < 100; i += 10) { // 10..90.. +100init = 550 total
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] }, i * ccLotSize_);

      size_t i_dec = i / 10;
      uint64_t ccbal = (i_dec * (i_dec + 1)) * 5 + 100;

      auto promZcUpdate = std::make_shared<std::promise<std::string>>();
      auto futZcUpdate = promZcUpdate->get_future();
      const auto &cbZcUpdate = [promZcUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promZcUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->zcUpdate(cbZcUpdate);
      const auto zcUpdRegId = futZcUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ zcUpdRegId }, false);

      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), ccbal * ccLotSize_);

      MineBlocks(6);

      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto &cbUpdate = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cbUpdate);
      const auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);

      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), ccbal * ccLotSize_);
   }

   UpdateAllBalances();
   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetUTXOsFor(userCCAddresses_[0]);
   std::swap(utxosA[0], utxosA[9]);
   std::swap(utxosA[2], utxosA[7]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 10);
   EXPECT_EQ(utxosB.size(), 1);

   const uint amountCC = 550; // w/o change
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.xbtAddr_ = userFundAddresses_[0];
   ccsA.xbtValue_ = COIN;

   CCoinSpender ccsB;
   ccsB.ccAddr_ = userCCAddresses_[1];
   ccsB.xbtAddr_ = userFundAddresses_[1];
   ccsB.ccValue_ = amountCC * ccLotSize_;

   auto tx = CreateCJtx(utxosA, utxosB, ccsA, ccsB);
   EXPECT_EQ(tx.getNumTxIn(), 11);
   EXPECT_EQ(tx.getNumTxOut(), 3);

   auto promZcUpdate = std::make_shared<std::promise<std::string>>();
   auto futZcUpdate = promZcUpdate->get_future();
   const auto &cbZcUpdate = [promZcUpdate, cct](const std::set<BinaryData> &addrs)
   {
      promZcUpdate->set_value(cct->registerAddresses(addrs, false));
   };
   cct->zcUpdate(cbZcUpdate);
   const auto zcUpdRegId = futZcUpdate.get();
   UnitTestWalletACT::waitOnRefresh({ zcUpdRegId }, false);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);

   MineBlocks(6);
   UpdateAllBalances();

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   auto promUpdate = std::make_shared<std::promise<std::string>>();
   auto futUpdate = promUpdate->get_future();
   const auto &cbUpdate = [promUpdate, cct](const std::set<BinaryData> &addrs)
   {
      promUpdate->set_value(cct->registerAddresses(addrs, false));
   };
   cct->update(cbUpdate);
   const auto updRegId = futUpdate.get();
   UnitTestWalletACT::waitOnRefresh({ updRegId }, false);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

TEST_F(TestCCoinAsync, ZeroConfChain)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   auto createTxLbd = [](uint64_t value, 
      std::shared_ptr<ScriptSpender> spender, 
      bs::Address& addr, 
      std::shared_ptr<ResolverFeed> feedPtr)->Tx
   {
      Signer signer;
      signer.addSpender(spender);
      signer.addRecipient(addr.getRecipient(bs::XBTAmount{ (int64_t)value }));

      auto script = spender->getOutputScript();
      auto changeAddr = BtcUtils::getScrAddrForScript(script);
      auto changeRec = std::make_shared<Recipient_P2WPKH>(
         changeAddr.getSliceCopy(1, 20), spender->getValue() - value);
      signer.addRecipient(changeRec);

      signer.setFeed(feedPtr);
      signer.sign();
      Tx tx(signer.serializeSignedTx());
      return tx;
   };

   UTXO utxo;
   {
      auto const wallet = syncMgr_->getWalletByAddress(genesisAddr_);

      auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
      auto fut = promPtr->get_future();
      const auto &cbTxOutList = [promPtr](std::vector<UTXO> inputs)->void
      {
         promPtr->set_value(inputs);
      };

      wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
      auto utxos = fut.get();

      EXPECT_EQ(utxos.size(), 1);
      utxo = utxos[0];
   }

   auto const signWallet = envPtr_->walletsMgr()->getWalletByAddress(genesisAddr_);
   auto const lockWallet = envPtr_->walletsMgr()->getHDRootForLeaf(signWallet->walletId());
   for (size_t i = 0; i < 10; i++)
   {
      //create CC tx
      auto y = (i + 1) * 10;
      Tx zc;
      {
         auto spender = std::make_shared<ScriptSpender>(utxo);
         const bs::core::WalletPasswordScoped passScoped(lockWallet, passphrase_);
         const auto&& lock = signWallet->lockDecryptedContainer();
         zc = createTxLbd(y*ccLotSize_, spender, userCCAddresses_[i], signWallet->getResolver());
      }

      //set block delay and broadcast 
      EXPECT_TRUE(zc.isInitialized());
      envPtr_->armoryInstance()->pushZC(zc.serialize(), i);

      //wait on zc notification
      std::vector<bs::Address> addresses = {
         bs::Address::fromHash(utxo.getRecipientScrAddr()),
         userCCAddresses_[i]
      };
      waitOnZc(zc.getThisHash(), addresses);

      //check balances
      uint64_t ccbal = y + 100;

      auto promZcUpdate = std::make_shared<std::promise<std::string>>();
      auto futZcUpdate = promZcUpdate->get_future();
      const auto &cbZcUpdate = [promZcUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promZcUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->zcUpdate(cbZcUpdate);
      const auto zcUpdRegId = futZcUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ zcUpdRegId }, false);

      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), ccbal * ccLotSize_);

      //update utxo with genesis address change for next zc in the chain
      auto txOut = zc.getTxOutCopy(1);
      UTXO zcUtxo;
      zcUtxo.unserializeRaw(txOut.serialize());
      zcUtxo.txHash_ = zc.getThisHash();
      zcUtxo.txOutIndex_ = 1;

      utxo = zcUtxo;
   }

   for (unsigned y = 0; y < 10; y++)
   {
      uint64_t ccbal = (y + 1) * 10 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[y].prefixed()), ccbal * ccLotSize_);
   }

   //mine 10 blocks one at a time and check balances
   for (unsigned i = 0; i < 10; i++)
   {
      MineBlocks(1);

      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto &cbUpdate = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cbUpdate);
      const auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);

      for (unsigned y = 0; y < 10; y++)
      {
         uint64_t ccbal = (y + 1) * 10 + 100;
         EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[y].prefixed()), ccbal * ccLotSize_);
      }
   }
}

TEST_F(TestCCoinAsync, Reorg)
{
   InitialFund();

   auto act = make_unique<AsyncCCT_ACT>(envPtr_->armoryConnection().get());
   auto cct = std::make_shared<AsyncCCT>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);

   auto promOnline = std::make_shared<std::promise<bool>>();
   auto futOnline = promOnline->get_future();
   const auto regPair = cct->goOnline([promOnline](bool result) {
      promOnline->set_value(result);
   });
   act->addRefreshCb(regPair.first, regPair.second);
   UnitTestWalletACT::waitOnRefresh({ regPair.first }, false);
   EXPECT_TRUE(futOnline.get());

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   auto createTxLbd = [](std::shared_ptr<ScriptSpender> spender,
      const std::map<bs::Address, uint64_t>& recipients,
      std::shared_ptr<ResolverFeed> feedPtr)->Tx
   {
      Signer signer;
      signer.addSpender(spender);

      uint64_t total = 0;
      for (auto& recipient : recipients)
      {
         total += recipient.second;
         signer.addRecipient(recipient.first.getRecipient(bs::XBTAmount{ (int64_t)recipient.second }));
      }

      if (total > spender->getValue())
         throw std::runtime_error("spending more than the outpoint value");

      auto script = spender->getOutputScript();
      auto changeAddr = BtcUtils::getScrAddrForScript(script);
      auto changeRec = std::make_shared<Recipient_P2WPKH>(
         changeAddr.getSliceCopy(1, 20), spender->getValue() - total);
      signer.addRecipient(changeRec);

      signer.setFeed(feedPtr);
      signer.sign();
      Tx tx(signer.serializeSignedTx());
      return tx;
   };

   UTXO utxoMain;
   {
      auto const wallet = syncMgr_->getWalletByAddress(genesisAddr_);

      auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
      auto fut = promPtr->get_future();
      const auto &cbTxOutList = [promPtr](std::vector<UTXO> inputs)->void
      {
         promPtr->set_value(inputs);
      };

      wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX, true);
      auto utxos = fut.get();

      EXPECT_EQ(utxos.size(), 1);
      utxoMain = utxos[0];
   }

   auto signWalletMain = envPtr_->walletsMgr()->getWalletByAddress(genesisAddr_);
   auto lockWalletMain = envPtr_->walletsMgr()->getHDRootForLeaf(signWalletMain->walletId());

   auto signWalletA = envPtr_->walletsMgr()->getWalletByAddress(userCCAddresses_[0]);
   auto lockWalletA = envPtr_->walletsMgr()->getHDRootForLeaf(signWalletA->walletId());

   auto signWalletB = envPtr_->walletsMgr()->getWalletByAddress(userCCAddresses_[1]);
   auto lockWalletB = envPtr_->walletsMgr()->getHDRootForLeaf(signWalletB->walletId());

   UTXO utxoA, utxoB;
   {
      //zc for the branch point
      auto spender = std::make_shared<ScriptSpender>(utxoMain);
      const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);

      std::vector<bs::Address> addresses;
      addresses.push_back(bs::Address::fromHash(utxoMain.getRecipientScrAddr()));

      auto lock = signWalletMain->lockDecryptedContainer();

      std::map<bs::Address, uint64_t> recipients;
      recipients.insert(std::make_pair(userCCAddresses_[0], 100 * ccLotSize_));
      recipients.insert(std::make_pair(userCCAddresses_[1], 100 * ccLotSize_));
      auto&& zc = createTxLbd(spender, recipients, signWalletMain->getResolver());

      for (auto& recipient : recipients)
         addresses.push_back(recipient.first);

      {
         auto txOut = zc.getTxOutCopy(0);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 0;

         utxoA = zcUtxo;
      }

      {
         auto txOut = zc.getTxOutCopy(1);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 1;

         utxoB = zcUtxo;
      }

      {
         auto txOut = zc.getTxOutCopy(2);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 2;

         utxoMain = zcUtxo;
      }

      //push it
      envPtr_->armoryInstance()->pushZC(zc.serialize(), 0);

      //wait on zc notification
      waitOnZc(zc.getThisHash(), addresses);

      //mine new block
      MineBlocks(1);
   }

   auto&& branchPointHash = getCurrentTopBlockHash();
   std::vector<BinaryData> vecMain, vecA, vecB;

   for (size_t i = 2; i < 6; i++)
   {
      //common zc
      auto y = (i + 1);
      {
         auto spender = std::make_shared<ScriptSpender>(utxoMain);
         const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);
         auto lock = signWalletMain->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[i], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients, signWalletMain->getResolver());
         vecMain.push_back(zc.serialize());

         auto txOut = zc.getTxOutCopy(1);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 1;

         utxoMain = zcUtxo;
      }

      auto c = i + 4;

      //branch A
      y = (c + 1);
      {
         auto spender = std::make_shared<ScriptSpender>(utxoA);
         const bs::core::WalletPasswordScoped passScoped(lockWalletA, passphrase_);
         auto lock = signWalletA->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients, signWalletA->getResolver());
         vecA.push_back(zc.serialize());

         auto txOut = zc.getTxOutCopy(1);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 1;

         utxoA = zcUtxo;
      }

      //branch B
      y *= 2;
      {
         auto spender = std::make_shared<ScriptSpender>(utxoB);
         const bs::core::WalletPasswordScoped passScoped(lockWalletB, passphrase_);
         auto lock = signWalletB->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients, signWalletB->getResolver());
         vecB.push_back(zc.serialize());

         auto txOut = zc.getTxOutCopy(1);
         UTXO zcUtxo;
         zcUtxo.unserializeRaw(txOut.serialize());
         zcUtxo.txHash_ = zc.getThisHash();
         zcUtxo.txOutIndex_ = 1;

         utxoB = zcUtxo;
      }
   }

   //push common & branch A tx
   for (unsigned i = 0; i < vecMain.size(); i++)
   {
      {
         Tx zc;
         zc.unserialize(vecMain[i]);
         envPtr_->armoryInstance()->pushZC(vecMain[i], i);
         waitOnZc(zc);
      }

      {
         Tx zc;
         zc.unserialize(vecA[i]);
         envPtr_->armoryInstance()->pushZC(vecA[i], i);
         waitOnZc(zc);
      }
   }

   //mine 3 blocks (leave 1 zc for each group)
   MineBlocks(3);

   const auto lbdUpdate = [cct] {
      auto promUpdate = std::make_shared<std::promise<std::string>>();
      auto futUpdate = promUpdate->get_future();
      const auto cb = [promUpdate, cct](const std::set<BinaryData> &addrs)
      {
         promUpdate->set_value(cct->registerAddresses(addrs, false));
      };
      cct->update(cb);
      auto updRegId = futUpdate.get();
      UnitTestWalletACT::waitOnRefresh({ updRegId }, false);
   };

   const auto lbdUpdateZc = [cct] {
      auto promUpdateZc = std::make_shared<std::promise<std::string>>();
      auto futUpdateZc = promUpdateZc->get_future();
      const auto cb = [promUpdateZc, cct](const std::set<BinaryData> &addrs)
      {
         promUpdateZc->set_value(cct->registerAddresses(addrs, false));
      };
      cct->zcUpdate(cb);
      auto zcUpdRegId = futUpdateZc.get();
      UnitTestWalletACT::waitOnRefresh({ zcUpdRegId }, false);
   };

   lbdUpdate();
   lbdUpdateZc();

   auto&& branchATop = getCurrentTopBlockHash();

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = c + 1 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
   }

   //reorg
   setReorgBranchPoint(branchPointHash);

   //push common & branch B tx
   for (unsigned i = 0; i < vecMain.size(); i++)
   {
      {
         Tx zc;
         zc.unserialize(vecMain[i]);
         envPtr_->armoryInstance()->pushZC(vecMain[i], i + 1, true);

         //dont wait on staged zc
      }

      {
         Tx zc;
         zc.unserialize(vecB[i]);
         envPtr_->armoryInstance()->pushZC(vecB[i], i + 1, true);

         //dont wait on staged zc
      }
   }

   //mine 4 blocks (leave 1 zc for each group, pass branch A by 1 block)
   MineBlocks(4);
   cct->reorg();

   lbdUpdate();
   lbdUpdateZc();

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = (c + 1) * 2 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
   }

   //reorg back to main branch
   setReorgBranchPoint(branchATop);
   envPtr_->armoryInstance()->pushZC(*vecA.rbegin(), 0, true);

   //mine 2 more blocks to go back on branch A
   MineBlocks(2);
   cct->reorg();

   lbdUpdate();
   lbdUpdateZc();

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = c + 1 + 100;

      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
   }
}

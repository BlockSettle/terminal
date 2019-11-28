#include "TestCCoin.h"
#include <QDebug>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "CheckRecipSigner.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

TestCCoin::TestCCoin()
{}

bs::Address LeafGetAddress(std::shared_ptr<bs::sync::hd::Leaf> leaf, bool ext, AddressEntryType aet = AddressEntryType_Default)
{
   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };
   if (ext) {
      leaf->getNewExtAddress(cbAddr);
   }
   else {
      leaf->getNewIntAddress(cbAddr);
   }
   return futAddr.get();
}

void TestCCoin::SetUp()
{
   // critical! clear events queue between tests run!
   UnitTestWalletACT::clear();

   envPtr_ = std::make_shared<TestEnv>(StaticLogger::loggerPtr);
   envPtr_->requireAssets();

   passphrase_ = SecureBinaryData("pass");

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
         bs::core::wallet::Seed(SecureBinaryData("genesis seed"), NetworkType::TestNet),
         envPtr_->armoryInstance()->homedir_, pd, true); // added inside
      {
         auto grp = coreWallet->createGroup(coreWallet->getXBTGroupType());

         const bs::core::WalletPasswordScoped lock(coreWallet, passphrase_);
         rootSignWallet_ = grp->createLeaf(AddressEntryType_P2WPKH, 1);
      }

   }

   for (size_t i = 0; i < usersCount_; ++i) {
      const auto coreWallet = envPtr_->walletsMgr()->createWallet("user"+std::to_string(i), "",
         bs::core::wallet::Seed(SecureBinaryData("seed for user"+std::to_string(i)), NetworkType::TestNet),
         envPtr_->armoryInstance()->homedir_, pd, true); // added inside
      {
         auto grp = coreWallet->createGroup(coreWallet->getXBTGroupType());

         const bs::core::WalletPasswordScoped lock(coreWallet, passphrase_);
         userSignWallets_.emplace_back(grp->createLeaf(AddressEntryType_P2WPKH, 1));
      }
   }

   auto inprocSigner = std::make_shared<InprocSigner>(envPtr_->walletsMgr(), envPtr_->logger(), "", NetworkType::TestNet);
   inprocSigner->Start();
   syncMgr_ = std::make_shared<bs::sync::WalletsManager>(envPtr_->logger(), envPtr_->appSettings(), envPtr_->armoryConnection());
   syncMgr_->setSignContainer(inprocSigner);
   syncMgr_->syncWallets();

   // sync wallets
   {
      auto syncWallet = syncMgr_->getWalletById(rootSignWallet_->walletId());
      rootWallet_ = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);


      rootWallet_->setCustomACT<UnitTestWalletACT>(envPtr_->armoryConnection());
      auto regIDs = rootWallet_->registerWallet(envPtr_->armoryConnection());
      UnitTestWalletACT::waitOnRefresh(regIDs);

      genesisAddr_ = LeafGetAddress(rootWallet_, true);
      revocationAddr_ = LeafGetAddress(rootWallet_, true);
   }
   for (auto w : userSignWallets_)
   {
      auto syncWallet = syncMgr_->getWalletById(w->walletId());
      auto syncLeaf = std::dynamic_pointer_cast<bs::sync::hd::Leaf>(syncWallet);

      syncLeaf->setCustomACT<UnitTestLocalACT>(envPtr_->armoryConnection());
      auto actPtr = dynamic_cast<UnitTestLocalACT*>(syncLeaf->peekACT());

      auto regIDs = syncLeaf->registerWallet(envPtr_->armoryConnection());
      actPtr->waitOnRefresh(regIDs);
      localACTs_.push_back(actPtr);

      userWallets_.emplace_back(syncLeaf);

      userCCAddresses_.emplace_back(LeafGetAddress(syncLeaf, true, AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)));
      userFundAddresses_.emplace_back(LeafGetAddress(syncLeaf, true, AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)));
   }
}

void TestCCoin::TearDown()
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

void TestCCoin::MineBlocks(unsigned count, bool wait)
{
   auto curHeight = envPtr_->armoryConnection()->topBlock();
   Recipient_P2PKH coinbaseRecipient(coinbaseScrAddr_, 50 * COIN);
   auto&& cbMap = envPtr_->armoryInstance()->mineNewBlock(&coinbaseRecipient, count);
   coinbaseHashes_.insert(cbMap.begin(), cbMap.end());

   if(wait)
      envPtr_->blockMonitor()->waitForNewBlocks(curHeight + count);
}

void TestCCoin::setReorgBranchPoint(const BinaryData& hash)
{
   envPtr_->armoryInstance()->setReorgBranchPoint(hash);
}

BinaryData TestCCoin::getCurrentTopBlockHash(void) const
{
   return envPtr_->armoryInstance()->getCurrentTopBlockHash();
}

void TestCCoin::UpdateBalances(std::shared_ptr<bs::sync::hd::Leaf> wallet)
{
   //update balance
   auto promPtr = std::make_shared<std::promise<bool>>();
   auto fut = promPtr->get_future();
   const auto &cbBalance = [promPtr](void)
   {
      promPtr->set_value(true);
   };

   //async, has to wait
   EXPECT_TRUE(wallet->updateBalances(cbBalance));
   fut.wait();
}

void TestCCoin::UpdateAllBalances()
{
   UpdateBalances(rootWallet_);
   for (auto && wallet : userWallets_)
      UpdateBalances(wallet);
}

void TestCCoin::waitOnZc(const Tx& tx)
{
   std::vector<bs::Address> addresses;
   for (unsigned i = 0; i < tx.getNumTxOut(); i++)
   {
      auto&& txOut = tx.getTxOutCopy(i);
      addresses.push_back(bs::Address::fromHash(txOut.getScrAddressStr()));
   }

   waitOnZc(tx.getThisHash(), addresses);
}

bool TestCCoin::waitOnZc(
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

      return (addrSet == zcAddrSet);
   }
}

BinaryData TestCCoin::FundFromCoinbase(
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
      signer.addRecipient(addr.getRecipient(bs::XBTAmount{ valuePerOne }));
   }
   signer.setFeed(coinbaseFeed_);

   //sign & send
   signer.sign();
   auto signedTx = signer.serialize();
   Tx tx(signedTx);
   
   envPtr_->armoryInstance()->pushZC(signedTx);
   waitOnZc(tx.getThisHash(), addresses);
   return signer.getTxId();
}

BinaryData TestCCoin::SimpleSendMany(const bs::Address & fromAddress, const std::vector<bs::Address> & toAddresses, const uint64_t & valuePerOne)
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
         std::vector<std::shared_ptr<ScriptRecipient>> recipients;
         for(const auto & addr : toAddresses) {
            recipients.push_back(addr.getRecipient(bs::XBTAmount{ valuePerOne }));
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

         const auto txReq = wallet->createTXRequest(valInputs, recipients, fee, false, fromAddress);
         BinaryData txSigned;
         {
            const bs::core::WalletPasswordScoped lock(lockWallet, passphrase_);
            txSigned = signWallet->signTXRequest(txReq, true);
            if (txSigned.isNull())
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

   wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
   return fut.get();
}

Tx TestCCoin::CreateCJtx(
   const std::vector<UTXO> & ccSortedInputsUserA,
   const std::vector<UTXO> & paymentSortedInputsUserB,
   const CCoinSpender& structA, const CCoinSpender& structB,
   const std::vector<UTXO> & ccInputsAppend,
   unsigned blockDelay)
{
   uint64_t fee = 1000;
   uint64_t ccValue = 0;
   uint64_t xbtValue = 0;

   std::set<bs::core::WalletsManager::WalletPtr> signWallets;
   auto const sellerWallet = syncMgr_->getWalletByAddress(structA.ccAddr_);
   auto const buyerWallet = syncMgr_->getWalletByAddress(structB.xbtAddr_);

   Signer cjSigner;
   for (auto& utxo : ccSortedInputsUserA) {
      auto spender = std::make_shared<ScriptSpender>(utxo);
      cjSigner.addSpender(spender);
      ccValue += utxo.getValue();
      const auto addr = bs::Address::fromUTXO(utxo);
      const auto signWallet = envPtr_->walletsMgr()->getWalletByAddress(addr);
      EXPECT_NE(signWallet, nullptr);
      signWallets.insert(signWallet);
   }

   for (auto& utxo : paymentSortedInputsUserB)
   {
      auto spender = std::make_shared<ScriptSpender>(utxo);
      cjSigner.addSpender(spender);
      xbtValue += utxo.getValue();
      const auto addr = bs::Address::fromUTXO(utxo);
      const auto signWallet = envPtr_->walletsMgr()->getWalletByAddress(addr);
      EXPECT_NE(signWallet, nullptr);
      signWallets.insert(signWallet);
   }

   for (auto& utxo : ccInputsAppend) {
      auto spender = std::make_shared<ScriptSpender>(utxo);
      cjSigner.addSpender(spender);
      ccValue += utxo.getValue();
      const auto addr = bs::Address::fromUTXO(utxo);
      const auto signWallet = envPtr_->walletsMgr()->getWalletByAddress(addr);
      EXPECT_NE(signWallet, nullptr);
      signWallets.insert(signWallet);
   }

   //CC recipients
   cjSigner.addRecipient(structB.ccAddr_.getRecipient(bs::XBTAmount{ structB.ccValue_ }));
   const bs::Address ccChange = structA.ccChange.isNull() ? structA.ccAddr_ : structA.ccChange;
   
   if (ccValue < structB.ccValue_)
      throw std::runtime_error("invalid spend amount");
   if (ccValue > structB.ccValue_)
   {
      auto changeVal = ccValue - structB.ccValue_;
      if (changeVal % ccLotSize_ != 0)
      {
         auto factor = changeVal / ccLotSize_;
         changeVal = ccLotSize_ * factor;
      }
      cjSigner.addRecipient(ccChange.getRecipient(bs::XBTAmount{ changeVal }));
   }

   //XBT recipients
   cjSigner.addRecipient(structA.xbtAddr_.getRecipient(bs::XBTAmount{ structA.xbtValue_ }));

   if(xbtValue - structA.xbtValue_ - fee > 0)
      cjSigner.addRecipient(structB.xbtAddr_.getRecipient(bs::XBTAmount{ xbtValue - structA.xbtValue_ - fee }));

   for (const auto &wallet : signWallets) {
      auto hdRoot = envPtr_->walletsMgr()->getHDRootForLeaf(wallet->walletId());
      const bs::core::WalletPasswordScoped passScoped(hdRoot, passphrase_);
      const auto&& lock = wallet->lockDecryptedContainer();
      cjSigner.resetFeeds();
      cjSigner.setFeed(wallet->getResolver());
      cjSigner.sign();
   }

   EXPECT_TRUE(cjSigner.isValid());
   EXPECT_TRUE(cjSigner.verify());
   auto signedTx = cjSigner.serialize();
   EXPECT_FALSE(signedTx.isNull());

   Tx tx(signedTx);
   EXPECT_TRUE(tx.isInitialized());

   std::vector<bs::Address> addresses;
   addresses.push_back(structA.ccAddr_);
   addresses.push_back(structA.xbtAddr_);
   addresses.push_back(structB.ccAddr_);
   addresses.push_back(structB.xbtAddr_);
   if (!structA.ccChange.isNull()) {
      addresses.push_back(structA.ccChange);
   }
   if (!structB.ccChange.isNull()) {
      addresses.push_back(structB.ccChange);
   }

   for (auto& utxo : ccInputsAppend) 
   {
      auto addy = bs::Address::fromUTXO(utxo);
      addresses.push_back(addy);
   }

   envPtr_->armoryInstance()->pushZC(signedTx, blockDelay);
   EXPECT_TRUE(waitOnZc(tx.getThisHash(), addresses));

   return tx;
}

void TestCCoin::InitialFund(const std::vector<bs::Address> &recipients)
{
   const uint64_t fee = 1000;
   const uint64_t required = (100 * ccLotSize_) * usersCount_ + fee + COIN;

   // fund genesis address
   FundFromCoinbase( { genesisAddr_ }, required);

   // fund "common" user addresses
   FundFromCoinbase(userFundAddresses_, 50 * COIN);

   ASSERT_NE(rootWallet_, nullptr);

   MineBlocks(6);
   UpdateBalances(rootWallet_);

   const auto &addrBalance = rootWallet_->getAddrBalance(genesisAddr_);
   ASSERT_FALSE(addrBalance.empty());
   EXPECT_EQ(addrBalance[0], required);

   const auto &ccAddresses = recipients.empty() ? userCCAddresses_ : recipients;
   // fund "CC" user addresses from genesis
   SimpleSendMany(genesisAddr_, ccAddresses, 100 * ccLotSize_);

   MineBlocks(6);
   UpdateAllBalances();
}

std::vector<UTXO> TestCCoin::GetUTXOsFor(const bs::Address & addr, bool sortedByValue)
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
   wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
   return fut.get();
}

std::vector<UTXO> TestCCoin::GetCCUTXOsFor(std::shared_ptr<ColoredCoinTracker> ccPtr,
   const bs::Address & addr, bool sortedByValue)
{
   auto promPtr = std::make_shared<std::promise<std::vector<UTXO>>>();
   auto fut = promPtr->get_future();
   auto utxoLbd = [promPtr](std::vector<UTXO> utxoVec, std::exception_ptr)
   {
      promPtr->set_value(utxoVec);
   };

   std::set<BinaryData> addrSet = { addr.prefixed() };
   ccPtr->getCCUtxoForAddresses(addrSet, false, utxoLbd);
   auto&& result = fut.get();

   if (sortedByValue)
      std::sort(result.begin(), result.end(), [](UTXO const & l, UTXO const & r) { return l.getValue() > r.getValue(); });
   return result;
}

void TestCCoin::revoke(const bs::Address& addr)
{
   std::vector<bs::Address> addrVec;
   addrVec.push_back(addr);
   SimpleSendMany(revocationAddr_, addrVec, 1000);
}

////
std::shared_ptr<ColoredCoinTracker> TestCCoin::makeCct(void)
{
   auto cct = std::make_shared<ColoredCoinTracker>(ccLotSize_, envPtr_->armoryConnection());
   cct->addOriginAddress(genesisAddr_);
   
   auto cctUt = (ColoredCoinTracker_UT*)cct.get();
   cctUt->setACT(std::make_shared<ColoredCoinTestACT>(envPtr_->armoryConnection().get()));

   return cct;
}

void TestCCoin::update(std::shared_ptr<ColoredCoinTracker> cct)
{
   auto cctUt = (ColoredCoinTracker_UT*)cct.get();
   cctUt->update_UT();
}

void TestCCoin::zcUpdate(std::shared_ptr<ColoredCoinTracker> cct)
{
   auto cctUt = (ColoredCoinTracker_UT*)cct.get();
   cctUt->zcUpdate_UT();
}

void TestCCoin::reorg(std::shared_ptr<ColoredCoinTracker> cct)
{
   auto cctUt = (ColoredCoinTracker_UT*)cct.get();
   cctUt->reorg_UT();
}

////
void ColoredCoinTracker_UT::registerAddresses(std::set<BinaryData>& addrSet)
{
   std::vector<BinaryData> addrVec;
   for (auto& addr : addrSet)
      addrVec.emplace_back(addr);

   auto&& regID = walletObj_->registerAddresses(addrVec, false);
   BinaryDataRef idRef; idRef.setRef(regID);

   auto act = std::dynamic_pointer_cast<ColoredCoinTestACT>(actPtr_);
   while (true)
   {
      auto&& notifVec = act->popRefreshNotif();
      if (notifVec.size() != 1)
         continue;

      if (notifVec[0] == idRef)
         return;
   }
}

void ColoredCoinTracker_UT::update_UT()
{
   auto&& addrSet = update();
   registerAddresses(addrSet);
}

void ColoredCoinTracker_UT::zcUpdate_UT()
{
   auto&& addrSet = zcUpdate();
   registerAddresses(addrSet);
}

void ColoredCoinTracker_UT::reorg_UT()
{
   reorg(true);
}

////////////////////////////////////////////////////////////////////////////////
TEST_F(TestCCoin, Initial_balances)
{
   InitialFund();

   const auto &gaBalance = rootWallet_->getAddrBalance(genesisAddr_);
   ASSERT_FALSE(gaBalance.empty());
   EXPECT_EQ(gaBalance[0], COIN);

   for (size_t i = 0; i < usersCount_; i++) {
      const auto &ccBalance = userWallets_[i]->getAddrBalance(userCCAddresses_[i]);
      ASSERT_FALSE(ccBalance.empty()) << i << " wallet " << userWallets_[i]->walletId();
      EXPECT_EQ(ccBalance[0], 100 * ccLotSize_) << i << " wallet " << userWallets_[i]->walletId();
      const auto &fundBalance = userWallets_[i]->getAddrBalance(userFundAddresses_[i]);
      ASSERT_FALSE(fundBalance.empty()) << i << " wallet " << userWallets_[i]->walletId();
      EXPECT_EQ(fundBalance[0], 50 * COIN) << i << " wallet " << userWallets_[i]->walletId();
   }

   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);
}

////
TEST_F(TestCCoin, Case_TxProcessOrder1)
{
   InitialFund({ userCCAddresses_[0], userCCAddresses_[2] });

   auto&& cct = makeCct();
   cct->goOnline();

   const uint64_t gaBalance = COIN + (usersCount_ - 2) * 100 * ccLotSize_;

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 1);
   EXPECT_EQ(utxosB.size(), 1);

   //tx #1
   const uint amountCC = 50;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.ccChange = userCCAddresses_[9];
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
   update(cct);

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[9]->getAddrBalance(userCCAddresses_[9])[0], (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], amountCC * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), amountCC * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);

   //tx #2
   ccsB.ccChange = userCCAddresses_[8];
   ccsB.xbtValue_ = COIN;

   CCoinSpender ccsC;
   ccsC.ccAddr_ = userCCAddresses_[7];
   ccsC.xbtAddr_ = userFundAddresses_[2];
   ccsC.ccValue_ = 25 * ccLotSize_;

   auto utxosC1 = GetUTXOsFor(ccsB.ccAddr_);
   ASSERT_FALSE(utxosC1.empty());
   auto utxosC2 = GetUTXOsFor(ccsC.xbtAddr_);
   ASSERT_FALSE(utxosC2.empty());

   tx = CreateCJtx(utxosC1, utxosC2, ccsB, ccsC);
   MineBlocks(6);
   update(cct);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[7].prefixed()), 25 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);

   //tx #3
   ccsC.ccChange = userCCAddresses_[3];
   ccsC.xbtValue_ = COIN;

   CCoinSpender ccsD;
   ccsD.ccAddr_ = userCCAddresses_[5];
   ccsD.xbtAddr_ = userFundAddresses_[3];
   ccsD.ccValue_ = 150 * ccLotSize_;

   auto utxosD1 = GetUTXOsFor(ccsC.ccAddr_);
   ASSERT_FALSE(utxosD1.empty());
   auto utxosD2 = GetUTXOsFor(ccsD.xbtAddr_);
   ASSERT_FALSE(utxosD2.empty());
   auto utxosD3 = GetUTXOsFor(genesisAddr_);
   ASSERT_FALSE(utxosD3.empty());

   tx = CreateCJtx(utxosD1, utxosD2, ccsC, ccsD, utxosD3);
   MineBlocks(6);
   update(cct);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[3].prefixed()), 326407 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[5].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[7].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);


   //
   for (const auto &utxo : utxosD1) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }
   for (const auto &utxo : utxosD3) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }

   const auto utxosD = GetUTXOsFor(ccsD.ccAddr_);
   ASSERT_FALSE(utxosD.empty());
   for (const auto &utxo : utxosD) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }

   //set up new cct object, should have same balance as first cct;
   auto&& cct2 = makeCct();
   cct2->goOnline();
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[3].prefixed()), 326407 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[5].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[7].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, Case_TxProcessOrder2)
{
   InitialFund({ userCCAddresses_[0], userCCAddresses_[2] });

   auto&& cct = makeCct();
   cct->goOnline();

   const uint64_t gaBalance = COIN + (usersCount_ - 2) * 100 * ccLotSize_;

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 1);
   EXPECT_EQ(utxosB.size(), 1);

   //tx #1
   const uint amountCC = 50;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.ccChange = userCCAddresses_[9];
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
   update(cct);

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[9]->getAddrBalance(userCCAddresses_[9])[0], (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], amountCC * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), amountCC * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);

   //tx #2
   ccsB.ccChange = userCCAddresses_[8];
   ccsB.xbtValue_ = COIN;

   CCoinSpender ccsC;
   ccsC.ccAddr_ = userCCAddresses_[7];
   ccsC.xbtAddr_ = userFundAddresses_[2];
   ccsC.ccValue_ = 25 * ccLotSize_;

   auto utxosC1 = GetUTXOsFor(ccsB.ccAddr_);
   ASSERT_FALSE(utxosC1.empty());
   auto utxosC2 = GetUTXOsFor(ccsC.xbtAddr_);
   ASSERT_FALSE(utxosC2.empty());

   tx = CreateCJtx(utxosC1, utxosC2, ccsB, ccsC);
   MineBlocks(6);
   update(cct);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 100 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[7].prefixed()), 25 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);

   //tx #3
   ccsC.ccChange = userCCAddresses_[3];
   ccsC.xbtValue_ = COIN;

   CCoinSpender ccsD;
   ccsD.ccAddr_ = userCCAddresses_[5];
   ccsD.xbtAddr_ = userFundAddresses_[3];
   ccsD.ccValue_ = 110 * ccLotSize_;

   auto utxosD1 = GetUTXOsFor(ccsC.ccAddr_);
   ASSERT_FALSE(utxosD1.empty());
   auto utxosD2 = GetUTXOsFor(ccsD.xbtAddr_);
   ASSERT_FALSE(utxosD2.empty());
   auto utxosD3 = GetUTXOsFor(userCCAddresses_[2]);
   ASSERT_FALSE(utxosD3.empty());

   tx = CreateCJtx(utxosD1, utxosD2, ccsC, ccsD, utxosD3);
   MineBlocks(6);
   update(cct);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[3].prefixed()), 15 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[5].prefixed()), 110 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[7].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);


   //
   for (const auto &utxo : utxosD1) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }
   for (const auto &utxo : utxosD3) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }

   const auto utxosD = GetUTXOsFor(ccsD.ccAddr_);
   ASSERT_FALSE(utxosD.empty());
   for (const auto &utxo : utxosD) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }

   //set up new cct object, should have same balance as first cct;
   auto&& cct2 = makeCct();
   cct2->goOnline();
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[2].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[3].prefixed()), 15 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[5].prefixed()), 110 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[7].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[8].prefixed()), (amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, Case_1CC_2CC)
{
   InitialFund({ userCCAddresses_[0] });

   auto&& cct = makeCct();
   cct->goOnline();

   const uint64_t gaBalance = COIN + (usersCount_ - 1) * 100 * ccLotSize_;

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 100 * ccLotSize_);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
   std::vector<UTXO> utxosB = GetUTXOsFor(userFundAddresses_[1]);

   EXPECT_EQ(utxosA.size(), 1);
   EXPECT_EQ(utxosB.size(), 1);

   const uint amountCC = 50;
   CCoinSpender ccsA;
   ccsA.ccAddr_ = userCCAddresses_[0];
   ccsA.ccChange = userCCAddresses_[9];
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

   update(cct);

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[9]->getAddrBalance(userCCAddresses_[9])[0], (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], amountCC * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), gaBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), amountCC * ccLotSize_);

   ccsB.ccChange = userCCAddresses_[8];
   ccsB.xbtValue_ = COIN;

   CCoinSpender ccsC;
   ccsC.ccAddr_ = userCCAddresses_[2];
   ccsC.xbtAddr_ = userFundAddresses_[2];
   ccsC.ccChange = userCCAddresses_[9];
   ccsC.ccValue_ = 25 * ccLotSize_;

   auto utxosC1 = GetUTXOsFor(ccsB.ccAddr_);
   ASSERT_FALSE(utxosC1.empty());
   auto utxosC2 = GetUTXOsFor(ccsC.xbtAddr_);
   ASSERT_FALSE(utxosC2.empty());
   auto utxosC3 = GetUTXOsFor(userCCAddresses_[9]);
   EXPECT_FALSE(utxosC3.empty());

   // add change from previous TX as an input - sort it after XBT inputs
   tx = CreateCJtx(utxosC1, utxosC2, ccsB, ccsC, utxosC3);
   MineBlocks(6);
   update(cct);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[2].prefixed()), 25 * ccLotSize_);
   // as we're using 2 inputs: [1] (50) and [9] (50), after subtracting tx #2 amount (25),
   // we should get 75 on change address [8] (50 + 50 - 25)
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), (2*amountCC - 25) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   std::set<BinaryData> participatingAddresses = {    // all addresses that participated in the above transactions
      userCCAddresses_[0], userCCAddresses_[1],       // they simulate CCLeaf and its balance
      userCCAddresses_[2], userCCAddresses_[8], userCCAddresses_[9]
   };    // should be 100 CCs in sum (as no amount was sent outside)
   EXPECT_EQ(cct->getConfirmedCcValueForAddresses(participatingAddresses), 100 * ccLotSize_);
   // no unconfirmed balance should retain after 6 confirmations
   EXPECT_EQ(cct->getUnconfirmedCcValueForAddresses(participatingAddresses), 0);

   for (const auto &utxo : utxosC1) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }
   for (const auto &utxo : utxosC3) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }

   const auto utxosC = GetUTXOsFor(ccsC.ccAddr_);
   ASSERT_FALSE(utxosC.empty());
   for (const auto &utxo : utxosC) {
      EXPECT_TRUE(cct->isTxHashValid(utxo.getTxHash()));
   }
}

////
TEST_F(TestCCoin, Case_MultiUnorderedCC_2CC)
{
   InitialFund();

   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for(size_t i = 10; i < 100; i+=10)
   {
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] },  i * ccLotSize_);
      MineBlocks(6);
   }
   UpdateAllBalances();

   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   update(cct);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
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
   EXPECT_EQ(tx.getNumTxOut(), 4 );
   MineBlocks(6);
   UpdateAllBalances();

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 5 * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   update(cct);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 5 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, Revoke)
{
   InitialFund();
   FundFromCoinbase({ revocationAddr_ }, 50 * COIN);
   MineBlocks(6);

   EXPECT_EQ(rootWallet_->getAddrBalance(genesisAddr_)[0], COIN);

   for (size_t i = 0; i < usersCount_; ++i) {
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userCCAddresses_[i])[0], 100 * ccLotSize_);
      EXPECT_EQ(userWallets_[i]->getAddrBalance(userFundAddresses_[i])[0], 50 * COIN);
   }

   auto&& cct = makeCct();
   cct->addRevocationAddress(revocationAddr_);
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send cc from addr9 to addr0
   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[9]);
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
   update(cct);

   //check cc balances
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 50 * ccLotSize_);

   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //revoke addr9
   revoke(userCCAddresses_[9]);

   //confirm the tx
   MineBlocks(1);
   update(cct);

   //check address has no more cc
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send more coins to addr9
   SimpleSendMany(genesisAddr_, { userCCAddresses_[9] }, 150 * ccLotSize_);
   MineBlocks(6);
   update(cct);

   //check it still has no cc value
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 1; i < usersCount_ - 1; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //send cc from addr8 to addr1
   std::vector<UTXO> utxosC = GetCCUTXOsFor(cct, userCCAddresses_[8]);
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
   update(cct);

   //check balances
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), 160 * ccLotSize_);

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[8].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 2; i < usersCount_ - 2; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   //bootstrap fresh cct, check balances are valid (retroaction check)
   auto cct2 = makeCct();
   cct2->addRevocationAddress(revocationAddr_);
   cct2->goOnline();

   //check balances
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[0].prefixed()), 150 * ccLotSize_);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[1].prefixed()), 160 * ccLotSize_);

   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[8].prefixed()), 0);
   EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[9].prefixed()), 0);

   //check other addresses are untouched
   for (size_t i = 2; i < usersCount_ - 2; i++)
      EXPECT_EQ(cct2->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);
}

////
TEST_F(TestCCoin, Case_MultiUnorderedCC_NoChange)
{
   InitialFund();
   
   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for(size_t i = 10; i < 100; i+=10) { // 10..90.. +100init = 550 total
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] },  i * ccLotSize_);
      MineBlocks(6);
   }
   UpdateAllBalances();

   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   update(cct);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
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

   update(cct);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, ZeroConf)
{
   InitialFund();

   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   for (size_t i = 10; i < 100; i += 10) { // 10..90.. +100init = 550 total
      SimpleSendMany(genesisAddr_, { userCCAddresses_[0] }, i * ccLotSize_);

      size_t i_dec = i / 10;
      uint64_t ccbal = (i_dec * (i_dec + 1)) * 5 + 100;
      zcUpdate(cct);
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), ccbal * ccLotSize_);

      MineBlocks(6);
      update(cct);
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), ccbal * ccLotSize_);
   }

   UpdateAllBalances();
   uint64_t newCcBalance = 450 * ccLotSize_ + 1000 * 9;

   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 550 * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
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

   zcUpdate(cct);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);

   MineBlocks(6);
   UpdateAllBalances();

   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], 0);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   update(cct);
   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN - newCcBalance);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), 0);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, ZeroConfChain)
{
   InitialFund();

   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   std::set<BinaryData> addrSet;
   for (auto& addr : userCCAddresses_)
      addrSet.insert(addr.prefixed());

   auto createTxLbd = [](uint64_t value, std::shared_ptr<ScriptSpender> spender, bs::Address& addr)->Tx
   {      
      Signer signer;
      signer.addSpender(spender);
      signer.addRecipient(addr.getRecipient(bs::XBTAmount{ value }));
      
      auto script = spender->getOutputScript();
      auto changeAddr = BtcUtils::getScrAddrForScript(script);
      auto changeRec = std::make_shared<Recipient_P2WPKH>(
         changeAddr.getSliceCopy(1, 20), spender->getValue() - value);
      signer.addRecipient(changeRec);

      signer.sign();
      Tx tx(signer.serialize());
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

      wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
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
         spender->setFeed(signWallet->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWallet, passphrase_);
         const auto&& lock = signWallet->lockDecryptedContainer();
         zc = createTxLbd(y*ccLotSize_, spender, userCCAddresses_[i]);
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
      zcUpdate(cct);
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), ccbal * ccLotSize_);

      //update utxo with genesis address change for next zc in the chain
      auto txOut = zc.getTxOutCopy(1);
      UTXO zcUtxo;
      zcUtxo.unserializeRaw(txOut.serialize());
      zcUtxo.txHash_ = zc.getThisHash();
      zcUtxo.txOutIndex_ = 1;

      utxo = zcUtxo;
   }

   EXPECT_EQ(cct->getConfirmedCcValueForAddresses(addrSet), 1000 * ccLotSize_);
   EXPECT_EQ(cct->getUnconfirmedCcValueForAddresses(addrSet), 550 * ccLotSize_);

   for (unsigned y = 0; y < 10; y++)
   {
      uint64_t ccbal = (y + 1) * 10 + 100;
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[y].prefixed()), ccbal * ccLotSize_);
   }

   //mine 10 blocks one at a time and check balances
   for (unsigned i = 0; i < 10; i++)
   {
      MineBlocks(1);
      update(cct);

      for (unsigned y = 0; y < 10; y++)
      {
         uint64_t ccbal = (y+1)*10 + 100;
         EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[y].prefixed()), ccbal * ccLotSize_);
      }
   }

   EXPECT_EQ(cct->getConfirmedCcValueForAddresses(addrSet), 1550 * ccLotSize_);
   EXPECT_EQ(cct->getUnconfirmedCcValueForAddresses(addrSet), 0);
}

////
TEST_F(TestCCoin, Reorg)
{
   InitialFund();

   auto&& cct = makeCct();
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   auto createTxLbd = [](std::shared_ptr<ScriptSpender> spender, 
      const std::map<bs::Address, uint64_t>& recipients)->Tx
   {
      Signer signer;
      signer.addSpender(spender);

      uint64_t total = 0;
      for (auto& recipient : recipients)
      {
         total += recipient.second;
         signer.addRecipient(recipient.first.getRecipient(bs::XBTAmount{ recipient.second }));
      }

      if (total > spender->getValue())
         throw std::runtime_error("spending more than the outpoint value");

      auto script = spender->getOutputScript();
      auto changeAddr = BtcUtils::getScrAddrForScript(script);
      auto changeRec = std::make_shared<Recipient_P2WPKH>(
         changeAddr.getSliceCopy(1, 20), spender->getValue() - total);
      signer.addRecipient(changeRec);

      signer.sign();
      Tx tx(signer.serialize());
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

      wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
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
      spender->setFeed(signWalletMain->getResolver());
      const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);

      std::vector<bs::Address> addresses;
      addresses.push_back(bs::Address::fromHash(utxoMain.getRecipientScrAddr()));

      auto lock = signWalletMain->lockDecryptedContainer();

      std::map<bs::Address, uint64_t> recipients;
      recipients.insert(std::make_pair(userCCAddresses_[0], 100 * ccLotSize_));
      recipients.insert(std::make_pair(userCCAddresses_[1], 100 * ccLotSize_));
      auto&& zc = createTxLbd(spender, recipients);

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
         spender->setFeed(signWalletMain->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);
         auto lock = signWalletMain->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[i], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
         spender->setFeed(signWalletA->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletA, passphrase_);
         auto lock = signWalletA->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
         spender->setFeed(signWalletB->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletB, passphrase_);
         auto lock = signWalletB->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
   update(cct);
   zcUpdate(cct);

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
   reorg(cct);
   update(cct);
   zcUpdate(cct);

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
   reorg(cct);
   update(cct);
   zcUpdate(cct);

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

////
TEST_F(TestCCoin, Case_1CC_2CC_WithACT)
{
   InitialFund();

   auto cct = std::make_shared<ColoredCoinTracker>(ccLotSize_, envPtr_->armoryConnection());

   auto actPtr = std::make_shared<ColoredCoinTestACT_WithNotif>(envPtr_->armoryConnection().get());
   auto cctUT = (ColoredCoinTracker_UT*)cct.get();
   cctUT->setACT(actPtr);

   cct->addOriginAddress(genesisAddr_);
   cct->goOnline();

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; i++)
      EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   std::vector<UTXO> utxosA = GetCCUTXOsFor(cct, userCCAddresses_[0]);
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
   actPtr->waitOnNotif(DBNS_ZC);
   EXPECT_EQ(tx.getNumTxIn(), 2);
   EXPECT_EQ(tx.getNumTxOut(), 4);
   MineBlocks(6, false);
   actPtr->waitOnNotif(DBNS_NewBlock);
   UpdateAllBalances();


   EXPECT_EQ(userWallets_[0]->getAddrBalance(userCCAddresses_[0])[0], (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[0]->getAddrBalance(userFundAddresses_[0])[0], 51 * COIN);

   EXPECT_EQ(userWallets_[1]->getAddrBalance(userCCAddresses_[1])[0], (100 + amountCC) * ccLotSize_);
   EXPECT_EQ(userWallets_[1]->getAddrBalance(userFundAddresses_[1])[0], 49 * COIN - 1000);

   EXPECT_EQ(cct->getCcValueForAddress(genesisAddr_), COIN);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[0].prefixed()), (100 - amountCC) * ccLotSize_);
   EXPECT_EQ(cct->getCcValueForAddress(userCCAddresses_[1].prefixed()), (100 + amountCC) * ccLotSize_);
}

////
TEST_F(TestCCoin, Reorg_WithACT)
{
   InitialFund();

   ColoredCoinTracker cct(ccLotSize_, envPtr_->armoryConnection());
   
   auto actPtr = std::make_shared<ColoredCoinTestACT_WithNotif>(envPtr_->armoryConnection().get());
   auto cctUT = (ColoredCoinTracker_UT*)&cct;
   cctUT->setACT(actPtr);

   cct.addOriginAddress(genesisAddr_);
   cct.goOnline();

   EXPECT_EQ(cct.getCcValueForAddress(genesisAddr_), COIN);

   for (size_t i = 0; i < usersCount_; ++i)
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[i].prefixed()), 100 * ccLotSize_);

   auto createTxLbd = [](std::shared_ptr<ScriptSpender> spender,
      const std::map<bs::Address, uint64_t>& recipients)->Tx
   {
      Signer signer;
      signer.addSpender(spender);

      uint64_t total = 0;
      for (auto& recipient : recipients)
      {
         total += recipient.second;
         signer.addRecipient(recipient.first.getRecipient(bs::XBTAmount{ recipient.second }));
      }

      if (total > spender->getValue())
         throw std::runtime_error("spending more than the outpoint value");

      auto script = spender->getOutputScript();
      auto changeAddr = BtcUtils::getScrAddrForScript(script);
      auto changeRec = std::make_shared<Recipient_P2WPKH>(
         changeAddr.getSliceCopy(1, 20), spender->getValue() - total);
      signer.addRecipient(changeRec);

      signer.sign();
      Tx tx(signer.serialize());
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

      wallet->getSpendableTxOutList(cbTxOutList, UINT64_MAX);
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
      spender->setFeed(signWalletMain->getResolver());
      const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);
      auto lock = signWalletMain->lockDecryptedContainer();

      std::map<bs::Address, uint64_t> recipients;
      recipients.insert(std::make_pair(userCCAddresses_[0], 100 * ccLotSize_));
      recipients.insert(std::make_pair(userCCAddresses_[1], 100 * ccLotSize_));
      auto&& zc = createTxLbd(spender, recipients);

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
      actPtr->waitOnNotif(DBNS_ZC);

      //mine new block
      MineBlocks(1, false);
      actPtr->waitOnNotif(DBNS_NewBlock);
   }

   auto&& branchPointHash = getCurrentTopBlockHash();
   std::vector<BinaryData> vecMain, vecA, vecB;

   for (size_t i = 2; i < 6; i++)
   {
      //common zc
      auto y = (i + 1);
      {
         auto spender = std::make_shared<ScriptSpender>(utxoMain);
         spender->setFeed(signWalletMain->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletMain, passphrase_);
         auto lock = signWalletMain->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[i], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
         spender->setFeed(signWalletA->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletA, passphrase_);
         auto lock = signWalletA->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
         spender->setFeed(signWalletB->getResolver());
         const bs::core::WalletPasswordScoped passScoped(lockWalletB, passphrase_);
         auto lock = signWalletB->lockDecryptedContainer();

         std::map<bs::Address, uint64_t> recipients;
         recipients.insert(std::make_pair(userCCAddresses_[c], y*ccLotSize_));
         auto&& zc = createTxLbd(spender, recipients);
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
         actPtr->waitOnNotif(DBNS_ZC);
      }

      {
         Tx zc;
         zc.unserialize(vecA[i]);
         envPtr_->armoryInstance()->pushZC(vecA[i], i);
         actPtr->waitOnNotif(DBNS_ZC);
      }
   }

   //mine 3 blocks (leave 1 zc for each group)
   MineBlocks(3, false);
   actPtr->waitOnNotif(DBNS_NewBlock);

   auto&& branchATop = getCurrentTopBlockHash();

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = c + 1 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
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
   MineBlocks(4, false);
   actPtr->waitOnNotif(DBNS_NewBlock);
   actPtr->waitOnNotif(DBNS_ZC);

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = (c + 1) * 2 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
   }

   //reorg back to main branch
   setReorgBranchPoint(branchATop);
   envPtr_->armoryInstance()->pushZC(*vecA.rbegin(), 0, true);

   //mine 2 more blocks to go back on branch A
   MineBlocks(2, false);
   actPtr->waitOnNotif(DBNS_NewBlock);

   //check balances
   for (unsigned i = 2; i < 6; i++)
   {
      auto y = i + 1 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[i]), y*ccLotSize_);

      auto c = i + 4;
      y = c + 1 + 100;
      EXPECT_EQ(cct.getCcValueForAddress(userCCAddresses_[c]), y*ccLotSize_);
   }
}

//TODO:
//blackhole cc
//over assign cc

//cc chains within same block
//rbf & replacement cases
//create zc, revoke cc payer address, mine revoke, push zc
//create zc, revoke cc payer address, push both, mine only revoke

//mine multiple revokes for same address, check the revoke height is the lowest of them all
//tx height change from reorg vs cached height

//lite client
//save & load from snapshots


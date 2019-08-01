#include <gtest/gtest.h>
#include <QDebug>
#include "CoinSelection.h"
#include "CoreHDWallet.h"
#include "InprocSigner.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "TestEnv.h"

using std::make_unique;

double getUTXOsum(const std::shared_ptr<bs::sync::Wallet> &wallet, const std::vector<UTXO> &utxos)
{
   double sum = 0;
   for (const auto &utxo : utxos) {
      sum += wallet->getTxBalance(utxo.getValue());
   }
   return sum;
}
/*
TEST(Armory, Balances_UTXOs)
{
   TestEnv::requireArmory();
   const unsigned int ccLotSize = 307;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   auto wallet = std::make_shared<bs::core::hd::Wallet>(
      "test", "", 
      NetworkType::TestNet, SecureBinaryData("passphrase"),
      "./", TestEnv::logger());

   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);

   auto ccGrp = wallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   ASSERT_NE(ccGrp, nullptr);
   auto ccLeaf = ccGrp->createLeaf("BLK");
   ASSERT_NE(ccLeaf, nullptr);

   const auto addr1 = leaf->getNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->getNewExtAddress();
   const auto addr3 = leaf->getNewExtAddress();
   const auto changeAddr = leaf->getNewChangeAddress();

   const auto ccAddr1 = ccLeaf->getNewExtAddress();
   const auto ccAddr2 = ccLeaf->getNewExtAddress();
   const auto ccChangeAddr = ccLeaf->getNewChangeAddress();

   auto inprocSigner = std::make_shared<InprocSigner>(wallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   const auto syncWallet = syncMgr->getHDWalletById(wallet->walletId());
   ASSERT_NE(syncWallet, nullptr);
   const auto syncLeaf = syncMgr->getWalletById(leaf->walletId());
   ASSERT_NE(syncLeaf, nullptr);
   const auto syncCcLeaf = syncMgr->getWalletById(ccLeaf->walletId());
   ASSERT_NE(syncCcLeaf, nullptr);

   syncCcLeaf->setData(ccLotSize);
   syncCcLeaf->setData(addr1.display());  // GA
   syncWallet->registerWallet(TestEnv::armory());

   auto curHeight = TestEnv::armory()->topBlock();
   const auto &cbSend = [](QString result) {
      ASSERT_FALSE(result.isEmpty());
   };
   TestEnv::regtestControl()->SendTo(0.5, addr1, cbSend);
   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(syncLeaf);
   syncLeaf->updateBalances();
   EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 0.5);
   TestEnv::blockMonitor()->waitForWalletReady(syncCcLeaf);

   const auto &cbTxOutList1 = [ccAddr1, leaf, syncLeaf, fee, changeAddr, ccLotSize](std::vector<UTXO> inputs1) {
      const uint64_t amount1 = ccLotSize * 1000;
      ASSERT_FALSE(inputs1.empty());
      const auto recipient1 = ccAddr1.getRecipient(amount1);
      ASSERT_NE(recipient1, nullptr);
      const auto txReq1 = syncLeaf->createTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
      const auto txSigned1 = leaf->signTXRequest(txReq1);
      ASSERT_FALSE(txSigned1.isNull());

      const auto &cbSendTX = [](bool result) {
         ASSERT_TRUE(result);
      };
      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr()), cbSendTX);
   };
   ASSERT_TRUE(syncLeaf->getSpendableTxOutList(cbTxOutList1, nullptr));

   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(syncCcLeaf));
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(syncLeaf));
   syncCcLeaf->updateBalances();
   syncLeaf->updateBalances();

   ASSERT_TRUE(syncCcLeaf->isBalanceAvailable());
   EXPECT_EQ(syncCcLeaf->getSpendableBalance(), 1000);

   const auto &cbCCTxOutList = [syncCcLeaf, ccAddr1](std::vector<UTXO> ccUTXOs) {
      EXPECT_FALSE(ccUTXOs.empty());
      EXPECT_EQ(getUTXOsum(syncCcLeaf, ccUTXOs), syncCcLeaf->getSpendableBalance());

      const auto &cbBalance = [ccAddr1](std::vector<uint64_t> balance) {
         EXPECT_EQ(balance[0], 1000);
      };
      syncCcLeaf->getAddrBalance(ccAddr1, cbBalance);
   };
   syncCcLeaf->getSpendableTxOutList(cbCCTxOutList, nullptr);

   const auto &cbTxOutList2 = [syncLeaf, leaf, addr2, fee, changeAddr](std::vector<UTXO> inputs2) {
      const uint64_t amount2 = 0.1 * BTCNumericTypes::BalanceDivider;
      ASSERT_FALSE(inputs2.empty());
      const auto recipient2 = addr2.getRecipient(amount2);
      ASSERT_NE(recipient2, nullptr);
      const auto txReq2 = syncLeaf->createTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
      const auto txSigned2 = leaf->signTXRequest(txReq2);
      ASSERT_FALSE(txSigned2.isNull());

      const auto &cbSendTX = [](bool result) {
         ASSERT_TRUE(result);
      };
      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr()), cbSendTX);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList2, nullptr);

   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   syncLeaf->updateBalances();
   TestEnv::blockMonitor()->waitForWalletReady(syncCcLeaf);

   const auto &cbAddrTxN1 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncLeaf->getAddrTxN(addr2, cbAddrTxN1);

   const auto &cbAddrBal1 = [](std::vector<uint64_t> balance) {
      EXPECT_EQ(balance[0], 0.1 * BTCNumericTypes::BalanceDivider);
   };
   syncLeaf->getAddrBalance(addr2, cbAddrBal1);

   const auto &cbAddrTxN2 = [](uint32_t txn) {
      EXPECT_EQ(txn, 3);
   };
   syncLeaf->getAddrTxN(changeAddr, cbAddrTxN2);

   const auto &cbAddrBal2 = [](std::vector<uint64_t> balance) {
      EXPECT_GT(balance[0], 0);
   };
   syncLeaf->getAddrBalance(changeAddr, cbAddrBal2);

   const auto &cbTxOutList = [syncLeaf](std::vector<UTXO> inputs) {
      EXPECT_EQ(getUTXOsum(syncLeaf, inputs), syncLeaf->getSpendableBalance());
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList, nullptr);

   const auto &cbTxOutList3 = [ccAddr2, ccLeaf, syncCcLeaf, ccChangeAddr, ccLotSize](std::vector<UTXO> inputs3) {
      const uint64_t amount3 = ccLotSize * 900;
      ASSERT_FALSE(inputs3.empty());
      const auto recipient3 = ccAddr2.getRecipient(amount3);
      ASSERT_NE(recipient3, nullptr);
      const auto txReq3 = syncCcLeaf->createTXRequest(inputs3, { recipient3 }, 50 * ccLotSize, false, ccChangeAddr);
      const auto txSigned3 = ccLeaf->signTXRequest(txReq3);
      ASSERT_FALSE(txSigned3.isNull());

      const auto &cbSendTX = [](bool result) {
         ASSERT_TRUE(result);
      };
      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned3.toHexStr()), cbSendTX);
   };
   syncCcLeaf->getSpendableTxOutList(cbTxOutList3, nullptr);

   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(syncCcLeaf);
   syncCcLeaf->updateBalances();

   EXPECT_EQ(syncCcLeaf->getSpendableBalance(), 950);

   const auto &cbAddrTxN3 = [](uint32_t txn) {
      EXPECT_EQ(txn, 2);
   };
   syncCcLeaf->getAddrTxN(ccAddr1, cbAddrTxN3);

   const auto &cbAddrBal3 = [](std::vector<uint64_t> balance) {
      EXPECT_EQ(balance[0], 0);
   };
   syncCcLeaf->getAddrBalance(ccAddr1, cbAddrBal3);

   const auto &cbAddrTxN4 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncCcLeaf->getAddrTxN(ccAddr2, cbAddrTxN4);

   const auto &cbAddrBal4 = [](std::vector<uint64_t> balance) {
      EXPECT_EQ(balance[0], 900);
   };
   syncCcLeaf->getAddrBalance(ccAddr2, cbAddrBal4);

   const auto &cbAddrTxN5 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncCcLeaf->getAddrTxN(ccChangeAddr, cbAddrTxN5);

   const auto &cbAddrBal5 = [](std::vector<uint64_t> balance) {
      EXPECT_EQ(balance[0], 50);
   };
   syncCcLeaf->getAddrBalance(ccChangeAddr, cbAddrBal5);
}

TEST(Armory, Ledger)
{
   TestEnv::requireArmory();
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;
   const uint64_t amount = 0.1 * BTCNumericTypes::BalanceDivider;
   auto wallet = std::make_shared<bs::core::hd::Wallet>(
      "test", "", 
      NetworkType::TestNet, SecureBinaryData("passphrase"),
      "./", TestEnv::logger());

   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);

   const auto changeAddr = leaf->getNewChangeAddress();
   const auto addr1 = leaf->getNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->getNewExtAddress();
   const auto addr3 = leaf->getNewExtAddress();

   auto inprocSigner = std::make_shared<InprocSigner>(wallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(wallet->walletId());
   ASSERT_NE(syncWallet, nullptr);
   auto syncLeaf = syncMgr->getWalletById(leaf->walletId());
   ASSERT_NE(syncLeaf, nullptr);

   syncWallet->registerWallet(TestEnv::armory());

   auto curHeight = TestEnv::armory()->topBlock();
   const auto &cbSend = [](QString result) {
      ASSERT_FALSE(result.isEmpty());
   };
   TestEnv::regtestControl()->SendTo(0.5, addr1, cbSend);
   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(syncLeaf));

   const auto &cbBalance1 = [syncLeaf](std::vector<uint64_t>) {
      EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 0.5);
   };
   syncLeaf->updateBalances(cbBalance1);

   const auto &cbTxOutList1 = [leaf, syncLeaf, amount, addr2, fee, changeAddr](std::vector<UTXO> inputs1) {
      ASSERT_FALSE(inputs1.empty());
      const auto recipient1 = addr2.getRecipient(amount);
      ASSERT_NE(recipient1, nullptr);
      const auto txReq1 = syncLeaf->createTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
      const auto txSigned1 = leaf->signTXRequest(txReq1);
      ASSERT_FALSE(txSigned1.isNull());
      const auto &cbSendTx = [](bool result) {
         ASSERT_TRUE(result);
      };
      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr()), cbSendTx);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList1, nullptr, amount + fee);

   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(syncLeaf);

   const auto &cbBalance2 = [syncLeaf](std::vector<uint64_t>) {
      EXPECT_EQ(syncLeaf->getSpendableBalance(), 0.5 - 0.0001);
   };
   syncLeaf->updateBalances(cbBalance2);

   const auto &cbAddrTxN1 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncLeaf->getAddrTxN(addr2, cbAddrTxN1);

   const auto &cbAddrBal1 = [](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], 0.1 * BTCNumericTypes::BalanceDivider);
   };
   syncLeaf->getAddrBalance(addr2, cbAddrBal1);

   const auto &cbAddrTxN2 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncLeaf->getAddrTxN(changeAddr, cbAddrTxN2);

   const auto &cbAddrBal2 = [](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], 0);
   };
   syncLeaf->getAddrBalance(addr1, cbAddrBal2);

   const auto &cbTxOutList2 = [leaf, syncLeaf, amount, addr3, fee, changeAddr](std::vector<UTXO> inputs2) {
      ASSERT_FALSE(inputs2.empty());
      const auto recipient2 = addr3.getRecipient(amount);
      ASSERT_NE(recipient2, nullptr);
      const auto txReq2 = syncLeaf->createTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
      const auto txSigned2 = leaf->signTXRequest(txReq2);
      ASSERT_FALSE(txSigned2.isNull());
      const auto &cbSendTx = [](bool result) {
         ASSERT_TRUE(result);
      };
      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr()), cbSendTx);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList2, nullptr, amount + fee);

   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(syncLeaf);
   syncLeaf->updateBalances();

   const auto &cbAddrTxN3 = [](uint32_t txn) {
      EXPECT_EQ(txn, 1);
   };
   syncLeaf->getAddrTxN(addr3, cbAddrTxN3);

   const auto &cbAddrBal3 = [](std::vector<uint64_t> balances) {
      EXPECT_EQ(balances[0], 0.1 * BTCNumericTypes::BalanceDivider);
   };
   syncLeaf->getAddrBalance(addr3, cbAddrBal3);

   const auto &cbAddrTxN4 = [](uint32_t txn) {
      EXPECT_GE(txn, 2);
   };
   syncLeaf->getAddrTxN(changeAddr, cbAddrTxN4);

   const auto &cbAddrBal4 = [fee](std::vector<uint64_t> balances) {
      EXPECT_GE(balances[0], 0.3 * BTCNumericTypes::BalanceDivider - 2 * fee);
   };
   syncLeaf->getAddrBalance(changeAddr, cbAddrBal4);

   const auto &cbTxOutList3 = [syncLeaf](std::vector<UTXO> txOutList) {
      EXPECT_EQ(getUTXOsum(syncLeaf, txOutList), syncLeaf->getSpendableBalance());
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList3, nullptr);
   EXPECT_EQ(syncLeaf->getSpendableBalance(), 0.5 - 2 * 0.0001);

   const auto &cbDelegate = [](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      const auto &cbLedger = [](ReturnMessage<std::vector<ClientClasses::LedgerEntry>> ledger) {
         EXPECT_GE(ledger.get().size(), 2);
      };
      delegate->getHistoryPage(0, cbLedger);
   };
   TestEnv::armory()->getLedgerDelegateForAddress(syncLeaf->walletId(), changeAddr, cbDelegate);

   const auto &cbWalletsDelegate = [](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      const auto &cbLedger = [](ReturnMessage<std::vector<ClientClasses::LedgerEntry>> ledger) {
         EXPECT_FALSE(ledger.get().empty());
      };
      delegate->getHistoryPage(0, cbLedger);
   };
   TestEnv::armory()->getWalletsLedgerDelegate(cbWalletsDelegate);
}


TEST(Armory, CoinSelection)
{
   bs::core::hd::Wallet wallet("test", "", NetworkType::TestNet);
   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);

   const auto addrInput1 = leaf->getNewExtAddress();
   const auto addrInput2 = leaf->getNewExtAddress();
   const auto addrOutput = leaf->getNewExtAddress();

   const size_t witnessSize = 0;

   UTXO utxo1;
   utxo1.txHash_ = CryptoPRNG::generateRandom(32);
   utxo1.txOutIndex_ = 0;
   utxo1.txHeight_ = 123456;
   utxo1.txIndex_ = 0;
   utxo1.value_ = 12300000;
   utxo1.script_.append(0);
   utxo1.script_.append(0x14);
   utxo1.script_.append(addrInput1.unprefixed());
   utxo1.witnessDataSizeBytes_ = witnessSize;

   UTXO utxo2;
   utxo2.txHash_ = CryptoPRNG::generateRandom(32);
   utxo2.txOutIndex_ = 0;
   utxo2.txHeight_ = 234567;
   utxo2.txIndex_ = 0;
   utxo2.value_ = 23400000;
   utxo2.script_.append(0);
   utxo2.script_.append(0x14);
   utxo2.script_.append(addrInput2.unprefixed());
   utxo2.witnessDataSizeBytes_ = witnessSize;

   bs::core::wallet::TXSignRequest txReq;
   txReq.inputs = { utxo1, utxo2 };
   txReq.recipients = { addrOutput.getRecipient((uint64_t)35699105) };  // -895 satoshi = 179 * 5

   const auto size1 = txReq.estimateTxVirtSize();
   EXPECT_EQ(size1, 179);

   std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipMap2;
//   recipMap2[0] = addrOutput.getRecipient((uint64_t)35699000); // -1000 satoshi exactly
   txReq.recipients = { addrOutput.getRecipient((uint64_t)35698995) };  // -1000 satoshi - some dust

   const auto size2 = txReq.estimateTxVirtSize();
   EXPECT_EQ(size1, size2);
}
*/
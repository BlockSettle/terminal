#include <gtest/gtest.h>
#include <QDebug>
#include "HDWallet.h"
#include "SafeLedgerDelegate.h"
#include "TestEnv.h"


double getUTXOsum(const std::shared_ptr<bs::Wallet> &wallet, const std::vector<UTXO> &utxos)
{
   double sum = 0;
   for (const auto &utxo : utxos) {
      sum += wallet->GetTxBalance(utxo.getValue());
   }
   return sum;
}

TEST(Armory, Balances_UTXOs)
{
   const unsigned int ccLotSize = 307;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;
   bs::hd::Wallet wallet("test", "", false, NetworkType::TestNet);

   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);

   auto ccGrp = wallet.createGroup(bs::hd::CoinType::BlockSettle_CC);
   ASSERT_NE(ccGrp, nullptr);
   auto ccLeaf = ccGrp->createLeaf("BLK");
   ASSERT_NE(ccLeaf, nullptr);
   ccLeaf->setData(ccLotSize);

   const auto addr1 = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->GetNewExtAddress();
   const auto addr3 = leaf->GetNewExtAddress();
   const auto changeAddr = leaf->GetNewChangeAddress();

   ccLeaf->setData(addr1.display<std::string>());  // GA
   const auto ccAddr1 = ccLeaf->GetNewExtAddress();
   const auto ccAddr2 = ccLeaf->GetNewExtAddress();
   const auto ccChangeAddr = ccLeaf->GetNewChangeAddress();
   wallet.RegisterWallet(PyBlockDataManager::instance());

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   ASSERT_FALSE(TestEnv::regtestControl()->SendTo(0.5, addr1).isEmpty());
   TestEnv::regtestControl()->GenerateBlocks(6);
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(leaf);
   leaf->UpdateBalanceFromDB();
   EXPECT_DOUBLE_EQ(leaf->GetSpendableBalance(), 0.5);
   TestEnv::blockMonitor()->waitForWalletReady(ccLeaf);

   const uint64_t amount1 = ccLotSize * 1000;
   const auto inputs1 = leaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs1.empty());
   const auto recipient1 = ccAddr1.getRecipient(amount1);
   ASSERT_NE(recipient1, nullptr);
   const auto txReq1 = leaf->CreateTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
   const auto txSigned1 = leaf->SignTXRequest(txReq1);
   ASSERT_FALSE(txSigned1.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(ccLeaf));
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(leaf));
   ccLeaf->UpdateBalanceFromDB();
   leaf->UpdateBalanceFromDB();

   ASSERT_TRUE(ccLeaf->isBalanceAvailable());
   EXPECT_EQ(ccLeaf->GetSpendableBalance(), 1000);
   auto ccUTXOs = ccLeaf->getSpendableTxOutList();
   EXPECT_FALSE(ccUTXOs.empty());
   EXPECT_EQ(getUTXOsum(ccLeaf, ccUTXOs), ccLeaf->GetSpendableBalance());
   EXPECT_EQ(ccLeaf->getAddrBalance(ccAddr1)[0], 1000);

   const uint64_t amount2 = 0.1 * BTCNumericTypes::BalanceDivider;
   const auto inputs2 = leaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs2.empty());
   const auto recipient2 = addr2.getRecipient(amount2);
   ASSERT_NE(recipient2, nullptr);
   const auto txReq2 = leaf->CreateTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
   const auto txSigned2 = leaf->SignTXRequest(txReq2);
   ASSERT_FALSE(txSigned2.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   leaf->UpdateBalanceFromDB();
   TestEnv::blockMonitor()->waitForWalletReady(ccLeaf);

   EXPECT_EQ(leaf->getAddrTxN(addr2), 1);
   EXPECT_EQ(leaf->getAddrBalance(addr2)[0], 0.1 * BTCNumericTypes::BalanceDivider);
   EXPECT_EQ(leaf->getAddrTxN(changeAddr), 3);
   EXPECT_GT(leaf->getAddrBalance(changeAddr)[0], 0);
   EXPECT_FALSE(leaf->getSpendableTxOutList().empty());
   EXPECT_EQ(getUTXOsum(leaf, leaf->getSpendableTxOutList()), leaf->GetSpendableBalance());

   const uint64_t amount3 = ccLotSize * 900;
   auto inputs3 = ccLeaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs3.empty());
   const auto recipient3 = ccAddr2.getRecipient(amount3);
   ASSERT_NE(recipient3, nullptr);
   const auto txReq3 = ccLeaf->CreateTXRequest(inputs3, { recipient3 }, 50 * ccLotSize, false, ccChangeAddr);
   const auto txSigned3 = ccLeaf->SignTXRequest(txReq3);
   ASSERT_FALSE(txSigned3.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned3.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(ccLeaf);
   ccLeaf->UpdateBalanceFromDB();

   EXPECT_EQ(ccLeaf->GetSpendableBalance(), 950);
   EXPECT_EQ(ccLeaf->getAddrTxN(ccAddr1), 2);
   EXPECT_EQ(ccLeaf->getAddrBalance(ccAddr1)[0], 0);
   EXPECT_EQ(ccLeaf->getAddrTxN(ccAddr2), 1);
   EXPECT_EQ(ccLeaf->getAddrBalance(ccAddr2)[0], 900);
   EXPECT_EQ(ccLeaf->getAddrTxN(ccChangeAddr), 1);
   EXPECT_EQ(ccLeaf->getAddrBalance(ccChangeAddr)[0], 50);
}

TEST(Armory, Ledger)
{
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;
   const uint64_t amount = 0.1 * BTCNumericTypes::BalanceDivider;
   bs::hd::Wallet wallet("test", "", false, NetworkType::TestNet);

   const auto &bdm = PyBlockDataManager::instance();
   ASSERT_NE(bdm, nullptr);

   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);

   const auto changeAddr = leaf->GetNewChangeAddress();
   const auto addr1 = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->GetNewExtAddress();
   const auto addr3 = leaf->GetNewExtAddress();

   wallet.RegisterWallet(PyBlockDataManager::instance());

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   ASSERT_FALSE(TestEnv::regtestControl()->SendTo(0.5, addr1).isEmpty());
   TestEnv::regtestControl()->GenerateBlocks(6);
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   ASSERT_TRUE(TestEnv::blockMonitor()->waitForWalletReady(leaf));
   leaf->UpdateBalanceFromDB();
   EXPECT_DOUBLE_EQ(leaf->GetSpendableBalance(), 0.5);

   const auto inputs1 = leaf->getSpendableTxOutList(amount + fee);
   ASSERT_FALSE(inputs1.empty());
   const auto recipient1 = addr2.getRecipient(amount);
   ASSERT_NE(recipient1, nullptr);
   const auto txReq1 = leaf->CreateTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
   const auto txSigned1 = leaf->SignTXRequest(txReq1);
   ASSERT_FALSE(txSigned1.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(leaf);
   leaf->UpdateBalanceFromDB();

   EXPECT_EQ(leaf->getAddrTxN(addr2), 1);
   EXPECT_EQ(leaf->getAddrBalance(addr2)[0], 0.1 * BTCNumericTypes::BalanceDivider);
   EXPECT_EQ(leaf->getAddrTxN(changeAddr), 1);
   EXPECT_EQ(leaf->getAddrBalance(addr1)[0], 0);
   EXPECT_EQ(leaf->GetSpendableBalance(), 0.5 - 0.0001);

   const auto inputs2 = leaf->getSpendableTxOutList(amount + fee);
   ASSERT_FALSE(inputs2.empty());
   const auto recipient2 = addr3.getRecipient(amount);
   ASSERT_NE(recipient2, nullptr);
   const auto txReq2 = leaf->CreateTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
   const auto txSigned2 = leaf->SignTXRequest(txReq2);
   ASSERT_FALSE(txSigned2.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   TestEnv::blockMonitor()->waitForWalletReady(leaf);
   leaf->UpdateBalanceFromDB();

   EXPECT_EQ(leaf->getAddrTxN(addr3), 1);
   EXPECT_EQ(leaf->getAddrBalance(addr3)[0], 0.1 * BTCNumericTypes::BalanceDivider);
   EXPECT_GE(leaf->getAddrTxN(changeAddr), 2);
   EXPECT_GE(leaf->getAddrBalance(changeAddr)[0], 0.3 * BTCNumericTypes::BalanceDivider - 2 * fee);
   EXPECT_FALSE(leaf->getSpendableTxOutList().empty());
   EXPECT_EQ(getUTXOsum(leaf, leaf->getSpendableTxOutList()), leaf->GetSpendableBalance());
   EXPECT_EQ(leaf->GetSpendableBalance(), 0.5 - 2 * 0.0001);

   const auto &addrLedgerDelegate = bdm->getLedgerDelegateForScrAddr(leaf->GetWalletId(), changeAddr.id());
   ASSERT_NE(addrLedgerDelegate, nullptr);
   const auto &ledger = addrLedgerDelegate->getHistoryPage(0);
   EXPECT_GE(ledger.size(), 2);

   const auto &walletsLedgerDelegate = bdm->GetWalletsLedgerDelegate();
   ASSERT_NE(walletsLedgerDelegate, nullptr);
   const auto &walletsLedger = walletsLedgerDelegate->getHistoryPage(0);
   EXPECT_FALSE(walletsLedger.empty());
}

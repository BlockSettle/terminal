#include <gtest/gtest.h>
#include <QComboBox>
#include <QDebug>
#include <QString>
#include <QLocale>
#include "ApplicationSettings.h"
#include "HDLeaf.h"
#include "HDNode.h"
#include "HDWallet.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletsManager.h"


TEST(TestWallet, BIP44_primary)
{
   const auto userId = SecureBinaryData(BinaryData::CreateFromHex("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
   ASSERT_NE(TestEnv::walletsMgr(), nullptr);

   TestEnv::walletsMgr()->CreateWallet("primary", "test", {}, true, { "Sample test seed", NetworkType::TestNet});
   EXPECT_TRUE(TestEnv::walletsMgr()->HasPrimaryWallet());
   TestEnv::walletsMgr()->SetUserId(userId);

   const auto wallet = TestEnv::walletsMgr()->GetPrimaryWallet();
   ASSERT_NE(wallet, nullptr);
   EXPECT_EQ(wallet->getName(), "primary");
   EXPECT_EQ(wallet->getDesc(), "test");
   EXPECT_EQ(wallet->getWalletId(), "2xZJsNXfL");
   EXPECT_EQ(wallet->getNode()->getSeed(), BinaryData("Sample test seed"));

   const auto grpAuth = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   ASSERT_NE(grpAuth, nullptr);

   const auto leafAuth = grpAuth->createLeaf(0);
   ASSERT_NE(leafAuth, nullptr);
   EXPECT_TRUE(leafAuth->isInitialized());
   EXPECT_EQ(leafAuth->getRootId().toHexStr(), "02300a2964a3d3c46ab231c1417f87e0fb116127c80fb361dc19e6ab675e3bef85");
   EXPECT_EQ(leafAuth->GetType(), bs::wallet::Type::Authentication);

   const auto grpXbt = wallet->getGroup(wallet->getXBTGroupType());
   ASSERT_NE(grpXbt, nullptr);

   const auto leafXbt = grpXbt->getLeaf(0);
   EXPECT_NE(leafXbt, nullptr);
   EXPECT_EQ(leafXbt->GetShortName(), "0");
   EXPECT_EQ(leafXbt->GetWalletName(), "primary/XBT [TESTNET]/0");
   EXPECT_EQ(leafXbt->GetWalletDescription(), "test");
   EXPECT_EQ(leafXbt->getRootId().toHexStr(), "0323373b6a7b4f3f8657637342b9254e7c5b4f210d8c28ac871a4e2340ee8cc70f");

   EXPECT_EQ(grpXbt->createLeaf(0), nullptr);

   const auto leaf1 = grpXbt->createLeaf(1);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_EQ(grpXbt->getNumLeaves(), 2);
   EXPECT_EQ(leaf1->GetShortName(), "1");
   EXPECT_EQ(leaf1->GetWalletName(), "primary/XBT [TESTNET]/1");
   EXPECT_EQ(leaf1->GetWalletDescription(), "test");
   EXPECT_TRUE(TestEnv::walletsMgr()->DeleteWalletFile(leaf1));
   EXPECT_EQ(grpXbt->getNumLeaves(), 1);

   EXPECT_EQ(TestEnv::walletsMgr()->GetDefaultWallet(), leafXbt);

   QComboBox cb;
   UiUtils::fillWalletsComboBox(&cb, TestEnv::walletsMgr());
   EXPECT_EQ(cb.count(), 1);
   EXPECT_EQ(cb.currentText(), QString::fromStdString(leafXbt->GetWalletName()));

   EXPECT_TRUE(TestEnv::walletsMgr()->DeleteWalletFile(wallet));
   EXPECT_FALSE(TestEnv::walletsMgr()->HasPrimaryWallet());
   TestEnv::walletsMgr()->SetUserId({});
}

TEST(TestWallet, BIP44_address)
{
   auto wallet = std::make_shared<bs::hd::Wallet>("test", "", false
      , bs::wallet::Seed{ "test seed", NetworkType::TestNet });
   ASSERT_NE(wallet, nullptr);
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_EQ(leaf->GetUsedAddressCount(), 0);

   const auto addr = leaf->GetNewExtAddress();
   EXPECT_EQ(addr.display<std::string>(), "tb1qzy2ml53plruqppz5kmyajeua8lphyq6nju729l");
   EXPECT_EQ(leaf->GetUsedAddressCount(), 1);

   const auto chgAddr = leaf->GetNewChangeAddress();
   EXPECT_EQ(chgAddr.display<std::string>(), "tb1qeyn6q6s42hrywnjf03vj8er55nzkdylzlv8mp3");
   EXPECT_EQ(leaf->GetUsedAddressCount(), 2);

   EXPECT_TRUE(wallet->eraseFile());
}

TEST(TestWallet, BIP44_derivation)
{
   const bs::hd::Node node(NetworkType::TestNet);
   const auto hardenedPath = bs::hd::Path::fromString("44'/1'/0'");
   auto path = hardenedPath;
   path.append(0);
   path.append(23);
   EXPECT_EQ(path.toString(), "m/44'/1'/0'/0/23");

   const auto nodeWallet = node.derive(hardenedPath);
   ASSERT_NE(nodeWallet, nullptr);
   nodeWallet->clearPrivKey();
   const auto nodeWallet1 = node.derive(hardenedPath);
   ASSERT_NE(nodeWallet1, nullptr);

   EXPECT_EQ(node.derive(path, true), nullptr);    // pubkey derivation from hardened path is impossible
   auto node1 = node.derive(path, false);
   auto node2 = nodeWallet->derive(bs::hd::Path({0, 23}), true);
   EXPECT_EQ(nodeWallet->derive(bs::hd::Path({ 0, 23 }), false), nullptr);
   auto node3 = nodeWallet1->derive(bs::hd::Path({ 0, 23 }), false);

   ASSERT_NE(node1, nullptr);
   ASSERT_NE(node2, nullptr);
   ASSERT_NE(node3, nullptr);
   EXPECT_FALSE(node1->pubCompressedKey().isNull());
   EXPECT_FALSE(node2->pubCompressedKey().isNull());
   EXPECT_EQ(node1->pubCompressedKey(), node2->pubCompressedKey());
   EXPECT_EQ(node2->pubCompressedKey(), node3->pubCompressedKey());
}

TEST(TestWallet, BIP44_WatchingOnly)
{
   const size_t nbAddresses = 10;
   auto wallet = std::make_shared<bs::hd::Wallet>("test", "", false, bs::wallet::Seed{"test seed", NetworkType::TestNet});
   ASSERT_NE(wallet, nullptr);
   EXPECT_FALSE(wallet->isWatchingOnly());
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf1 = grp->createLeaf(0);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_FALSE(leaf1->isWatchingOnly());
   for (size_t i = 0; i < nbAddresses; i++) {
      leaf1->GetNewExtAddress();
   }
   EXPECT_EQ(leaf1->GetUsedAddressCount(), nbAddresses);

   auto leaf2 = grp->createLeaf(1);
   ASSERT_NE(leaf2, nullptr);
   for (size_t i = 0; i < nbAddresses; i++) {
      leaf2->GetNewExtAddress();
   }
   EXPECT_EQ(leaf2->GetUsedAddressCount(), nbAddresses);

   auto woWallet = wallet->CreateWatchingOnly({});
   ASSERT_NE(woWallet, nullptr);
   EXPECT_EQ(woWallet->getGroups().size(), 1);
   auto woGroup = woWallet->getGroup(woWallet->getXBTGroupType());
   ASSERT_NE(woGroup, nullptr);

   auto woLeaf1 = woGroup->getLeaf(0);
   ASSERT_NE(woLeaf1, nullptr);
   EXPECT_TRUE(woLeaf1->isWatchingOnly());
   auto woLeaf2 = woGroup->getLeaf(1);
   ASSERT_NE(woLeaf2, nullptr);
   EXPECT_TRUE(woLeaf2->isWatchingOnly());
   EXPECT_EQ(woLeaf1->GetUsedAddressCount(), nbAddresses);
   EXPECT_EQ(woLeaf2->GetUsedAddressCount(), nbAddresses);

   const auto addrList = leaf1->GetUsedAddressList();
   for (size_t i = 0; i < nbAddresses; i++) {
      const auto index = woLeaf1->GetAddressIndex(addrList[i]);
      const auto addr = leaf1->CreateAddressWithIndex(index, addrList[i].getType());
      EXPECT_EQ(addrList[i].prefixed(), addr.prefixed()) << "addresses at " << index << " are unequal";
   }
   EXPECT_EQ(leaf1->GetUsedAddressCount(), nbAddresses);

   EXPECT_TRUE(woWallet->isWatchingOnly());
   EXPECT_NE(woWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth), nullptr);
   EXPECT_EQ(woGroup->createLeaf(2), nullptr);
}

TEST(TestWallet, Auth)
{
   ASSERT_NE(TestEnv::authAddrMgr(), nullptr);
   EXPECT_TRUE(TestEnv::authAddrMgr()->IsReady());
   QComboBox cb;
   UiUtils::fillAuthAddressesComboBox(&cb, TestEnv::authAddrMgr());
   const auto verifiedAddresses = TestEnv::authAddrMgr()->GetVerifiedAddressList();
   EXPECT_EQ(cb.count(), verifiedAddresses.size());
   EXPECT_EQ(cb.currentText(), verifiedAddresses[TestEnv::authAddrMgr()->getDefaultIndex()].display());

   for (const auto &addr : verifiedAddresses) {
      EXPECT_EQ(TestEnv::authAddrMgr()->GetState(addr), AddressVerificationState::Verified);
   }
}

TEST(TestWallet, Settlement)
{
   ASSERT_NE(TestEnv::walletsMgr(), nullptr);

   EXPECT_TRUE(TestEnv::walletsMgr()->CreateSettlementWallet());
   const auto wallet = TestEnv::walletsMgr()->GetSettlementWallet();
   ASSERT_NE(wallet, nullptr);
   EXPECT_EQ(wallet->GetWalletName(), "Settlement");
   EXPECT_EQ(wallet->GetType(), bs::wallet::Type::Settlement);
   EXPECT_FALSE(wallet->GetWalletId().empty());
}

TEST(TestWallet, Comments)
{
   const std::string addrComment("Test address comment");
   const std::string txComment("Test TX comment");

   bs::hd::Wallet hdWallet("hdWallet", "", false, NetworkType::TestNet);
   hdWallet.createStructure();
   const auto group = hdWallet.getGroup(hdWallet.getXBTGroupType());
   ASSERT_NE(group, nullptr);
   const auto wallet = group->getLeaf(0);
   ASSERT_NE(wallet, nullptr);

   auto addr = wallet->GetNewExtAddress(AddressEntryType_P2SH);
   ASSERT_FALSE(addr.isNull());
   hdWallet.saveToFile("test_wallet");
   hdWallet.RegisterWallet(PyBlockDataManager::instance());

   EXPECT_TRUE(wallet->SetAddressComment(addr, addrComment));
   EXPECT_EQ(wallet->GetAddressComment(addr), addrComment);

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (TestEnv::regtestControl()->GetBalance() < 50) {
      TestEnv::regtestControl()->GenerateBlocks(101);
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 101);
   }
   if (!TestEnv::regtestControl()->SendTo(0.01, addr).isEmpty()) {
      TestEnv::regtestControl()->GenerateBlocks(6);
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      if (TestEnv::blockMonitor()->waitForWalletReady(wallet)) {
         wallet->UpdateBalanceFromDB();
      }
   }

   const auto txReq = wallet->CreateTXRequest(wallet->getSpendableTxOutList()
      , { addr.getRecipient((uint64_t)12000) }, 345);
   const auto txData = wallet->SignTXRequest(txReq);
   ASSERT_FALSE(txData.isNull());
   EXPECT_TRUE(wallet->SetTransactionComment(txData, txComment));
   Tx tx(txData);
   EXPECT_TRUE(tx.isInitialized());
   EXPECT_EQ(wallet->GetTransactionComment(tx.getThisHash()), txComment);

   EXPECT_TRUE(hdWallet.eraseFile());
}

TEST(TestWallet, Encryption)
{
   const SecureBinaryData password("test pass");
   const SecureBinaryData wrongPass("wrong pass");
   bs::hd::Node node(NetworkType::TestNet);
   EXPECT_FALSE(node.isEncrypted());

   const auto encrypted = node.encrypt(password);
   ASSERT_NE(encrypted, nullptr);
   EXPECT_TRUE(encrypted->isEncrypted());
   EXPECT_NE(node.privateKey(), encrypted->privateKey());
   EXPECT_EQ(encrypted->derive(bs::hd::Path({2, 3, 4})), nullptr);
   EXPECT_EQ(encrypted->encrypt(password), nullptr);

   const auto decrypted = encrypted->decrypt(password);
   ASSERT_NE(decrypted, nullptr);
   EXPECT_FALSE(decrypted->isEncrypted());
   EXPECT_EQ(node.privateKey(), decrypted->privateKey());
   EXPECT_EQ(decrypted->decrypt(password), nullptr);

   const auto wrongDec = encrypted->decrypt(wrongPass);
   ASSERT_NE(wrongDec, nullptr);
   EXPECT_NE(node.privateKey(), wrongDec->privateKey());
   EXPECT_EQ(node.privateKey().getSize(), wrongDec->privateKey().getSize());

   bs::hd::Wallet wallet("test", "", false, { "test seed", NetworkType::TestNet });
   EXPECT_FALSE(wallet.isWatchingOnly());
   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->isWatchingOnly());
   EXPECT_FALSE(leaf->isEncrypted());

   wallet.changePassword(password);
   EXPECT_TRUE(wallet.isEncrypted());
   EXPECT_TRUE(leaf->isEncrypted());

   auto addr = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   ASSERT_FALSE(addr.isNull());
   wallet.RegisterWallet(PyBlockDataManager::instance());

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (!TestEnv::regtestControl()->SendTo(0.001, addr).isEmpty()) {
      TestEnv::regtestControl()->GenerateBlocks(6);
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      if (TestEnv::blockMonitor()->waitForWalletReady(leaf)) {
         leaf->UpdateBalanceFromDB();
      }
   }

   const auto txReq = leaf->CreateTXRequest(leaf->getSpendableTxOutList()
      , { addr.getRecipient((uint64_t)1200) }, 345);
   EXPECT_THROW(leaf->SignTXRequest(txReq), std::exception);
   const auto txData = leaf->SignTXRequest(txReq, password);
   ASSERT_FALSE(txData.isNull());
}

TEST(TestWallet, ExtOnlyAddresses)
{
   bs::hd::Wallet wallet1("test", "", true, { "test seed", NetworkType::TestNet });
   auto grp1 = wallet1.createGroup(wallet1.getXBTGroupType());
   ASSERT_NE(grp1, nullptr);

   auto leaf1 = grp1->createLeaf(0);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_TRUE(leaf1->hasExtOnlyAddresses());

   const auto addr1 = leaf1->GetNewChangeAddress();
   const auto index1 = leaf1->GetAddressIndex(addr1);
   EXPECT_EQ(index1, "0/0");

   bs::hd::Wallet wallet2("test", "", false, { "test seed", NetworkType::TestNet });
   auto grp2 = wallet2.createGroup(wallet2.getXBTGroupType());
   ASSERT_NE(grp2, nullptr);

   auto leaf2 = grp2->createLeaf(0);
   ASSERT_NE(leaf2, nullptr);
   EXPECT_FALSE(leaf2->hasExtOnlyAddresses());

   const auto addr2 = leaf2->GetNewChangeAddress();
   const auto index2 = leaf2->GetAddressIndex(addr2);
   EXPECT_EQ(index2, "1/0");
   EXPECT_NE(addr1, addr2);
}

TEST(TestWallet, SimpleTX)
{
   bs::hd::Wallet wallet("test", "", false, NetworkType::TestNet);
   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->hasExtOnlyAddresses());

   const auto addr1 = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   const auto changeAddr = leaf->GetNewChangeAddress(AddressEntryType_P2SH);
   wallet.RegisterWallet(PyBlockDataManager::instance());
   EXPECT_EQ(leaf->GetUsedAddressCount(), 3);

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (!TestEnv::regtestControl()->SendTo(0.1, addr1).isEmpty()) {
      curHeight = TestEnv::regtestControl()->GenerateBlocks(6);
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      leaf->UpdateBalanceFromDB();
   }
   EXPECT_DOUBLE_EQ(leaf->GetSpendableBalance(), 0.1);

   const uint64_t amount = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;
   const auto inputs = leaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs.empty());
   const auto recipient = addr2.getRecipient(amount);
   const auto txReq = leaf->CreateTXRequest(inputs, { recipient }, fee, false, changeAddr);
   const auto txSigned = leaf->SignTXRequest(txReq);
   ASSERT_FALSE(txSigned.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned.toHexStr())));

   curHeight = TestEnv::regtestControl()->GenerateBlocks(6);
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   leaf->UpdateBalanceFromDB();
   EXPECT_EQ(leaf->getAddrBalance(addr2)[0], amount);
}

TEST(TestWallet, SimpleTX_bech32)
{
   bs::hd::Wallet wallet("test", "", false, NetworkType::TestNet);
   auto grp = wallet.createGroup(wallet.getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->hasExtOnlyAddresses());

   const auto addr1 = leaf->GetNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->GetNewExtAddress();
   const auto addr3 = leaf->GetNewExtAddress();
   const auto changeAddr = leaf->GetNewChangeAddress();
   wallet.RegisterWallet(PyBlockDataManager::instance());
   EXPECT_EQ(leaf->GetUsedAddressCount(), 4);

   auto curHeight = PyBlockDataManager::instance()->GetTopBlockHeight();
   if (!TestEnv::regtestControl()->SendTo(0.1, addr1).isEmpty()) {
      TestEnv::regtestControl()->GenerateBlocks(6);
      curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      leaf->UpdateBalanceFromDB();
   }
   EXPECT_DOUBLE_EQ(leaf->GetSpendableBalance(), 0.1);

   const uint64_t amount1 = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;
   const auto inputs1 = leaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs1.empty());
   const auto recipient1 = addr2.getRecipient(amount1);
   ASSERT_NE(recipient1, nullptr);
   const auto txReq1 = leaf->CreateTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
   const auto txSigned1 = leaf->SignTXRequest(txReq1);
   ASSERT_FALSE(txSigned1.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr())));

   EXPECT_TRUE(TestEnv::regtestControl()->GenerateBlocks(6));
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
   leaf->UpdateBalanceFromDB();
   EXPECT_EQ(leaf->getAddrBalance(addr2)[0], amount1);

   const uint64_t amount2 = 0.04 * BTCNumericTypes::BalanceDivider;
   const auto inputs2 = leaf->getSpendableTxOutList();
   ASSERT_FALSE(inputs2.empty());
   const auto recipient2 = addr3.getRecipient(amount2);
   ASSERT_NE(recipient2, nullptr);
   const auto txReq2 = leaf->CreateTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
   const auto txSigned2 = leaf->SignTXRequest(txReq2);
   ASSERT_FALSE(txSigned2.isNull());
   ASSERT_TRUE(TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr())));
}

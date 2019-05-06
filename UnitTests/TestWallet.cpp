#include <gtest/gtest.h>
#include <QComboBox>
#include <QDebug>
#include <QString>
#include <QLocale>
#include "ApplicationSettings.h"
#include "CoreHDLeaf.h"
#include "CoreHDWallet.h"
#include "CoreSettlementWallet.h"
#include "CoreWalletsManager.h"
#include "InprocSigner.h"
#include "SettlementAddressEntry.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletEncryption.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


TEST(TestWallet, BIP44_primary)
{
   const auto userId = SecureBinaryData(BinaryData::CreateFromHex("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
   ASSERT_NE(TestEnv::walletsMgr(), nullptr);

   TestEnv::walletsMgr()->createWallet("primary", "test", { "Sample test seed", NetworkType::TestNet }
      , TestEnv::appSettings()->GetHomeDir().toStdString(), true);
   EXPECT_NE(TestEnv::walletsMgr()->getPrimaryWallet(), nullptr);
   TestEnv::walletsMgr()->setChainCode(userId);

   const auto wallet = TestEnv::walletsMgr()->getPrimaryWallet();
   ASSERT_NE(wallet, nullptr);
   EXPECT_EQ(wallet->name(), "primary");
   EXPECT_EQ(wallet->description(), "test");
   EXPECT_EQ(wallet->walletId(), "2xZJsNXfL");
   EXPECT_EQ(wallet->getRootNode({})->getSeed(), BinaryData("Sample test seed"));

   const auto grpAuth = wallet->getGroup(bs::hd::CoinType::BlockSettle_Auth);
   ASSERT_NE(grpAuth, nullptr);

   const auto leafAuth = grpAuth->createLeaf(0);
   ASSERT_NE(leafAuth, nullptr);
   EXPECT_EQ(leafAuth->getRootId().toHexStr(), "02300a2964a3d3c46ab231c1417f87e0fb116127c80fb361dc19e6ab675e3bef85");
   EXPECT_EQ(leafAuth->type(), bs::core::wallet::Type::Authentication);

   const auto grpXbt = wallet->getGroup(wallet->getXBTGroupType());
   ASSERT_NE(grpXbt, nullptr);

   const auto leafXbt = grpXbt->getLeaf(0);
   EXPECT_NE(leafXbt, nullptr);
   EXPECT_EQ(leafXbt->shortName(), "0");
   EXPECT_EQ(leafXbt->name(), "primary/1/0");
   EXPECT_EQ(leafXbt->description(), "test");
   EXPECT_EQ(leafXbt->getRootId().toHexStr(), "0323373b6a7b4f3f8657637342b9254e7c5b4f210d8c28ac871a4e2340ee8cc70f");

   EXPECT_EQ(grpXbt->createLeaf(0), nullptr);

   const auto leaf1 = grpXbt->createLeaf(1);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_EQ(grpXbt->getNumLeaves(), 2);
   EXPECT_EQ(leaf1->shortName(), "1");
   EXPECT_EQ(leaf1->name(), "primary/1/1");
   EXPECT_EQ(leaf1->description(), "test");
   EXPECT_TRUE(TestEnv::walletsMgr()->deleteWalletFile(leaf1));
   EXPECT_EQ(grpXbt->getNumLeaves(), 1);

   const auto grpCC = wallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   const auto leafCC = grpCC->createLeaf("BSP");
   EXPECT_EQ(leafCC->name(), "primary/BS/BSP");

   auto inprocSigner = std::make_shared<InprocSigner>(TestEnv::walletsMgr(), TestEnv::logger(), "", NetworkType::TestNet);
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncXbtLeaf = syncMgr->getWalletById(leafXbt->walletId());
   EXPECT_EQ(syncXbtLeaf->name(), "primary/XBT [TESTNET]/0");

   QComboBox cbox;
   UiUtils::fillWalletsComboBox(&cbox, syncMgr);
   EXPECT_EQ(cbox.count(), 1);
   EXPECT_EQ(cbox.currentText().toStdString(), syncXbtLeaf->name());

   EXPECT_TRUE(TestEnv::walletsMgr()->deleteWalletFile(wallet));
   EXPECT_EQ(TestEnv::walletsMgr()->getPrimaryWallet(), nullptr);
   TestEnv::walletsMgr()->setChainCode({});
}

TEST(TestWallet, BIP44_address)
{
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{ "test seed", NetworkType::TestNet });
   ASSERT_NE(wallet, nullptr);
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);
   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_EQ(leaf->getUsedAddressCount(), 0);

   const auto addr = leaf->getNewExtAddress();
   EXPECT_EQ(addr.display(), "tb1qzy2ml53plruqppz5kmyajeua8lphyq6nju729l");
   EXPECT_EQ(leaf->getUsedAddressCount(), 1);

   const auto chgAddr = leaf->getNewChangeAddress();
   EXPECT_EQ(chgAddr.display(), "tb1qeyn6q6s42hrywnjf03vj8er55nzkdylzlv8mp3");
   EXPECT_EQ(leaf->getUsedAddressCount(), 2);

   EXPECT_TRUE(wallet->eraseFile());
}

TEST(TestWallet, BIP44_derivation)
{
   const bs::core::hd::Node node(NetworkType::TestNet);
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
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", ""
      , bs::core::wallet::Seed{"test seed", NetworkType::TestNet});
   ASSERT_NE(wallet, nullptr);
   EXPECT_FALSE(wallet->isWatchingOnly());
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf1 = grp->createLeaf(0);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_FALSE(leaf1->isWatchingOnly());
   for (size_t i = 0; i < nbAddresses; i++) {
      leaf1->getNewExtAddress();
   }
   EXPECT_EQ(leaf1->getUsedAddressCount(), nbAddresses);

   auto leaf2 = grp->createLeaf(1);
   ASSERT_NE(leaf2, nullptr);
   for (size_t i = 0; i < nbAddresses; i++) {
      leaf2->getNewExtAddress();
   }
   EXPECT_EQ(leaf2->getUsedAddressCount(), nbAddresses);

   auto woWallet = wallet->createWatchingOnly({});
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
   EXPECT_EQ(woLeaf1->getUsedAddressCount(), nbAddresses);
   EXPECT_EQ(woLeaf2->getUsedAddressCount(), nbAddresses);

   const auto addrList = leaf1->getUsedAddressList();
   for (size_t i = 0; i < nbAddresses; i++) {
      const auto index = woLeaf1->getAddressIndex(addrList[i]);
      const auto addr = leaf1->createAddressWithIndex(index, addrList[i].getType());
      EXPECT_EQ(addrList[i].prefixed(), addr.prefixed()) << "addresses at " << index << " are unequal";
   }
   EXPECT_EQ(leaf1->getUsedAddressCount(), nbAddresses);

   EXPECT_TRUE(woWallet->isWatchingOnly());
   EXPECT_NE(woWallet->createGroup(bs::hd::CoinType::BlockSettle_Auth), nullptr);
   EXPECT_EQ(woGroup->createLeaf(2), nullptr);
}

TEST(TestWallet, Auth)
{
   TestEnv::requireAssets();
   ASSERT_NE(TestEnv::authAddrMgr(), nullptr);
   EXPECT_TRUE(TestEnv::authAddrMgr()->IsReady());
   QComboBox cb;
   UiUtils::fillAuthAddressesComboBox(&cb, TestEnv::authAddrMgr());
   const auto verifiedAddresses = TestEnv::authAddrMgr()->GetVerifiedAddressList();
   EXPECT_EQ(cb.count(), verifiedAddresses.size());
   EXPECT_EQ(cb.currentText().toStdString()
      , verifiedAddresses[TestEnv::authAddrMgr()->getDefaultIndex()].display());

   for (const auto &addr : verifiedAddresses) {
      EXPECT_EQ(TestEnv::authAddrMgr()->GetState(addr), AddressVerificationState::Verified);
   }
}

TEST(TestWallet, Settlement)
{
   const std::string filename = "settlement_test_wallet.lmdb";
   constexpr size_t nKeys = 3;
   BinaryData settlementId[nKeys];
   for (size_t i = 0; i < nKeys; i++) {
      settlementId[i] = CryptoPRNG::generateRandom(32);
   }
   BinaryData buyPrivKey[nKeys];
   for (size_t i = 0; i < nKeys; i++) {
      buyPrivKey[i] = CryptoPRNG::generateRandom(32);
   }
   BinaryData sellPrivKey[nKeys];
   for (size_t i = 0; i < nKeys; i++) {
      sellPrivKey[i] = CryptoPRNG::generateRandom(32);
   }
   CryptoECDSA crypto;
   BinaryData buyPubKey[nKeys];
   for (size_t i = 0; i < nKeys; i++) {
      buyPubKey[i] = crypto.CompressPoint(crypto.ComputePublicKey(buyPrivKey[i]));
      ASSERT_EQ(buyPubKey[i].getSize(), 33);
   }
   BinaryData sellPubKey[nKeys];
   for (size_t i = 0; i < nKeys; i++) {
      sellPubKey[i] = crypto.CompressPoint(crypto.ComputePublicKey(sellPrivKey[i]));
      ASSERT_EQ(sellPubKey[i].getSize(), 33);
      ASSERT_NE(buyPubKey[i], sellPubKey[i]);
   }

   std::shared_ptr<bs::core::SettlementAddressEntry> addrEntry1;
   {
      bs::core::SettlementWallet wallet1(NetworkType::TestNet);
      wallet1.saveToFile(filename);
      addrEntry1 = wallet1.newAddress(settlementId[0], buyPubKey[0], sellPubKey[0], "Test comment");
      ASSERT_EQ(wallet1.getUsedAddressCount(), 1);
      EXPECT_EQ(bs::Address(addrEntry1->getPrefixedHash()), wallet1.getUsedAddressList()[0]);
      EXPECT_EQ(wallet1.getAddressComment(addrEntry1->getPrefixedHash()), "Test comment");
   }

   {
      bs::core::SettlementWallet wallet2(NetworkType::TestNet, filename);
      ASSERT_EQ(wallet2.getUsedAddressCount(), 1);
      EXPECT_EQ(bs::Address(addrEntry1->getPrefixedHash()), wallet2.getUsedAddressList()[0]);
      EXPECT_EQ(wallet2.getAddressComment(addrEntry1->getPrefixedHash()), "Test comment");
      wallet2.newAddress(settlementId[1], buyPubKey[1], sellPubKey[1]);
      EXPECT_EQ(wallet2.getUsedAddressCount(), 2);
   }

   {
      bs::core::SettlementWallet wallet3(NetworkType::TestNet, filename);
      EXPECT_EQ(wallet3.getUsedAddressCount(), 2);
      EXPECT_EQ(bs::Address(addrEntry1->getPrefixedHash()), wallet3.getUsedAddressList()[0]);
      wallet3.newAddress(settlementId[2], buyPubKey[2], sellPubKey[2]);
      EXPECT_EQ(wallet3.getUsedAddressCount(), 3);
   }

   {
      bs::core::SettlementWallet wallet4(NetworkType::TestNet, filename);
      EXPECT_EQ(wallet4.getUsedAddressCount(), 3);
      EXPECT_TRUE(wallet4.eraseFile());
   }
}

TEST(TestWallet, Comments)
{
   TestEnv::requireArmory();
   const std::string addrComment("Test address comment");
   const std::string txComment("Test TX comment");

   auto hdWallet = std::make_shared< bs::core::hd::Wallet>("hdWallet", "", NetworkType::TestNet);
   hdWallet->createStructure();
   const auto group = hdWallet->getGroup(hdWallet->getXBTGroupType());
   ASSERT_NE(group, nullptr);
   const auto wallet = group->getLeaf(0);
   ASSERT_NE(wallet, nullptr);

   auto addr = wallet->getNewExtAddress(AddressEntryType_P2SH);
   ASSERT_FALSE(addr.isNull());

   auto inprocSigner = std::make_shared<InprocSigner>(hdWallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncHdWallet = syncMgr->getHDWalletById(hdWallet->walletId());
   auto syncWallet = syncMgr->getWalletById(wallet->walletId());
   syncHdWallet->registerWallet(TestEnv::armory());

   EXPECT_TRUE(syncWallet->setAddressComment(addr, addrComment));
   EXPECT_EQ(wallet->getAddressComment(addr), addrComment);

   const auto &cbSend = [syncWallet](QString result) {
      const auto &curHeight = TestEnv::armory()->topBlock();
//      TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      if (TestEnv::blockMonitor()->waitForWalletReady(syncWallet)) {
         syncWallet->updateBalances();
      }
   };
//   TestEnv::regtestControl()->SendTo(0.01, addr, cbSend);

   const auto &cbTxOutList = [wallet, syncWallet, addr, txComment](std::vector<UTXO> inputs) {
      const auto txReq = syncWallet->createTXRequest(inputs
         , { addr.getRecipient((uint64_t)12000) }, 345);
      const auto txData = wallet->signTXRequest(txReq);
      ASSERT_FALSE(txData.isNull());
      EXPECT_TRUE(syncWallet->setTransactionComment(txData, txComment));
      Tx tx(txData);
      EXPECT_TRUE(tx.isInitialized());
      EXPECT_EQ(wallet->getTransactionComment(tx.getThisHash()), txComment);
   };
   syncWallet->getSpendableTxOutList(cbTxOutList, nullptr);
}

TEST(TestWallet, Encryption)
{
   TestEnv::requireArmory();
   const SecureBinaryData password("test pass");
   const SecureBinaryData wrongPass("wrong pass");
   bs::core::hd::Node node(NetworkType::TestNet);
   EXPECT_TRUE(node.encTypes().empty());

   const auto encrypted = node.encrypt(password);
   ASSERT_NE(encrypted, nullptr);
   ASSERT_FALSE(encrypted->encTypes().empty());
   EXPECT_TRUE(encrypted->encTypes()[0] == bs::wallet::EncryptionType::Password);
   EXPECT_NE(node.privateKey(), encrypted->privateKey());
   EXPECT_EQ(encrypted->derive(bs::hd::Path({2, 3, 4})), nullptr);
   EXPECT_EQ(encrypted->encrypt(password), nullptr);

   const auto decrypted = encrypted->decrypt(password);
   ASSERT_NE(decrypted, nullptr);
   EXPECT_TRUE(decrypted->encTypes().empty());
   EXPECT_EQ(node.privateKey(), decrypted->privateKey());
   EXPECT_EQ(decrypted->decrypt(password), nullptr);

   const auto wrongDec = encrypted->decrypt(wrongPass);
   ASSERT_NE(wrongDec, nullptr);
   EXPECT_NE(node.privateKey(), wrongDec->privateKey());
   EXPECT_EQ(node.privateKey().getSize(), wrongDec->privateKey().getSize());

   bs::core::wallet::Seed seed{ "test seed", NetworkType::TestNet };
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", "", seed);
   EXPECT_FALSE(wallet->isWatchingOnly());
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->isWatchingOnly());
   EXPECT_TRUE(leaf->encryptionTypes().empty());

   const bs::wallet::PasswordData pwdData = {password, bs::wallet::EncryptionType::Password, {}};
   wallet->changePassword({ pwdData }, { 1, 1 }, {}, false, false, false);
   EXPECT_TRUE(wallet->encryptionTypes()[0] == bs::wallet::EncryptionType::Password);
   EXPECT_TRUE(leaf->encryptionTypes()[0] == bs::wallet::EncryptionType::Password);

   auto addr = leaf->getNewExtAddress(AddressEntryType_P2SH);
   ASSERT_FALSE(addr.isNull());

   auto inprocSigner = std::make_shared<InprocSigner>(wallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(wallet->walletId());
   auto syncLeaf = syncMgr->getWalletById(leaf->walletId());
   syncWallet->registerWallet(TestEnv::armory());

   const auto &cbSend = [syncLeaf](QString result) {
      const auto curHeight = TestEnv::armory()->topBlock();
//      TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      if (TestEnv::blockMonitor()->waitForWalletReady(syncLeaf)) {
         syncLeaf->updateBalances();
      }
   };
//   TestEnv::regtestControl()->SendTo(0.001, addr, cbSend);

   const auto &cbTxOutList = [leaf, syncLeaf, addr, password](std::vector<UTXO> inputs) {
      const auto txReq = syncLeaf->createTXRequest(inputs
         , { addr.getRecipient((uint64_t)1200) }, 345);
      EXPECT_THROW(leaf->signTXRequest(txReq), std::exception);
      const auto txData = leaf->signTXRequest(txReq, password);
      ASSERT_FALSE(txData.isNull());
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList, nullptr);
}

TEST(TestWallet, ExtOnlyAddresses)
{
   const bs::core::wallet::Seed seed{ "test seed", NetworkType::TestNet };
   bs::core::hd::Wallet wallet1("test", "", seed, TestEnv::logger(), true);
   auto grp1 = wallet1.createGroup(wallet1.getXBTGroupType());
   ASSERT_NE(grp1, nullptr);

   auto leaf1 = grp1->createLeaf(0);
   ASSERT_NE(leaf1, nullptr);
   EXPECT_TRUE(leaf1->hasExtOnlyAddresses());

   const auto addr1 = leaf1->getNewChangeAddress();
   const auto index1 = leaf1->getAddressIndex(addr1);
   EXPECT_EQ(index1, "0/0");

   bs::core::hd::Wallet wallet2("test", "", seed);
   auto grp2 = wallet2.createGroup(wallet2.getXBTGroupType());
   ASSERT_NE(grp2, nullptr);

   auto leaf2 = grp2->createLeaf(0);
   ASSERT_NE(leaf2, nullptr);
   EXPECT_FALSE(leaf2->hasExtOnlyAddresses());

   const auto addr2 = leaf2->getNewChangeAddress();
   const auto index2 = leaf2->getAddressIndex(addr2);
   EXPECT_EQ(index2, "1/0");
   EXPECT_NE(addr1, addr2);
}

TEST(TestWallet, 1of2_SameKey)
{
   bs::core::hd::Wallet wallet1("hdWallet", "", NetworkType::TestNet, TestEnv::logger());
   const std::string email = "email@example.com";
   const std::vector<bs::wallet::PasswordData> authKeys = {
      { CryptoPRNG::generateRandom(32), bs::wallet::EncryptionType::Auth, email },
      { CryptoPRNG::generateRandom(32), bs::wallet::EncryptionType::Auth, email }
   };
   const bs::wallet::KeyRank keyRank = { 1, 2 };
   EXPECT_EQ(wallet1.changePassword(authKeys, keyRank, {}, false, false, false), true);
   ASSERT_EQ(wallet1.encryptionTypes().size(), 1);
   EXPECT_EQ(wallet1.encryptionTypes()[0], bs::wallet::EncryptionType::Auth);
   ASSERT_EQ(wallet1.encryptionKeys().size(), 1);
   EXPECT_EQ(wallet1.encryptionKeys()[0], email);
   EXPECT_EQ(wallet1.encryptionRank(), keyRank);
   EXPECT_NE(wallet1.getRootNode(authKeys[0].password), nullptr);
   EXPECT_NE(wallet1.getRootNode(authKeys[1].password), nullptr);

   const std::string filename = "m_of_n_test.lmdb";
   wallet1.saveToFile(filename);
   {
      bs::core::hd::Wallet wallet2(filename);
      ASSERT_EQ(wallet2.encryptionTypes().size(), 1);
      EXPECT_EQ(wallet2.encryptionTypes()[0], bs::wallet::EncryptionType::Auth);
      ASSERT_EQ(wallet2.encryptionKeys().size(), 1);
      EXPECT_EQ(wallet2.encryptionKeys()[0], email);
      EXPECT_EQ(wallet2.encryptionRank(), keyRank);
      EXPECT_NE(wallet2.getRootNode(authKeys[0].password), nullptr);
      EXPECT_NE(wallet2.getRootNode(authKeys[1].password), nullptr);
   }
   EXPECT_TRUE(wallet1.eraseFile());
}

TEST(TestWallet, SimpleTX)
{
   TestEnv::requireArmory();
   auto wallet = std::make_shared< bs::core::hd::Wallet>("test", "", NetworkType::TestNet);
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->hasExtOnlyAddresses());

   const auto addr1 = leaf->getNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->getNewExtAddress(AddressEntryType_P2SH);
   const auto changeAddr = leaf->getNewChangeAddress(AddressEntryType_P2SH);
   EXPECT_EQ(leaf->getUsedAddressCount(), 3);

   auto inprocSigner = std::make_shared<InprocSigner>(wallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(wallet->walletId());
   auto syncLeaf = syncMgr->getWalletById(leaf->walletId());

   syncWallet->registerWallet(TestEnv::armory());

   const auto &cbSend = [syncLeaf](QString result) {
      const auto curHeight = TestEnv::armory()->topBlock();
//      TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);
      syncLeaf->updateBalances();
      EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 0.1);
   };
//   TestEnv::regtestControl()->SendTo(0.1, addr1, cbSend);

   const uint64_t amount = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   const auto &cbTxOutList = [leaf, syncLeaf, changeAddr, addr2, amount, fee](std::vector<UTXO> inputs) {
      const auto recipient = addr2.getRecipient(amount);
      const auto txReq = syncLeaf->createTXRequest(inputs, { recipient }, fee, false, changeAddr);
      const auto txSigned = leaf->signTXRequest(txReq);
      ASSERT_FALSE(txSigned.isNull());

      const auto &cbTx = [](bool result) {
         ASSERT_TRUE(result);
      };
//      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned.toHexStr()), cbTx);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList, nullptr);

   const auto curHeight = TestEnv::armory()->topBlock();
//   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);

   const auto &cbBalance = [syncLeaf, amount, addr2](std::vector<uint64_t>) {
      const auto &cbAddrBal = [amount](std::vector<uint64_t> balances) {
         EXPECT_EQ(balances[0], amount);
      };
      syncLeaf->getAddrBalance(addr2, cbAddrBal);
   };
   syncLeaf->updateBalances(cbBalance);
}

TEST(TestWallet, SimpleTX_bech32)
{
   TestEnv::requireArmory();
   auto wallet = std::make_shared<bs::core::hd::Wallet>("test", "", NetworkType::TestNet);
   auto grp = wallet->createGroup(wallet->getXBTGroupType());
   ASSERT_NE(grp, nullptr);

   auto leaf = grp->createLeaf(0);
   ASSERT_NE(leaf, nullptr);
   EXPECT_FALSE(leaf->hasExtOnlyAddresses());

   const auto addr1 = leaf->getNewExtAddress(AddressEntryType_P2SH);
   const auto addr2 = leaf->getNewExtAddress();
   const auto addr3 = leaf->getNewExtAddress();
   const auto changeAddr = leaf->getNewChangeAddress();
   EXPECT_EQ(leaf->getUsedAddressCount(), 4);

   auto inprocSigner = std::make_shared<InprocSigner>(wallet, TestEnv::logger());
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(TestEnv::logger()
      , TestEnv::appSettings(), TestEnv::armory());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   auto syncWallet = syncMgr->getHDWalletById(wallet->walletId());
   auto syncLeaf = syncMgr->getWalletById(leaf->walletId());

   syncWallet->registerWallet(TestEnv::armory());

   const auto &cbSend = [syncLeaf](QString result) {
      const auto curHeight = TestEnv::armory()->topBlock();
//      TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
      TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);

      const auto &cbBalance = [syncLeaf](std::vector<uint64_t>) {
         EXPECT_DOUBLE_EQ(syncLeaf->getSpendableBalance(), 0.1);
      };
      syncLeaf->updateBalances(cbBalance);
   };
//   TestEnv::regtestControl()->SendTo(0.1, addr1, cbSend);

   const uint64_t amount1 = 0.05 * BTCNumericTypes::BalanceDivider;
   const uint64_t fee = 0.0001 * BTCNumericTypes::BalanceDivider;

   const auto &cbTX = [](bool result) {
      ASSERT_TRUE(result);
   };

   const auto &cbTxOutList1 = [leaf, syncLeaf, addr2, changeAddr, amount1, fee, cbTX](std::vector<UTXO> inputs1) {
      const auto recipient1 = addr2.getRecipient(amount1);
      ASSERT_NE(recipient1, nullptr);
      const auto txReq1 = syncLeaf->createTXRequest(inputs1, { recipient1 }, fee, false, changeAddr);
      const auto txSigned1 = leaf->signTXRequest(txReq1);
      ASSERT_FALSE(txSigned1.isNull());

//      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned1.toHexStr()), cbTX);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList1, nullptr);

   auto curHeight = TestEnv::armory()->topBlock();
//   TestEnv::regtestControl()->GenerateBlocks(6, [](bool) {});
   curHeight = TestEnv::blockMonitor()->waitForNewBlocks(curHeight + 6);

   const auto &cbBalance = [syncLeaf, addr2, amount1](std::vector<uint64_t>) {
      const auto &cbAddrBal = [amount1](std::vector<uint64_t> balances) {
         EXPECT_EQ(balances[0], amount1);
      };
      syncLeaf->getAddrBalance(addr2, cbAddrBal);
   };
   syncLeaf->updateBalances(cbBalance);

   const auto &cbTxOutList2 = [leaf, syncLeaf, addr3, fee, changeAddr, cbTX](std::vector<UTXO> inputs2) {
      const uint64_t amount2 = 0.04 * BTCNumericTypes::BalanceDivider;
      const auto recipient2 = addr3.getRecipient(amount2);
      ASSERT_NE(recipient2, nullptr);
      const auto txReq2 = syncLeaf->createTXRequest(inputs2, { recipient2 }, fee, false, changeAddr);
      const auto txSigned2 = leaf->signTXRequest(txReq2);
      ASSERT_FALSE(txSigned2.isNull());
//      TestEnv::regtestControl()->SendTx(QString::fromStdString(txSigned2.toHexStr()), cbTX);
   };
   syncLeaf->getSpendableTxOutList(cbTxOutList2, nullptr);
}

TEST(TestWallet, ImportExport)
{
   const std::string authLeaf = "Auth";
   const bs::core::wallet::Seed seed{ NetworkType::TestNet };
   auto wallet1 = std::make_shared<bs::core::hd::Wallet>("test1", "", seed, nullptr, true);
   auto grp1 = wallet1->createGroup(wallet1->getXBTGroupType());
   auto leaf0 = grp1->createLeaf(0u);
   leaf0->getNewExtAddress();

   const auto &rootNode1 = wallet1->getRootNode({});
   ASSERT_NE(rootNode1, nullptr);
   auto leaf1 = grp1->createLeaf(authLeaf, rootNode1);
   ASSERT_NE(leaf1, nullptr);
   const auto addr1 = leaf1->getNewExtAddress();

   const auto seed1 = rootNode1->seed();
   const auto easyPrivKey = seed1.toEasyCodeChecksum();
   ASSERT_FALSE(easyPrivKey.part1.empty());
   ASSERT_FALSE(easyPrivKey.part2.empty());

   const auto seed2 = bs::core::wallet::Seed::fromEasyCodeChecksum(easyPrivKey, NetworkType::TestNet);
   auto wallet2 = std::make_shared<bs::core::hd::Wallet>("test2", "", seed2, nullptr, true);
   auto grp2 = wallet2->createGroup(wallet2->getXBTGroupType());

   auto leaf2 = grp2->createLeaf(authLeaf);
   ASSERT_NE(leaf2, nullptr);
   const auto addr2 = leaf2->getNewExtAddress();

   EXPECT_EQ(leaf1->walletId(), leaf2->walletId());
   EXPECT_EQ(addr1, addr2);
}

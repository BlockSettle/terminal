#include <gtest/gtest.h>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QLocale>
#include <QString>
#include <spdlog/spdlog.h>
#include "Address.h"
#include "AssetManager.h"
#include "CacheFile.h"
#include "CurrencyPair.h"
#include "EasyCoDec.h"
#include "MarketDataProvider.h"
#include "OTPFile.h"
#include "TestEnv.h"


TEST(TestCommon, CurrencyPair)
{
   CurrencyPair cp1("USD/EUR");
   EXPECT_EQ(cp1.NumCurrency(), "USD");
   EXPECT_EQ(cp1.DenomCurrency(), "EUR");
   EXPECT_EQ(cp1.ContraCurrency("USD"), "EUR");
   EXPECT_EQ(cp1.ContraCurrency("GBP"), "");

   EXPECT_THROW(CurrencyPair("USD"), std::exception);

   CurrencyPair cp2("USD/EUR/JPY");
   EXPECT_EQ(cp2.NumCurrency(), "USD");
   EXPECT_EQ(cp2.DenomCurrency(), "EUR/JPY");
   EXPECT_EQ(cp2.ContraCurrency("EUR"), "");
}

TEST(TestCommon, Address)
{
   const std::string b58Addr = "2NBoXxTwt1ruSkuCv5iJaSmZUccHNB2yPjB";
   const bs::Address addr(b58Addr);
   ASSERT_FALSE(addr.isNull());
   EXPECT_EQ(addr.getSize(), 21);
   EXPECT_TRUE(addr.isValid());
   EXPECT_EQ(addr.display<std::string>(bs::Address::Format::Auto), b58Addr);
   EXPECT_EQ(addr.getType(), AddressEntryType_P2SH);
   EXPECT_EQ(addr.display<std::string>(bs::Address::Bech32), "tb1qew8fxz85q4fvz2vx422qd8j3zj7w83t606230m");
   EXPECT_EQ(addr.display<std::string>(bs::Address::Hex), "c4cb8e9308f40552c12986aa94069e5114bce3c57a");

   const std::string bech32Addr = "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx";
   const auto pubKey = BinaryData::CreateFromHex("0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
   bs::Address bAddr1(BtcUtils::getHash160(pubKey), AddressEntryType_P2WPKH);
   EXPECT_EQ(bAddr1.display<std::string>(), bech32Addr);
   EXPECT_EQ(bAddr1.getSize(), 20);
   const bs::Address bAddr2(bech32Addr);
   EXPECT_EQ(bAddr2.getType(), AddressEntryType_P2WPKH);
   EXPECT_EQ(bAddr2, bAddr1);
   EXPECT_EQ(bAddr2.unprefixed(), BtcUtils::getHash160(pubKey));
   EXPECT_EQ(bAddr2.display<std::string>(), bech32Addr);
}

TEST(TestCommon, CacheFile)
{
   const auto filename = "test_tx_cache";
   auto txCF = new TxCacheFile(filename);
   ASSERT_NE(txCF, nullptr);
   const auto tx = txCF->get(BinaryData("non-existent key"));
   EXPECT_FALSE(tx.isInitialized());

   delete txCF;
   QFile txCFile(QString::fromLatin1(filename));
   EXPECT_TRUE(txCFile.exists());
   EXPECT_TRUE(txCFile.remove());
   QFile txCLock(QString::fromLatin1(filename) + QLatin1String("-lock"));
   EXPECT_TRUE(txCLock.exists());
   EXPECT_TRUE(txCLock.remove());
}

TEST(TestCommon, OTPFile)
{
   const SecureBinaryData password = BinaryData("password");
   const SecureBinaryData wrongPass = BinaryData("wrong_password");
   const SecureBinaryData newPass = BinaryData("new_password");
   const auto privKey = BinaryData::CreateFromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

   const auto otpFile = OTPFile::CreateFromPrivateKey(TestEnv::logger(), QString(), privKey, password);
   ASSERT_NE(otpFile, nullptr);
   EXPECT_TRUE(otpFile->IsEncrypted());
   EXPECT_TRUE(otpFile->GetCurrentPrivateKey().isNull());
   EXPECT_TRUE(otpFile->GetCurrentPrivateKey(wrongPass).isNull());
   EXPECT_EQ(otpFile->GetOtpId(), "034646ae5047316b4230d0086c8acec687f00b1cd9d1dc634f6cb358ac0a9a8fff");
   EXPECT_EQ(otpFile->GetShortId().toStdString(), "034646ae");
   EXPECT_EQ(otpFile->GetChainCode().toHexStr(), "aa967eb6f4a0eaac7ac23bafa2c385e72037163605c8eeea99b735aaebfa9b02");
   EXPECT_EQ(otpFile->GetCurrentPrivateKey(password).toHexStr(), "15b087085e45eca3023714bdee76d23772936ec207ecaf2181bdee8b4cff70f6");
   EXPECT_EQ(otpFile->GetUsedKeysCount(), 0);

   EXPECT_TRUE(otpFile->AdvanceKeysTo(1, wrongPass).isNull());
   EXPECT_FALSE(otpFile->AdvanceKeysTo(1, password).isNull());
   EXPECT_EQ(otpFile->GetCurrentPrivateKey(password).toHexStr(), "4510e7805516f1caf2b3e8f664db2242fc9e57faa1a1e214033fc022af414c8a");
   EXPECT_EQ(otpFile->GetUsedKeysCount(), 1);

   EXPECT_FALSE(otpFile->UpdateCurrentPrivateKey(wrongPass, newPass));
   EXPECT_TRUE(otpFile->UpdateCurrentPrivateKey(password, newPass));
   EXPECT_TRUE(otpFile->GetCurrentPrivateKey(password).isNull());
   EXPECT_EQ(otpFile->GetCurrentPrivateKey(newPass).toHexStr(), "4510e7805516f1caf2b3e8f664db2242fc9e57faa1a1e214033fc022af414c8a");
   EXPECT_EQ(otpFile->GetUsedKeysCount(), 1);

   auto data = SecureBinaryData().GenerateRandom(256);
   const auto sigPair = otpFile->Sign(data, newPass);
   const auto signature = sigPair.first;
   const auto keyIndex = sigPair.second;
   ASSERT_FALSE(signature.isNull());
   EXPECT_EQ(keyIndex, 1);

   auto otpCheck = OTPFile::CreateFromPrivateKey(TestEnv::logger(), QString(), privKey, password);
   EXPECT_TRUE(otpCheck->CheckSignature(data, signature, keyIndex, password));
   EXPECT_EQ(otpCheck->GetUsedKeysCount(), 2);
   otpCheck = OTPFile::CreateFromPrivateKey(TestEnv::logger(), QString(), privKey, password);
   EXPECT_FALSE(otpCheck->CheckSignature(data, signature, keyIndex, wrongPass));
   EXPECT_EQ(otpCheck->GetUsedKeysCount(), 0);
   uint8_t *ptr = data.getPtr();
   memset(ptr + 23, 0, 8);
   data = { ptr, data.getSize() };
   EXPECT_EQ(data.getSize(), 256);
   otpCheck = OTPFile::CreateFromPrivateKey(TestEnv::logger(), QString(), privKey, password);
   EXPECT_FALSE(otpCheck->CheckSignature(data, signature, keyIndex, password));
   EXPECT_EQ(otpCheck->GetUsedKeysCount(), 2);
}

TEST(TestCommon, AssetManager)
{
   auto mdProvider = TestEnv::mdProvider();
   AssetManager assetMgr(TestEnv::logger(), TestEnv::walletsMgr(), mdProvider, TestEnv::celerConnection());
   assetMgr.connect(mdProvider.get(), &MarketDataProvider::MDSecurityReceived, &assetMgr, &AssetManager::onMDSecurityReceived);
   assetMgr.connect(mdProvider.get(), &MarketDataProvider::MDSecuritiesReceived, &assetMgr, &AssetManager::onMDSecuritiesReceived);

   const bs::network::SecurityDef secDef[2] = {
      { bs::network::Asset::SpotFX},
      { bs::network::Asset::SpotXBT}
   };
   emit mdProvider->MDSecurityReceived("EUR/USD", secDef[0]);
   emit mdProvider->MDSecurityReceived("GBP/SEK", secDef[0]);
   emit mdProvider->MDSecurityReceived("XBT/USD", secDef[1]);
   emit mdProvider->MDSecuritiesReceived();
   QApplication::processEvents();
   assetMgr.onAccountBalanceLoaded("EUR", 1234.5);
   assetMgr.onAccountBalanceLoaded("USD", 2345.67);
   assetMgr.onAccountBalanceLoaded("GBP", 345);

   EXPECT_EQ(assetMgr.currencies().size(), 3);
   EXPECT_TRUE(assetMgr.hasSecurities());
   EXPECT_EQ(assetMgr.securities().size(), 3);
   EXPECT_EQ(assetMgr.getBalance("USD"), 2345.67);

   assetMgr.onMDUpdate(bs::network::Asset::SpotXBT, QLatin1String("XBT/USD")
      , { bs::network::MDField{bs::network::MDField::PriceLast, 4321} });
   EXPECT_EQ(assetMgr.getPrice("USD"), 1 / 4321.0);
   EXPECT_DOUBLE_EQ(assetMgr.getCashTotal(), 2345.67 / 4321);

   assetMgr.onMDUpdate(bs::network::Asset::PrivateMarket, QLatin1String("BLK/XBT")
      , { bs::network::MDField{bs::network::MDField::PriceLast, 0.023} });
   EXPECT_EQ(assetMgr.getPrice("BLK"), 0.023);
}

TEST(TestCommon, UtxoReservation)
{
   std::vector<UTXO> utxos, utxos1, utxos2, filtered;
   const size_t nbUtxos = 100;
   utxos.reserve(nbUtxos);
   for (int i = 1; i <= nbUtxos; i++) {
      utxos.emplace_back(UTXO(i * 1000, UINT32_MAX, 0, 0, SecureBinaryData().GenerateRandom(32), SecureBinaryData().GenerateRandom(23)));
   }

   for (const auto i : { 1, 3, 5 }) {
      utxos1.push_back(utxos[i]);
   }
   for (const auto i : { 2, 3, 7 }) {
      utxos2.push_back(utxos[i]);
   }
   ASSERT_EQ(utxos.size(), nbUtxos);
   ASSERT_EQ(utxos1.size(), 3);
   ASSERT_EQ(utxos2.size(), 3);

   filtered = utxos;
   bs::UtxoReservation ur;
   ur.reserve("wallet", "id1", utxos1);
   ur.reserve("wallet", "id2", utxos2);
   EXPECT_FALSE(ur.filter("undef-wallet", filtered));
   EXPECT_EQ(filtered.size(), nbUtxos);

   EXPECT_TRUE(ur.filter("wallet", filtered));
   EXPECT_EQ(filtered.size(), nbUtxos - 5);
   EXPECT_EQ(ur.unreserve("id3"), std::string());

   EXPECT_EQ(ur.unreserve("id1"), "wallet");
   filtered = utxos;
   EXPECT_TRUE(ur.filter("wallet", filtered));
   EXPECT_EQ(filtered.size(), nbUtxos - 3);

   EXPECT_EQ(ur.unreserve("id2"), "wallet");
   filtered = utxos;
   EXPECT_FALSE(ur.filter("wallet", filtered));
   EXPECT_EQ(filtered.size(), nbUtxos);

   bs::UtxoReservation::init();
   const auto adapter = std::make_shared<bs::UtxoReservation::Adapter>();
   ASSERT_NE(adapter, nullptr);
   EXPECT_TRUE(bs::UtxoReservation::addAdapter(adapter));
   EXPECT_TRUE(bs::UtxoReservation::delAdapter(adapter));
   EXPECT_FALSE(bs::UtxoReservation::delAdapter(adapter));
   bs::UtxoReservation::destroy();
}

TEST(TestCommon, EasyCoDec)
{
   EasyCoDec codec;
   const std::string hexStr = "0123456789ABCDEF0123456789abcdeffedcba9876543210FEDCBA9876543210";
   const auto data = codec.fromHex(hexStr);
   EXPECT_EQ(data.part1, "asdf ghjk wert uion asdf ghjk wert uion");
   EXPECT_EQ(data.part2, "noiu trew kjhg fdsa noiu trew kjhg fdsa");

   const auto str = codec.toHex(data);
   EXPECT_EQ(BinaryData::CreateFromHex(str), BinaryData::CreateFromHex(hexStr));

   std::unordered_set<char> allowedChars = {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'w', 'e', 'r', 't', 'u', 'i', 'o', 'n'};
   EXPECT_EQ(codec.allowedChars(), allowedChars);

   const auto smallData = codec.fromHex("0123456789abcdef");
   EXPECT_EQ(smallData.part1.length(), smallData.part2.length());
   EXPECT_FALSE(smallData.part1.empty());
   EXPECT_THROW(codec.fromHex("0123456789abcdefghijklmnopabcdeffedcba9876543210fedcba9876543210"), std::invalid_argument);
   EasyCoDec::Data data1 = { "asd", "asdf ghjk wert uion asdf ghjk wert uion" };
   EXPECT_THROW(codec.toHex(data1), std::invalid_argument);
   EXPECT_THROW(codec.toHex(EasyCoDec::Data{}), std::invalid_argument);
   EasyCoDec::Data data2 = { "abce ghjk wert uion asdf ghjk wert uion", "vxyz trew kjhg fdsa noiu trew kjhg fdsa" };
   EXPECT_THROW(codec.toHex(data2), std::invalid_argument);
   EasyCoDec::Data data3 = { "asdf ghjk w  t uion asdf gdsk wert uion", "n  u trew kjhg fdsa noiu trew kjhg f  a" };
   EXPECT_THROW(codec.toHex(data3), std::invalid_argument);
}

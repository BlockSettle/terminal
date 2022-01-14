/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <gtest/gtest.h>
#include <botan/auto_rng.h>
#include <botan/ecdsa.h>
#include <botan/ecies.h>
#include <botan/serpent.h>
#include <botan/ec_group.h>
#include <botan/pubkey.h>
#include <botan/hex.h>
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
#include "HeadlessContainer.h"
#include "InprocSigner.h"
#include "MDCallbacksQt.h"
#include "MarketDataProvider.h"
#include "PriceAmount.h"
#include "TestEnv.h"
#include "WalletUtils.h"
#include "Wallets/SyncWalletsManager.h"


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
   const auto addr = bs::Address::fromAddressString(b58Addr);
   ASSERT_FALSE(addr.empty());
   EXPECT_EQ(addr.getSize(), 21);
   EXPECT_TRUE(addr.isValid());
   EXPECT_EQ(addr.display(), b58Addr);
   EXPECT_EQ(addr.getType(), AddressEntryType_P2SH);

   const std::string bech32Addr = "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx";
   const auto pubKey = BinaryData::CreateFromHex("0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
   auto bAddr1 = bs::Address::fromPubKey(pubKey, AddressEntryType_P2WPKH);
   EXPECT_EQ(bAddr1.display(), bech32Addr);
   EXPECT_EQ(bAddr1.getSize(), 21);
   const auto bAddr2 = bs::Address::fromAddressString(bech32Addr);
   EXPECT_EQ(bAddr2.getType(), AddressEntryType_P2WPKH);
   EXPECT_EQ(bAddr2, bAddr1);
   EXPECT_EQ(bAddr2.unprefixed(), BtcUtils::getHash160(pubKey));
   EXPECT_EQ(bAddr2.display(), bech32Addr);
}

TEST(TestCommon, CacheFile)
{
   const auto filename = "test_tx_cache";
   auto txCF = new TxCacheFile(filename);
   ASSERT_NE(txCF, nullptr);
   const auto tx = txCF->get(BinaryData::fromString("non-existent key"));
   EXPECT_EQ(tx, nullptr);

   delete txCF;
   QFile txCFile(QString::fromLatin1(filename));
   EXPECT_TRUE(txCFile.exists());
   EXPECT_TRUE(txCFile.remove());
   QFile txCLock(QString::fromLatin1(filename) + QLatin1String("-lock"));
   EXPECT_TRUE(txCLock.exists());
   EXPECT_TRUE(txCLock.remove());
}

TEST(TestCommon, AssetManager)
{
   TestEnv env(StaticLogger::loggerPtr);
   env.requireAssets();

   auto hct = new QtHCT(nullptr);
   auto inprocSigner = std::make_shared<InprocSigner>(env.walletsMgr()
      , StaticLogger::loggerPtr, hct, "", NetworkType::TestNet);
   inprocSigner->Start();
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(StaticLogger::loggerPtr
      , env.appSettings(), env.armoryConnection());
   syncMgr->setSignContainer(inprocSigner);
   syncMgr->syncWallets();

   const auto &mdCallbacks = env.mdCallbacks();
   AssetManager assetMgr(StaticLogger::loggerPtr, nullptr); //FIXME: pass AssetMgrCT as a second arg, if needed
   assetMgr.connect(mdCallbacks.get(), &MDCallbacksQt::MDSecurityReceived, &assetMgr, &AssetManager::onMDSecurityReceived);
   assetMgr.connect(mdCallbacks.get(), &MDCallbacksQt::MDSecuritiesReceived, &assetMgr, &AssetManager::onMDSecuritiesReceived);

   const bs::network::SecurityDef secDef[2] = {
      { bs::network::Asset::SpotFX},
      { bs::network::Asset::SpotXBT}
   };
   emit mdCallbacks->MDSecurityReceived("EUR/USD", secDef[0]);
   emit mdCallbacks->MDSecurityReceived("GBP/SEK", secDef[0]);
   emit mdCallbacks->MDSecurityReceived("XBT/USD", secDef[1]);
   emit mdCallbacks->MDSecuritiesReceived();
   QApplication::processEvents();
   assetMgr.onAccountBalanceLoaded("EUR", 1234.5);
   assetMgr.onAccountBalanceLoaded("USD", 2345.67);
   assetMgr.onAccountBalanceLoaded("GBP", 345);

   EXPECT_EQ(assetMgr.currencies().size(), 3);
   EXPECT_TRUE(assetMgr.hasSecurities());
   EXPECT_EQ(assetMgr.securities().size(), 3);
   EXPECT_EQ(assetMgr.getBalance("USD", false, nullptr), 2345.67);

   assetMgr.onMDUpdate(bs::network::Asset::SpotXBT, QLatin1String("XBT/USD")
      , { bs::network::MDField{bs::network::MDField::PriceLast, 4321} });
   EXPECT_EQ(assetMgr.getPrice("USD"), 1 / 4321.0);
   EXPECT_DOUBLE_EQ(assetMgr.getCashTotal(), 2345.67 / 4321);

   assetMgr.onMDUpdate(bs::network::Asset::PrivateMarket, QLatin1String("BLK/XBT")
      , { bs::network::MDField{bs::network::MDField::PriceLast, 0.023} });
   EXPECT_EQ(assetMgr.getPrice("BLK"), 0.023);
   delete hct;
}

TEST(TestCommon, UtxoReservation)
{
   std::vector<UTXO> utxos, utxos1, utxos2, filtered, tmp;
   const size_t nbUtxos = 100;
   utxos.reserve(nbUtxos);
   for (int i = 1; i <= nbUtxos; i++) {
      utxos.emplace_back(UTXO(i * 1000, UINT32_MAX, 0, 0, CryptoPRNG::generateRandom(32), CryptoPRNG::generateRandom(23)));
   }

   for (const auto i : { 1, 3, 5 }) {
      utxos1.push_back(utxos[i]);
   }
   for (const auto i : { 2, 4, 6 }) {
      utxos2.push_back(utxos[i]);
   }
   ASSERT_EQ(utxos.size(), nbUtxos);
   ASSERT_EQ(utxos1.size(), 3);
   ASSERT_EQ(utxos2.size(), 3);

   filtered = utxos;
   bs::UtxoReservation ur(StaticLogger::loggerPtr);
   ur.reserve("id1", utxos1);
   ur.reserve("id2", utxos2);
   ur.filter(filtered, tmp);
   EXPECT_EQ(filtered.size(), nbUtxos - 6);

   ur.filter(filtered, tmp);
   EXPECT_EQ(filtered.size(), nbUtxos - 6);
   EXPECT_EQ(ur.unreserve("id3"), false);

   EXPECT_EQ(ur.unreserve("id1"), true);
   filtered = utxos;
   ur.filter(filtered, tmp);
   EXPECT_EQ(filtered.size(), nbUtxos - 3);

   EXPECT_EQ(ur.unreserve("id2"), true);
   filtered = utxos;
   ur.filter(filtered, tmp);
   EXPECT_EQ(filtered.size(), nbUtxos);
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

TEST(TestCommon, BotanECIES)
{
   Botan::AutoSeeded_RNG rng;

   Botan::EC_Group domain("secp256k1");
   Botan::BigInt privateKeyValue(rng, 256);

   Botan::ECDH_PrivateKey privateKey(rng, domain, privateKeyValue);

   // Raw value we could use to stream
   std::vector<uint8_t> publicKeyValue = privateKey.public_value(Botan::PointGFp::COMPRESSED);
   EXPECT_EQ(publicKeyValue.size(), 33);

   // Another variant:
//   Botan::ECIES_System_Params ecies_params(domain, "KDF1-18033(SHA-512)", "AES-256/CBC", 32,
//      "HMAC(SHA-512)", 20);

   // Use stream cipher and compressed point to get smaller encrypted size
   Botan::ECIES_System_Params eciesParams(domain,
      "KDF2(SHA-256)", "ChaCha(20)", 32, "HMAC(SHA-256)", 20,
      Botan::PointGFp::COMPRESSED, Botan::ECIES_Flags::NONE);

   auto publicKeyDecoded = Botan::OS2ECP(publicKeyValue, domain.get_curve());

   Botan::ECIES_Encryptor encrypt(rng, eciesParams);

   encrypt.set_other_key(publicKeyDecoded);

   // Block ciphers (ie AES-256/CBC) need IV vector set, we use stream cipher here (ChaCha(20))
   auto iv = std::vector<uint8_t>(0, 0);
   encrypt.set_initialization_vector(iv);

   auto plainData = rng.random_vec(16);

   std::vector<uint8_t> encryptedData = encrypt.encrypt(plainData, rng);

   Botan::ECIES_Decryptor decryptor(privateKey, eciesParams, rng);

   decryptor.set_initialization_vector(iv);

   auto decryptedData = decryptor.decrypt(encryptedData);

//   std::cout << "encrypted size: " << encryptedData.size() << "\n";

   EXPECT_EQ(plainData, decryptedData);
}

TEST(TestCommon, BotanECDSA)
{
   Botan::AutoSeeded_RNG rng;

   Botan::EC_Group domain("secp256k1");
   Botan::BigInt privateKeyValue(rng, 256);

   Botan::ECDSA_PrivateKey privateKey(rng, domain, privateKeyValue);

   // Raw value we could use to stream
   std::vector<uint8_t> publicKeyValue = privateKey.public_point().encode(Botan::PointGFp::COMPRESSED);
   EXPECT_EQ(publicKeyValue.size(), 33);

   std::string text("This is a tasty burger!");
   std::vector<uint8_t> data(text.data(), text.data() + text.length());

   Botan::PK_Signer signer(privateKey, rng, "EMSA1(SHA-256)");
   signer.update(data);
   std::vector<uint8_t> signature = signer.signature(rng);

   auto publicKeyDecoded = Botan::OS2ECP(publicKeyValue, domain.get_curve());
   Botan::ECDSA_PublicKey publicKey(domain, publicKeyDecoded);

   Botan::PK_Verifier verifier(publicKey, "EMSA1(SHA-256)");
   verifier.update(data);

   EXPECT_TRUE(verifier.check_signature(signature));

   auto signatureInvalid = signature;
   signatureInvalid.back() = ~signatureInvalid.back();

   Botan::PK_Verifier verifier2(publicKey, "EMSA1(SHA-256)");
   verifier2.update(data);
   EXPECT_FALSE(verifier2.check_signature(signatureInvalid));
}

TEST(TestCommon, BotanSerpent)
{
   Botan::AutoSeeded_RNG rng;
   const auto privKey = CryptoPRNG::generateRandom(32);
   const auto password = CryptoPRNG::generateRandom(16);
   const auto wrongPassword = CryptoPRNG::generateRandom(16);

   Botan::Serpent encrypter;
   encrypter.set_key(password.getDataVector());

   std::vector<uint8_t> encrypted(privKey.getSize());
   encrypter.encrypt(privKey.getDataVector(), encrypted);

   ASSERT_FALSE(encrypted.empty());
   EXPECT_NE(encrypted, privKey.getDataVector());

   Botan::Serpent decrypter;
   decrypter.set_key(wrongPassword.getDataVector());
   std::vector<uint8_t> decrypted(privKey.getSize());
   decrypter.decrypt(encrypted, decrypted);
   EXPECT_NE(decrypted, privKey.getDataVector());

   decrypter.set_key(password.getDataVector());
   decrypter.decrypt(encrypted, decrypted);
   EXPECT_EQ(decrypted, privKey.getDataVector());
}

#include "AssetEncryption.h"
TEST(TestCommon, BotanSerpent_KDF_Romix)
{
   Botan::AutoSeeded_RNG rng;
   const auto privKey = CryptoPRNG::generateRandom(32);
   const auto password = CryptoPRNG::generateRandom(16);

   KeyDerivationFunction_Romix encKdf;
   const auto serializedKdf = encKdf.serialize();

   Botan::Serpent encrypter;
   encrypter.set_key(encKdf.deriveKey(password).getDataVector());

   std::vector<uint8_t> encrypted(privKey.getSize());
   encrypter.encrypt(privKey.getDataVector(), encrypted);
   ASSERT_FALSE(encrypted.empty());
   EXPECT_NE(encrypted, privKey.getDataVector());

   Botan::Serpent decrypter;
   const auto decKdf = KeyDerivationFunction::deserialize(serializedKdf);
   std::vector<uint8_t> decrypted(privKey.getSize());
   decrypter.set_key(decKdf->deriveKey(password).getDataVector());
   decrypter.decrypt(encrypted, decrypted);
   EXPECT_EQ(decrypted, privKey.getDataVector());
}

TEST(TestCommon, SelectUtxoForAmount)
{
   auto test = [](const std::vector<uint64_t> &inputs, uint64_t amount, size_t count, uint64_t sum) {
      std::vector<UTXO> utxos;
      for (auto value : inputs) {
         UTXO utxo;
         utxo.value_ = value;
         utxos.push_back(utxo);
      }

      auto result = bs::selectUtxoForAmount(utxos, amount);
      uint64_t resultSum = 0;
      for (const auto &utxo : result) {
         resultSum += utxo.value_;
      }

      ASSERT_EQ(count, result.size());
      ASSERT_EQ(sum, resultSum);
   };

   test({}, 0, 0, 0);

   test({1}, 1, 1, 1);

   test({1, 2}, 1, 1, 1);
   test({2, 1}, 1, 1, 1);
   test({2, 1}, 2, 1, 2);

   test({1, 2, 3}, 0, 0, 0);
   test({1, 2, 3}, 2, 1, 2);
   test({1, 2, 3}, 3, 1, 3);
   test({1, 2, 3}, 4, 2, 4);
   test({1, 2, 3}, 5, 2, 5);
   test({1, 2, 3}, 6, 3, 6);
   test({1, 2, 3}, 7, 3, 6);

   test({10, 15, 20}, 7, 1, 10);
   test({10, 15, 20}, 12, 1, 15);
   test({10, 15, 20}, 17, 1, 20);
   test({10, 15, 20}, 22, 2, 30);
   test({10, 15, 20}, 42, 3, 45);
   test({10, 15, 20}, 100, 3, 45);

   test({1, 1, 1}, 3, 3, 3);
}

TEST(TestCommon, XBTAmount)
{
   auto xbt1 = bs::XBTAmount(double(21*1000*1000));
   // Check that converting to double and back some big amount dooes not loose minimum difference (1 satoshi)
   // This will also check +/- operators
   auto diff1 = bs::XBTAmount((xbt1 + bs::XBTAmount(int64_t(1))).GetValueBitcoin()) - xbt1;
   EXPECT_EQ(diff1, 1);
}

TEST(TestCommon, PriceAmount)
{
   EXPECT_EQ(bs::CentAmount(0.0).to_string(), "0.00");
   EXPECT_EQ(bs::CentAmount(0.1).to_string(), "0.10");
   EXPECT_EQ(bs::CentAmount(-0.1).to_string(), "-0.10");
   EXPECT_EQ(bs::CentAmount(0.19).to_string(), "0.19");
   EXPECT_EQ(bs::CentAmount(-0.19).to_string(), "-0.19");

   EXPECT_EQ(bs::CentAmount(0.129).to_string(), "0.12");
   EXPECT_EQ(bs::CentAmount(0.1299999).to_string(), "0.12");
   EXPECT_EQ(bs::CentAmount(0.13).to_string(), "0.13");
   EXPECT_EQ(bs::CentAmount(-0.129).to_string(), "-0.12");
   EXPECT_EQ(bs::CentAmount(-0.1299999).to_string(), "-0.12");
   EXPECT_EQ(bs::CentAmount(-0.13).to_string(), "-0.13");

   EXPECT_EQ(bs::CentAmount(-0.0001).to_string(), "0.00");
   EXPECT_EQ(bs::CentAmount(0.0001).to_string(), "0.00");

   EXPECT_EQ(bs::CentAmount(12345.0001).to_string(), "12345.00");
   EXPECT_EQ(bs::CentAmount(-12345.0001).to_string(), "-12345.00");
   EXPECT_EQ(bs::CentAmount(0.12345).to_string(), "0.12");
   EXPECT_EQ(bs::CentAmount(-0.12345).to_string(), "0.12");
}

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
#include "Message/Worker.h"
#include "Wallets/HeadlessContainer.h"
#include "Wallets/InprocSigner.h"
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
using namespace Armory::Wallets::Encryption;

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

TEST(TestCommon, Workers)
{
   struct DataIn1 : public bs::InData
   {
      ~DataIn1() override = default;
      std::string message;
   };
   struct DataOut1 : public bs::OutData
   {
      ~DataOut1() override = default;
      std::string message;
   };
   struct DataIn2 : public bs::InData
   {
      ~DataIn2() override = default;
      std::string message;
   };
   struct DataOut2 : public bs::OutData
   {
      ~DataOut2() override = default;
      std::string message;
   };

   class Handler1 : public bs::HandlerImpl<DataIn1, DataOut1>
   {
   protected:
      std::shared_ptr<DataOut1> processData(const std::shared_ptr<DataIn1>& in) override
      {
         DataOut1 out;
         out.message = in->message;
         for (auto& c : out.message) {
            c = std::tolower(c);
         }
         out.message = "h1: " + out.message;
         return std::make_shared<DataOut1>(out);
      }
   };

   class Handler2 : public bs::HandlerImpl<DataIn2, DataOut2>
   {
   protected:
      std::shared_ptr<DataOut2> processData(const std::shared_ptr<DataIn2>& in) override
      {
         DataOut2 out;
         out.message = in->message;
         for (auto& c : out.message) {
            c = std::toupper(c);
         }
         out.message = "h2: " + out.message;
         return std::make_shared<DataOut2>(out);
      }
   };

   class TestWorkerMgr : public bs::WorkerPool
   {
   public:
      TestWorkerMgr() : bs::WorkerPool() {}

      void test1()
      {
         DataIn1 data;
         data.message = "TEST1 message";
         const auto& inData = std::make_shared<DataIn1>(data);

         const auto& cb = [](const std::shared_ptr<bs::OutData>& result)
         {
            auto data = std::static_pointer_cast<DataOut1>(result);
            StaticLogger::loggerPtr->debug("[TestWorkerMgr::test1] {}", data ? data->message : "null");
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->message, "h1: test1 message");
         };
         processQueued(inData, cb);
      }

      void test2()
      {
         DataIn2 data;
         data.message = "TEST2 message";
         const auto& inData = std::make_shared<DataIn2>(data);

         const auto& cb = [](const std::shared_ptr<bs::OutData>& result)
         {
            auto data = std::static_pointer_cast<DataOut2>(result);
            StaticLogger::loggerPtr->debug("[TestWorkerMgr::test2] {}", data ? data->message : "null");
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->message, "h2: TEST2 MESSAGE");
         };
         processQueued(inData, cb);
      }

      void testNested()
      {
         DataIn1 data1;
         data1.message = "NESTED message";
         const auto& inData1 = std::make_shared<DataIn1>(data1);

         const auto& cb1 = [](const std::shared_ptr<bs::OutData>& result)
         {
            auto data = std::static_pointer_cast<DataOut1>(result);
            StaticLogger::loggerPtr->debug("[TestWorkerMgr::nested1] {}", data ? data->message : "null");
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->message, "h1: nested message");
         };

         DataIn2 data2;
         data2.message = "NORMAL message";
         const auto& inData2 = std::make_shared<DataIn2>(data2);

         const auto& cb2 = [this, cb1, inData1]
            (const std::shared_ptr<bs::OutData>& result)
         {
            auto data = std::static_pointer_cast<DataOut2>(result);
            StaticLogger::loggerPtr->debug("[TestWorkerMgr::nested2] {}", data ? data->message : "null");
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->message, "h2: NORMAL MESSAGE");
            processQueued(inData1, cb1);
         };
         processQueued(inData2, cb2);
      }

   protected:
      std::shared_ptr<bs::Worker> worker(const std::shared_ptr<bs::InData>&) override
      {
         const std::vector<std::shared_ptr<bs::Handler>> handlers{
            std::make_shared<Handler1>(), std::make_shared<Handler2>() };
         return std::make_shared<bs::WorkerImpl>(handlers);
      }
   };

   TestWorkerMgr wm;
   for (int i = 0; i < 5; ++i) {
      wm.test1();
      wm.test2();
      wm.testNested();
   }
   StaticLogger::loggerPtr->debug("{} thread[s] used", wm.nbThreads());
   while (!wm.finished()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
   }
}

#include "common.pb.h"
using namespace BlockSettle::Common;
TEST(TestCommon, unparsable_proto)
{
   const auto& data = "fa01f0060aed060a200a95d6ee27cdb6c5cf56e052cfb826d5a383f64bea54f5adef4f20c45f700443300140014a0a302e3030303031353030521433dd7099a58f7730c4c57eda6cb30c0c4ec27e1b5a8505010000000001011a50e276d80729cf66f5ba49e3d0d1319e84073efef579995b3bdd45edc27da70000000000fdffffff01dc050000000000002251206059a08c63741bce44793c30d941723a7157c19b97681d531f6af35f5de4a16f03403dfec38046483d2c520754c4e61124463ab58d701a85ecd21083aefd93a4dac9434414cd0a197b52d3531e55e1956a6492f2fbfb297b08d06dcf3061424ffaa6fdbe012082c9e496d987a3a64d948cd10bc083472d6b8edf609863251c4d463511942388ac0063036f7264010109696d6167652f706e67004d850189504e470d0a1a0a0000000d4948445200000018000000180806000000e0773df8000000017352474200aece1ce90000000467414d410000b18f0bfc6105000000097048597300000ec300000ec301c76fa8640000011a49444154484be591b10dc2301045b3040b50b205d414f4cc1010743474200a2a6acac018f4acc00ad4144848c63fe2c371b9d804990a4b4f8e63df7f17279b1557f74bfe44b0c8327730b0ce6aa282ba7062d54882825838b16a4963c171973fe13bab9654040825323884ce9004bf20f3c5b18133562d890a701518addeb9a41c103fe4c904e3cded25184dd20bea46124188a402ac491201d012492c1c4405404ad6d37ec927e1207a454476be5f0ddff6ac5a620a50d4f1e49eb9c79daa12acf17ee9e790a422c0e1ae67e0d93e80cc5d5e9232dcafb98fb375125380ae59887006a1630463d6fb5f0950acbfa46ebf91809de119c5ba535ca15c63bf91401663d69db6d5bab18057c0623c0306c97f8435cfeaac59717577f032db66d32fac780000000049454e44ae4260826821c182c9e496d987a3a64d948cd10bc083472d6b8edf609863251c4d4635119423880000000060a1ee2f6a4a0a14fd9208e542e2001907786b18c232c15f18cacd8518cc16220b2d302e3030303032383932280532201a50e276d80729cf66f5ba49e3d0d1319e84073efef579995b3bdd45edc27da7724b0a1433dd7099a58f7730c4c57eda6cb30c0c4ec27e1b18dc0b220a302e30303030313530302805322251206059a08c63741bce44793c30d941723a7157c19b97681d531f6af35f5de4a16f";
   const auto binData = BinaryData::CreateFromHex(data);
   ASSERT_FALSE(binData.empty());
   WalletsMessage msg;
   EXPECT_TRUE(msg.ParseFromString(binData.toBinStr()));
   StaticLogger::loggerPtr->debug("[{}] data case: {}\n{}", __func__
      , msg.data_case(), msg.DebugString());
}
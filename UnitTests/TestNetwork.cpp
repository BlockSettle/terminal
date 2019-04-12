#include <gtest/gtest.h>
#include <QApplication>
#include <QDebug>
#include "CelerMessageMapper.h"
#include "CommonTypes.h"
#include "IdStringGenerator.h"
#include "QuoteProvider.h"
#include "TestEnv.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_ServerConnection.h"

TEST(TestNetwork, CelerMessageMapper)
{
   for (int i = CelerAPI::CelerMessageTypeFirst; i < CelerAPI::CelerMessageTypeLast; i++) {
      const auto t = static_cast<CelerAPI::CelerMessageType>(i);
      const auto str = CelerAPI::GetMessageClass(t);
      ASSERT_FALSE(str.empty()) << "At index " << i;
      EXPECT_EQ(CelerAPI::GetMessageType(str), t) << "At index " << i;
   }
   EXPECT_EQ(CelerAPI::GetMessageType("non-existent"), CelerAPI::UndefinedType);
}

using namespace bs::network;

TEST(TestNetwork, Types)
{
   for (const auto t : {Side::Buy, Side::Sell}) {
      const auto celerType = Side::toCeler(t);
      EXPECT_EQ(Side::fromCeler(celerType), t) << "Side " << Side::toString(t);
   }

   for (int i = bs::network::Asset::first; i < bs::network::Asset::last; i++) {
      const auto t = static_cast<bs::network::Asset::Type>(i);
      const auto celerProdType = bs::network::Asset::toCelerProductType(t);
      EXPECT_EQ(bs::network::Asset::fromCelerProductType(celerProdType), t) << "Asset type " << bs::network::Asset::toString(t);
   }
}

TEST(TestNetwork, IdString)
{
   IdStringGenerator gen;
   EXPECT_EQ(gen.getNextId(), "1");
   EXPECT_EQ(gen.getNextId(), "2");
}

TEST(TestNetwork, PayinsContainer)
{
   bs::PayinsContainer payins(StaticLogger::loggerPtr);
   EXPECT_FALSE(payins.erase("non-existent"));
   const std::string key = "settlement1";
   EXPECT_TRUE(payins.get(key).isNull());
   const auto tx = CryptoPRNG::generateRandom(16);
   EXPECT_TRUE(payins.save(key, tx));
   EXPECT_EQ(payins.get(key), tx);
   EXPECT_FALSE(payins.save(key, CryptoPRNG::generateRandom(16)));
   EXPECT_TRUE(payins.erase(key));
   EXPECT_FALSE(payins.erase(key));
}

TEST(TestNetwork, ZMQ_BIP15X)
{
   static std::promise<bool> connectProm;
   static auto connectFut = connectProm.get_future();
   static bool connReported = false;
   static std::promise<bool> clientPktsProm;
   static auto clientPktsFut = clientPktsProm.get_future();
   static std::promise<bool> srvPktsProm;
   static auto srvPktsFut = srvPktsProm.get_future();

   static std::vector<std::string> clientPackets;
   static size_t clientPktCnt = 0;
   for (int i = 0; i < 5; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(23).toBinStr());
   }
   clientPackets.push_back(CryptoPRNG::generateRandom(1475).toBinStr());   // max value for fragmentation code to start to fail
   clientPackets.push_back(CryptoPRNG::generateRandom(2300).toBinStr());   // comment this line out to see if test will pass
   for (int i = 0; i < 5; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   static std::vector<std::string> srvPackets;
   static size_t srvPktCnt = 0;
   size_t pktSize = 100;
   for (int i = 0; i < 11; ++i) {
      srvPackets.push_back(CryptoPRNG::generateRandom(pktSize).toBinStr());
      pktSize *= 2;
   }
   for (int i = 0; i < 5; ++i) {
      srvPackets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   class ServerConnListener : public ServerConnectionListener
   {
   public:
      ServerConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : ServerConnectionListener(), logger_(logger) {}
      ~ServerConnListener() noexcept override = default;

      bool onReady(int cur = 0, int total = 0);

   protected:
      void OnDataFromClient(const std::string &clientId, const std::string &data) override {
         logger_->debug("[{}] {} from {} #{}", __func__, data.size()
            , BinaryData(clientId).toHexStr(), clientPktCnt);
         if (clientPktCnt < clientPackets.size()) {
            if (clientPackets[clientPktCnt++] != data) {
               logger_->error("[{}] packet #{} mismatch", __func__, clientPktCnt - 1);
            }
         }
         if (!failed_ && (clientPktCnt == clientPackets.size())) {
            clientPktsProm.set_value(true);
         }
      }
      void onClientError(const std::string &clientId, const std::string &errStr) override {
         logger_->debug("[{}] {}: {}", __func__, BinaryData(clientId).toHexStr(), errStr);
         if (!connReported) {
            connectProm.set_value(false);
            connReported = true;
         }
         if (!failed_) {
            clientPktsProm.set_value(false);
            failed_ = true;
         }
      }
      void OnClientConnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
      }
      void OnClientDisconnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool  failed_ = false;
   };

   class ClientConnListener : public DataConnectionListener
   {
   public:
      ClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override {
         logger_->debug("[{}] {} #{}", __func__, data.size(), srvPktCnt);
         if (srvPktCnt < srvPackets.size()) {
            if (srvPackets[srvPktCnt++] != data) {
               logger_->error("[{}] packet #{} mismatch", __func__, srvPktCnt - 1);
            }
         }
         if (!failed_ && (srvPktCnt == srvPackets.size())) {
            srvPktsProm.set_value(true);
         }
      }
      void OnConnected() override {
         logger_->debug("[{}]", __func__);
         connectProm.set_value(true);
         connReported = true;
      }
      void OnDisconnected() override {
         logger_->debug("[{}]", __func__);
         if (!connReported) {
            connectProm.set_value(false);
            connReported = true;
         }
         if (!failed_) {
            srvPktsProm.set_value(false);
            failed_ = true;
         }
      }
      void OnError(DataConnectionError errorCode) override {
         logger_->debug("[{}] {}", __func__, int(errorCode));
         if (!connReported) {
            connectProm.set_value(false);
            connReported = true;
         }
         if (!failed_) {
            srvPktsProm.set_value(false);
            failed_ = true;
         }
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool  failed_ = false;
   };

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      TestEnv::logger(), true, true);
   const auto zmqContext = std::make_shared<ZmqContext>(TestEnv::logger());
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
      TestEnv::logger(), zmqContext, [trustedClients] { return trustedClients; });
//   serverConn->enableClientCookieUsage();
   const auto serverKey = serverConn->getOwnPubKey();
   clientConn->SetContext(zmqContext);
   const auto srvLsn = std::make_shared<ServerConnListener>(TestEnv::logger());
   const auto clientLsn = std::make_shared<ClientConnListener>(TestEnv::logger());

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
   clientConn->addAuthPeer(serverKey, host + ":" + port);
   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   const bool connResult = connectFut.get();
   EXPECT_TRUE(connResult);

   if (connResult) {
      for (const auto &clientPkt : clientPackets) {
         clientConn->send(clientPkt);
      }
      EXPECT_TRUE(clientPktsFut.get());

      for (const auto &srvPkt : srvPackets) {
         serverConn->SendDataToAllClients(srvPkt);
      }
      EXPECT_TRUE(srvPktsFut.get());
   }

   EXPECT_TRUE(clientConn->closeConnection());
   serverConn.reset();  // This is needed to detach listener before it's destroyed
}

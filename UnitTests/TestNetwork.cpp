/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <gtest/gtest.h>
#include <random>

#include "Bip15xDataConnection.h"
#include "Bip15xServerConnection.h"
#include "CommonTypes.h"
#include "IdStringGenerator.h"
#include "ServerConnection.h"
#include "TestEnv.h"
#include "TransportBIP15x.h"
#include "TransportBIP15xServer.h"
#include "WsDataConnection.h"
#include "WsServerConnection.h"

using namespace std::chrono_literals;

static bs::network::BIP15xParams getTestParams()
{
   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;
   return params;
}

static bs::network::BIP15xPeer getPeerKey(const std::string &name, bs::network::TransportBIP15x *tr)
{
   return bs::network::BIP15xPeer(name, tr->getOwnPubKey());
}

static bs::network::BIP15xPeer getPeerKey(const std::string &host, const std::string &port
   , bs::network::TransportBIP15x *tr)
{
   std::string name = fmt::format("{}:{}", host, port);
   return bs::network::BIP15xPeer(name, tr->getOwnPubKey());
}

static bs::network::TransportBIP15xServer::TrustedClientsCallback getEmptyPeersCallback()
{
   return [] () {
      return bs::network::BIP15xPeers();
   };
}

static bs::network::TransportBIP15xServer::TrustedClientsCallback constructTrustedPeersCallback(
   const bs::network::BIP15xPeers& peers)
{
   return [peers] () {
      return peers;
   };
}

#if 0
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
      const auto celerType = bs::celer::toCeler(t);
      EXPECT_EQ(bs::celer::fromCeler(celerType), t) << "Side " << Side::toString(t);
   }

   for (int i = bs::network::Asset::first; i < bs::network::Asset::last; i++) {
      const auto t = static_cast<bs::network::Asset::Type>(i);
      const auto celerProdType = bs::celer::toCelerProductType(t);
      EXPECT_EQ(bs::celer::fromCelerProductType(celerProdType), t) << "Asset type " << bs::network::Asset::toString(t);
   }
}
#endif   //0

TEST(TestNetwork, IdString)
{
   IdStringGenerator gen;
   for (int i = 1; i < 10; ++i) {
      EXPECT_EQ(gen.getNextId(), std::to_string(i));
   }
}

#if 0    // doesn't build
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
   EXPECT_TRUE(payins.get(key).isNull());
   EXPECT_FALSE(payins.erase(key));
}
#endif   //0

TEST(TestNetwork, BIP15X_2Way)
{
   static std::promise<bool> connectProm;
   static auto connectFut = connectProm.get_future();
   static std::promise<bool> clientPktsProm;
   static auto clientPktsFut = clientPktsProm.get_future();
   static std::promise<bool> srvPktsProm;
   static auto srvPktsFut = srvPktsProm.get_future();

   static std::vector<std::string> clientPackets;
   for (int i = 0; i < 5; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(23).toBinStr());
   }
   clientPackets.push_back(CryptoPRNG::generateRandom(1475).toBinStr());   // max value for fragmentation code to start to fail
   clientPackets.push_back(CryptoPRNG::generateRandom(102400).toBinStr());   // comment this line out to see if test will pass
   clientPackets.push_back(CryptoPRNG::generateRandom(2345).toBinStr());   // comment this line out to see if test will pass
   for (int i = 0; i < 4; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   static std::vector<std::string> srvPackets;
   uint32_t pktSize = 200;
   for (int i = 0; i < 12; ++i) {
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

   protected:
       void OnDataFromClient(const std::string &clientId, const std::string &data) override
       {
         logger_->debug("[{}] {} from {} #{}", __func__, data.size()
            , BinaryData::fromString(clientId).toHexStr(), clientPktCnt_);
         if (clientPktCnt_ < clientPackets.size()) {
            const auto &clientPkt = clientPackets.at(clientPktCnt_++);
            if (clientPkt != data) {
               packetsMatch_ = false;
               logger_->error("[{}] packet #{} mismatch ({} [{}] vs [{}])", __func__
                  , clientPktCnt_ - 1, BinaryData::fromString(clientPkt).toHexStr()
                  , clientPkt.size(), data.size());
            }
         }
         if (!failed_ && (clientPktCnt_ == clientPackets.size())) {
            clientPktsProm.set_value(packetsMatch_);
         }
      }
      void onClientError(const std::string &clientId, ClientError error, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
         if (!failed_) {
            clientPktsProm.set_value(false);
            failed_ = true;
         }
      }
      void OnClientConnected(const std::string &clientId, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }
      void OnClientDisconnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool  failed_ = false;
      size_t clientPktCnt_ = 0;
      bool packetsMatch_ = true;
   };

   class ClientConnListener : public DataConnectionListener
   {
   public:
      ClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override
      {
         logger_->debug("[{}] {} #{}", __func__, data.size(), srvPktCnt_);
         if (srvPktCnt_ < srvPackets.size()) {
            if (srvPackets[srvPktCnt_++] != data) {
               packetsMatch_ = false;
               logger_->error("[{}] packet #{} mismatch", __func__, srvPktCnt_ - 1);
            }
         }
         if (!failed_ && (srvPktCnt_ == srvPackets.size())) {
            srvPktsProm.set_value(packetsMatch_);
         }
      }
      void OnConnected() override
      {
         logger_->debug("[{}]", __func__);
         connectProm.set_value(true);
         connReported_ = true;
      }
      void fail() {
          if (!connReported_) {
              connectProm.set_value(false);
              connReported_ = true;
          }
          if (!failed_) {
              srvPktsProm.set_value(false);
              failed_ = true;
          }
      }
      void OnDisconnected() override
      {
         logger_->debug("[{}]", __func__);
      }
      void OnError(DataConnectionError errorCode) override
      {
         logger_->debug("[{}] {}", __func__, int(errorCode));
         fail();
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool failed_ = false;
      bool connReported_ = false;
      size_t srvPktCnt_ = 0;
      bool packetsMatch_ = true;
   };

   const auto &logger = StaticLogger::loggerPtr;
   const auto srvLsn = std::make_unique<ServerConnListener>(logger);
   const auto clientLsn = std::make_unique<ClientConnListener>(logger);

   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger
      , getTestParams());
   auto wsConn = std::make_unique<WsDataConnection>(logger, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(logger, std::move(wsConn), clientTransport);

   auto clientKey = getPeerKey("client", clientTransport.get());
   auto serverTrustedClients = constructTrustedPeersCallback({clientKey});

   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(logger
      , serverTrustedClients, bs::network::BIP15xAuthMode::TwoWay);
   auto wsServ = std::make_unique<WsServerConnection>(logger, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      logger, std::move(wsServ), srvTransport);
//   serverConn->enableClientCookieUsage();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(connectFut.get());

   StaticLogger::loggerPtr->debug("Start sending client packets");
   for (const auto &clientPkt : clientPackets) {
      clientConn->send(clientPkt);
      if (clientPkt.size() < 123) {
         StaticLogger::loggerPtr->debug("c::sent {}", BinaryData::fromString(clientPkt).toHexStr());
      }
   }
   EXPECT_TRUE(clientPktsFut.get());

   StaticLogger::loggerPtr->debug("Start sending server packets");
   for (const auto &srvPkt : srvPackets) {
      serverConn->SendDataToAllClients(srvPkt);
      if (srvPkt.size() < 123) {
         StaticLogger::loggerPtr->debug("s::sent {}", BinaryData::fromString(srvPkt).toHexStr());
      }
   }
   EXPECT_TRUE(srvPktsFut.get());

   ASSERT_TRUE(clientConn->closeConnection());
//   std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
}

TEST(TestNetwork, BIP15X_1Way)
{
   static std::promise<bool> connectProm;
   static auto connectFut = connectProm.get_future();
   static std::promise<bool> clientPktsProm;
   static auto clientPktsFut = clientPktsProm.get_future();
   static std::promise<bool> srvPktsProm;
   static auto srvPktsFut = srvPktsProm.get_future();

   static std::vector<std::string> clientPackets;
   for (int i = 0; i < 5; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(23).toBinStr());
   }
   clientPackets.push_back(CryptoPRNG::generateRandom(1475).toBinStr());   // max value for fragmentation code to start to fail
   clientPackets.push_back(CryptoPRNG::generateRandom(102400).toBinStr());   // comment this line out to see if test will pass
   clientPackets.push_back(CryptoPRNG::generateRandom(2345).toBinStr());   // comment this line out to see if test will pass
   for (int i = 0; i < 4; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   static std::vector<std::string> srvPackets;
   uint32_t pktSize = 200;
   for (int i = 0; i < 12; ++i) {
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

   protected:
       void OnDataFromClient(const std::string &clientId, const std::string &data) override
       {
         logger_->debug("[{}] {} from {} #{}", __func__, data.size()
            , BinaryData::fromString(clientId).toHexStr(), clientPktCnt_);
         if (clientPktCnt_ < clientPackets.size()) {
            const auto &clientPkt = clientPackets.at(clientPktCnt_++);
            if (clientPkt != data) {
               packetsMatch_ = false;
               logger_->error("[{}] packet #{} mismatch ({} [{}] vs [{}])", __func__
                  , clientPktCnt_ - 1, BinaryData::fromString(clientPkt).toHexStr()
                  , clientPkt.size(), data.size());
            }
         }
         if (!failed_ && (clientPktCnt_ == clientPackets.size())) {
            clientPktsProm.set_value(packetsMatch_);
         }
      }
      void onClientError(const std::string &clientId, ClientError error, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
         if (!failed_) {
            clientPktsProm.set_value(false);
            failed_ = true;
         }
      }
      void OnClientConnected(const std::string &clientId, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }
      void OnClientDisconnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool  failed_ = false;
      size_t clientPktCnt_ = 0;
      bool packetsMatch_ = true;
   };

   class ClientConnListener : public DataConnectionListener
   {
   public:
      ClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override
      {
         logger_->debug("[{}] {} #{}", __func__, data.size(), srvPktCnt_);
         if (srvPktCnt_ < srvPackets.size()) {
            if (srvPackets[srvPktCnt_++] != data) {
               packetsMatch_ = false;
               logger_->error("[{}] packet #{} mismatch", __func__, srvPktCnt_ - 1);
            }
         }
         if (!failed_ && (srvPktCnt_ == srvPackets.size())) {
            srvPktsProm.set_value(packetsMatch_);
         }
      }
      void OnConnected() override
      {
         logger_->debug("[{}]", __func__);
         connectProm.set_value(true);
         connReported_ = true;
      }
      void fail() {
          if (!connReported_) {
              connectProm.set_value(false);
              connReported_ = true;
          }
          if (!failed_) {
              srvPktsProm.set_value(false);
              failed_ = true;
          }
      }
      void OnDisconnected() override
      {
         logger_->debug("[{}]", __func__);
      }
      void OnError(DataConnectionError errorCode) override
      {
         logger_->debug("[{}] {}", __func__, int(errorCode));
         fail();
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool failed_ = false;
      bool connReported_ = false;
      size_t srvPktCnt_ = 0;
      bool packetsMatch_ = true;
   };

   const auto &logger = StaticLogger::loggerPtr;
   const auto srvLsn = std::make_unique<ServerConnListener>(logger);
   const auto clientLsn = std::make_unique<ClientConnListener>(logger);

   auto testParams = getTestParams();
   testParams.authMode = bs::network::BIP15xAuthMode::OneWay;
   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(logger
      , testParams);
   auto wsConn = std::make_unique<WsDataConnection>(logger, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(logger, std::move(wsConn), clientTransport);

   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(logger
      , getEmptyPeersCallback(), bs::network::BIP15xAuthMode::OneWay);
   auto wsServ = std::make_unique<WsServerConnection>(logger, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      logger, std::move(wsServ), srvTransport);

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(connectFut.get());

   StaticLogger::loggerPtr->debug("Start sending client packets");
   for (const auto &clientPkt : clientPackets) {
      clientConn->send(clientPkt);
      if (clientPkt.size() < 123) {
         StaticLogger::loggerPtr->debug("c::sent {}", BinaryData::fromString(clientPkt).toHexStr());
      }
   }
   EXPECT_TRUE(clientPktsFut.get());

   StaticLogger::loggerPtr->debug("Start sending server packets");
   for (const auto &srvPkt : srvPackets) {
      serverConn->SendDataToAllClients(srvPkt);
      if (srvPkt.size() < 123) {
         StaticLogger::loggerPtr->debug("s::sent {}", BinaryData::fromString(srvPkt).toHexStr());
      }
   }
   EXPECT_TRUE(srvPktsFut.get());

   ASSERT_TRUE(clientConn->closeConnection());
//   std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
}

TEST(TestNetwork, BIP15X_Rekey)
{
   static std::promise<bool> connectProm1;
   static auto connectFut1 = connectProm1.get_future();
   static bool conn1Reported = false;
   static std::promise<bool> connectProm2;
   static auto connectFut2 = connectProm2.get_future();
   static bool conn2Reported = false;
   static std::promise<bool> clientPktsProm1;
   static auto clientPktsFut1 = clientPktsProm1.get_future();
   static std::promise<bool> clientPktsProm2;
   static auto clientPktsFut2 = clientPktsProm2.get_future();
   static std::promise<bool> srvPktsProm1;
   static auto srvPktsFut1 = srvPktsProm1.get_future();

   static std::vector<std::string> packets;
   static size_t clientPktCnt = 0;
   static size_t srvPktCnt = 0;
   for (int i = 0; i < 50; ++i) {
      packets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   class ServerConnListener : public ServerConnectionListener
   {
   public:
      ServerConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : ServerConnectionListener(), logger_(logger) {}
      ~ServerConnListener() noexcept override = default;

      bool onReady(int cur = 0, int total = 0);

      void OnDataFromClient(const std::string &clientId, const std::string &data) override {
         logger_->debug("[{}] {} from {} #{}", __func__, data.size()
            , BinaryData::fromString(clientId).toHexStr(), clientPktCnt);
         if (clientPktCnt < packets.size()) {
            if (packets[clientPktCnt++] != data) {
               logger_->error("[{}] packet #{} mismatch", __func__, clientPktCnt - 1);
            }
         }
         else {
            logger_->debug("[{}] rekeying client {} after packet {}", __func__
               , BinaryData::fromString(clientId).toHexStr(), data.size());
            clientPktCnt = 0;
            std::this_thread::sleep_for(10ms);  //FIXME: client fails to decrypt without this delay
            transport_->rekey(clientId);
            return;
         }
         if (clientPktCnt == packets.size()) {
            logger_->debug("[{}] all packets received", __func__);
            if (!prom1Set_) {
               clientPktsProm1.set_value(true);
               prom1Set_ = true;
            }
            else if (!prom2Set_) {
               clientPktsProm2.set_value(true);
               prom2Set_ = true;
            }
         }
      }
      void onClientError(const std::string &clientId, ClientError error, const Details &details) override {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
         if (!conn1Reported) {
            connectProm1.set_value(false);
            conn1Reported = true;
         }
         if (!prom1Set_) {
            clientPktsProm1.set_value(false);
            prom1Set_ = true;
         }
         else if (!prom2Set_) {
            clientPktsProm2.set_value(false);
            prom2Set_ = true;
         }
      }
      void OnClientConnected(const std::string &clientId, const Details &details) override {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }
      void OnClientDisconnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
      }

   public:
      std::shared_ptr<spdlog::logger>  logger_;
      std::shared_ptr<bs::network::TransportBIP15xServer>   transport_;
      bool prom1Set_ = false;
      bool prom2Set_ = false;
   };

   class ClientConnListener : public DataConnectionListener
   {
   public:
      ClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override {
         if (srvPktCnt < packets.size()) {
            if (packets[srvPktCnt++] != data) {
               logger_->error("[{}] packet #{} mismatch", __func__, srvPktCnt - 1);
            }
         }
         if (srvPktCnt == packets.size()) {
            logger_->debug("[{}] all {} packets received", __func__, srvPktCnt);
            if (!promSet_) {
               srvPktsProm1.set_value(true);
               promSet_ = true;
            }
         }
      }
      void OnConnected() override {
         logger_->debug("[{}]", __func__);
         connectProm1.set_value(true);
         conn1Reported = true;
      }
      void OnDisconnected() override {
         logger_->debug("[{}]", __func__);
         if (!conn1Reported) {
            connectProm1.set_value(false);
            conn1Reported = true;
         }
      }
      void OnError(DataConnectionError errorCode) override {
         logger_->debug("[{}] {}", __func__, int(errorCode));
         if (!conn1Reported) {
            connectProm1.set_value(false);
            conn1Reported = true;
         }
         if (!promSet_) {
            srvPktsProm1.set_value(false);
            promSet_ = true;
         }
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      bool  promSet_ = false;
   };

   class AnotherClientConnListener : public DataConnectionListener
   {
   public:
      AnotherClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override {
      }
      void OnConnected() override {
         logger_->debug("[{}]", __func__);
         connectProm2.set_value(true);
         conn2Reported = true;
      }
      void OnDisconnected() override {
         logger_->debug("[{}]", __func__);
         if (!conn2Reported) {
            connectProm2.set_value(false);
            conn2Reported = true;
         }
      }
      void OnError(DataConnectionError errorCode) override {
         logger_->debug("[{}] {}", __func__, int(errorCode));
         if (!conn2Reported) {
            connectProm2.set_value(false);
            conn2Reported = true;
         }
      }

   private:
      std::shared_ptr<spdlog::logger>  logger_;
   };

   // Create listeners before clients to prevent dangling pointer use after destruction
   const auto srvLsn = std::make_unique<ServerConnListener>(StaticLogger::loggerPtr);
   const auto client2Lsn = std::make_unique<AnotherClientConnListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_unique<ClientConnListener>(StaticLogger::loggerPtr);

   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
   auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);

   auto clientKey = getPeerKey("client", clientTransport.get());
   auto serverTrustedClients = constructTrustedPeersCallback({clientKey});

   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, serverTrustedClients, bs::network::BIP15xAuthMode::TwoWay);
   
   auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      StaticLogger::loggerPtr, std::move(wsServ), srvTransport);
   srvLsn->transport_ = srvTransport;

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));
   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   EXPECT_TRUE(connectFut1.get());

   for (const auto &clientPkt : packets) {
      clientConn->send(clientPkt);
   }
   EXPECT_TRUE(clientPktsFut1.get());

   clientTransport->rekey();
   clientConn->send(CryptoPRNG::generateRandom(23).toBinStr());

   for (const auto &srvPkt : packets) {
      serverConn->SendDataToAllClients(srvPkt);
   }
   EXPECT_TRUE(srvPktsFut1.get());

   for (const auto &clientPkt : packets) {
      clientConn->send(clientPkt);
   }
   ASSERT_EQ(clientPktsFut2.wait_for(1000ms), std::future_status::ready);
   EXPECT_TRUE(clientPktsFut2.get());
   EXPECT_TRUE(clientConn->closeConnection());

   const auto &client2Transport = std::make_shared<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
   auto wsConn2 = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   const auto client2Conn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn2), clientTransport);
   client2Transport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));
   ASSERT_TRUE(client2Conn->openConnection(host, port, client2Lsn.get()));
   EXPECT_TRUE(connectFut2.get());
   EXPECT_TRUE(client2Conn->closeConnection());
}


static bool await(std::atomic<int>& what, std::chrono::milliseconds deadline = std::chrono::milliseconds{ 1000 }) {
    const auto napTime = 1ms;
    for (auto elapsed = 0ms; elapsed < deadline; elapsed += napTime) {
        if (what.load()) {
            return true;
        }
        std::this_thread::sleep_for(napTime);
    }
    return false;
};

class TstClientListener : public DataConnectionListener
{
public:
    TstClientListener(const std::shared_ptr<spdlog::logger> &logger)
        : DataConnectionListener(), logger_(logger) {}

    void OnDataReceived(const std::string &data) override {
       ++dataRecv_;
       logger_->debug("[{}] {}", __func__, data.size());
    }
    void OnConnected() override {
       ++connected_;
       logger_->debug("[{}]", __func__);
    }
    void OnDisconnected() override {
       ++disconnected_;
       logger_->debug("[{}]", __func__);
    }
    void OnError(DataConnectionError errorCode) override {
       ++error_;
       logger_->debug("[{}] {}", __func__, int(errorCode));
    }

    std::atomic<int> dataRecv_{};
    std::atomic<int> connected_{};
    std::atomic<int> disconnected_{};
    std::atomic<int> error_{};

private:
    std::shared_ptr<spdlog::logger>  logger_;
};

class TstServerListener : public ServerConnectionListener
{
public:
    TstServerListener(const std::shared_ptr<spdlog::logger> &logger)
        : ServerConnectionListener(), logger_(logger) {}
    ~TstServerListener() noexcept override = default;

    void OnDataFromClient(const std::string &clientId, const std::string &data) override {
       dataRecv_++;
       logger_->debug("[{}] {} from {}", __func__, data.size()
           , BinaryData::fromString(clientId).toHexStr());
    }
    void onClientError(const std::string &clientId, ClientError error, const Details &details) override {
       error_++;
       logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
    }
    void OnClientConnected(const std::string &clientId, const Details &details) override {
       lastConnectedClient_ = clientId;
       connected_++;
       if (transport_) {
          lastConnectedKey_ = transport_->getClientKey(clientId);
          ASSERT_TRUE(lastConnectedKey_);
       }
       logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
    }
    void OnClientDisconnected(const std::string &clientId) override {
       lastDisconnectedClient_ = clientId;
       disconnected_++;
       logger_->debug("[{}] {}", __func__, BinaryData::fromString(clientId).toHexStr());
    }

public:
    std::atomic<int> dataRecv_{};
    std::atomic<int> connected_{};
    std::atomic<int> disconnected_{};
    std::atomic<int> error_{};
    std::string lastConnectedClient_;
    std::string lastDisconnectedClient_;
    std::unique_ptr<bs::network::BIP15xPeer> lastConnectedKey_;

    std::shared_ptr<bs::network::TransportBIP15xServer>  transport_;
    std::shared_ptr<spdlog::logger>  logger_;
};

TEST(TestNetwork, BIP15X_ClientClose)
{
    static std::vector<std::string> clientPackets;
    for (int i = 0; i < 5; ++i) {
        clientPackets.push_back(CryptoPRNG::generateRandom(23).toBinStr());
    }

    static std::vector<std::string> srvPackets;
    uint32_t pktSize = 100;
    for (int i = 0; i < 5; ++i) {
        srvPackets.push_back(CryptoPRNG::generateRandom(pktSize).toBinStr());
        pktSize *= 2;
    }

    const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
    const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

    struct Messages {
        size_t clientMsgs;
        size_t serverMsgs;
    } pass[] = {
        {1, 1},
        {5, 5},
        {0, 0},
        {0, 1},
        {1, 0}
    };
    const size_t passes = sizeof pass / sizeof pass[0];

    for (size_t i = 0; i < passes; ++i) {
       const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
          StaticLogger::loggerPtr, getTestParams());
       auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
       const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);

       auto clientKey = getPeerKey("client", clientTransport.get());
       auto serverPeersCb = constructTrustedPeersCallback({clientKey});
       const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
           StaticLogger::loggerPtr, serverPeersCb, bs::network::BIP15xAuthMode::TwoWay);
        std::vector<std::string> trustedClients = {
            std::string("test:") + clientTransport->getOwnPubKey().toHexStr() };
        auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
        auto serverConn = std::make_unique<Bip15xServerConnection>(
           StaticLogger::loggerPtr, std::move(wsServ), srvTransport);

        clientLsn->connected_ = 0;
        clientLsn->disconnected_ = 0;
        srvLsn->connected_ = 0;
        srvLsn->disconnected_ = 0;
        srvLsn->transport_ = srvTransport;

        const std::string host = "127.0.0.1";
        std::string port;
        do {
            port = std::to_string((rand() % 50000) + 10000);
        } while (!serverConn->BindConnection(host, port, srvLsn.get()));

        clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

        ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
        
        ASSERT_TRUE(await(clientLsn->connected_));
        ASSERT_TRUE(await(srvLsn->connected_));

        for (size_t j = 0; j < pass[i].clientMsgs; ++j) {
            clientConn->send(clientPackets.at(j));
        }
        for (size_t j = 0; j < pass[i].serverMsgs; ++j) {
            serverConn->SendDataToAllClients(srvPackets.at(j));
        }

        ASSERT_TRUE(clientConn->closeConnection());
        ASSERT_TRUE(await(clientLsn->disconnected_));
        ASSERT_TRUE(await(srvLsn->disconnected_));
        ASSERT_FALSE(clientLsn->error_.load());
        ASSERT_FALSE(srvLsn->error_.load());
    }
}

TEST(TestNetwork, BIP15X_ClientReopen)
{
    static std::vector<std::string> clientPackets;
    for (int i = 0; i < 5; ++i) {
        clientPackets.push_back(CryptoPRNG::generateRandom(23).toBinStr());
    }

    static std::vector<std::string> srvPackets;
    uint32_t pktSize = 100;
    for (int i = 0; i < 5; ++i) {
        srvPackets.push_back(CryptoPRNG::generateRandom(pktSize).toBinStr());
        pktSize *= 2;
    }

    const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
    const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

    const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
       StaticLogger::loggerPtr, getTestParams());
    auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
    const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);

    auto clientKey = getPeerKey("client", clientTransport.get());
    auto serverPeersCb = constructTrustedPeersCallback({clientKey});
    const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
       StaticLogger::loggerPtr, serverPeersCb, bs::network::BIP15xAuthMode::TwoWay);
    std::vector<std::string> trustedClients = {
        std::string("test:") + clientTransport->getOwnPubKey().toHexStr() };
    auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
    auto serverConn = std::make_unique<Bip15xServerConnection>(
       StaticLogger::loggerPtr, std::move(wsServ), srvTransport);

    const std::string host = "127.0.0.1";
    std::string port;
    do {
        port = std::to_string((rand() % 50000) + 10000);
    } while (!serverConn->BindConnection(host, port, srvLsn.get()));

    srvTransport->addAuthPeer(getPeerKey("client", clientTransport.get()));
    clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

    struct Messages {
        size_t clientMsgs;
        size_t serverMsgs;
    } pass[] = {
        {1, 1},
        {5, 5},
        {0, 0},
        {0, 1},
        {1, 0}
    };
    const size_t passes = sizeof pass / sizeof pass[0];

    for (size_t i = 0; i < passes; ++i) {
        clientLsn->connected_ = 0;
        srvLsn->connected_ = 0;

        ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));

        ASSERT_TRUE(await(clientLsn->connected_));
        ASSERT_TRUE(await(srvLsn->connected_));

        for (size_t j = 0; j < pass[i].clientMsgs; ++j) {
            clientConn->send(clientPackets.at(j));
        }
        for (size_t j = 0; j < pass[i].serverMsgs; ++j) {
            serverConn->SendDataToAllClients(srvPackets.at(j));
        }

        clientLsn->disconnected_ = 0;
        srvLsn->disconnected_ = 0;

        ASSERT_TRUE(clientConn->closeConnection()) << i;
        ASSERT_TRUE(await(clientLsn->disconnected_)) << i;
        ASSERT_TRUE(await(srvLsn->disconnected_)) << i;
        ASSERT_FALSE(clientLsn->error_.load()) << i;
        ASSERT_FALSE(srvLsn->error_.load()) << i;
    }
    serverConn.reset();  // This is needed to detach listener before it's destroyed
}

TEST(TestNetwork, BIP15X_DisconnectCounters)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
   auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientTransport->getOwnPubKey().toHexStr() };

   auto clientKey = getPeerKey("client", clientTransport.get());
   auto serverPeersCb = constructTrustedPeersCallback({clientKey});
   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, serverPeersCb, bs::network::BIP15xAuthMode::TwoWay);
   auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      StaticLogger::loggerPtr, std::move(wsServ), srvTransport);

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));

   ASSERT_TRUE(await(clientLsn->connected_));
   ASSERT_TRUE(await(srvLsn->connected_));

   ASSERT_TRUE(clientConn->closeConnection());

   ASSERT_TRUE(await(clientLsn->disconnected_));
   ASSERT_TRUE(await(srvLsn->disconnected_));

   serverConn.reset();

   ASSERT_EQ(clientLsn->connected_.load(), 1);
   ASSERT_EQ(srvLsn->connected_.load(), 1);
   ASSERT_EQ(clientLsn->disconnected_.load(), 1);
   ASSERT_EQ(srvLsn->disconnected_.load(), 1);
   ASSERT_EQ(clientLsn->error_.load(), 0);
   ASSERT_EQ(srvLsn->error_.load(), 0);
}

// Disabled because it's not really a unit test
TEST(TestNetwork, DISABLED_ZMQ_BIP15X_StressTest)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
   auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);

   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, getEmptyPeersCallback(), bs::network::BIP15xAuthMode::TwoWay);
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientTransport->getOwnPubKey().toHexStr() };
   auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      StaticLogger::loggerPtr, std::move(wsServ), srvTransport);

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));

   std::this_thread::sleep_for(1000ms);

   int sendCount = 0;
   while (true) {
      while (sendCount - srvLsn->dataRecv_.load() - clientLsn->dataRecv_.load() < 20) {
         int pktSize = (rand() % 1000) + 1000;
         clientConn->send(CryptoPRNG::generateRandom(pktSize).toBinStr());
         serverConn->SendDataToAllClients(CryptoPRNG::generateRandom(pktSize).toBinStr());
         sendCount += 2;
      }
      std::this_thread::sleep_for(1ns);
   }
}

TEST(TestNetwork, BIP15X_ClientKey)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto &clientTransport = std::make_shared<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
   auto wsConn = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   const auto clientConn = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(wsConn), clientTransport);

   auto clientKey = getPeerKey("client", clientTransport.get());
   auto serverPeersCb = constructTrustedPeersCallback({clientKey});
   const auto &srvTransport = std::make_shared<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, serverPeersCb, bs::network::BIP15xAuthMode::TwoWay);
   auto wsServ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto serverConn = std::make_unique<Bip15xServerConnection>(
      StaticLogger::loggerPtr, std::move(wsServ), srvTransport);

   srvLsn->transport_ = srvTransport;

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   clientTransport->addAuthPeer(getPeerKey(host, port, srvTransport.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(await(clientLsn->connected_));

   ASSERT_TRUE(clientConn->send("test"));
   ASSERT_TRUE(await(srvLsn->dataRecv_));
   ASSERT_TRUE(serverConn->SendDataToAllClients("test2"));
   ASSERT_TRUE(await(clientLsn->dataRecv_));

   ASSERT_TRUE(srvLsn->lastConnectedKey_);
   ASSERT_TRUE(clientTransport->getOwnPubKey().getSize() == 33);
   EXPECT_TRUE(srvLsn->lastConnectedKey_->pubKey() == clientTransport->getOwnPubKey());
}

#include <gtest/gtest.h>
#include <random>
#include "CelerMessageMapper.h"
#include "CommonTypes.h"
#include "IdStringGenerator.h"
#include "QuoteProvider.h"
#include "TestEnv.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_ServerConnection.h"
#include "zmq.h"

using namespace std::chrono_literals;

ZmqBIP15XDataConnectionParams getTestParams()
{
   ZmqBIP15XDataConnectionParams params;
   params.ephemeralPeers = true;
   return params;
}

ZmqBIP15XPeer getPeerKey(const std::string &name, ZmqBIP15XDataConnection *conn)
{
   return ZmqBIP15XPeer(name, conn->getOwnPubKey());
}

ZmqBIP15XPeer getPeerKey(const std::string &host, const std::string &port, ZmqBIP15XServerConnection *conn)
{
   std::string name = fmt::format("{}:{}", host, port);
   return ZmqBIP15XPeer(name, conn->getOwnPubKey());
}

ZmqBIP15XServerConnection::TrustedClientsCallback getEmptyPeersCallback()
{
   return [] () {
      return ZmqBIP15XPeers();
   };
}

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

TEST(TestNetwork, ZMQ_BIP15X)
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
   clientPackets.push_back(CryptoPRNG::generateRandom(2300).toBinStr());   // comment this line out to see if test will pass
   for (int i = 0; i < 5; ++i) {
      clientPackets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   static std::vector<std::string> srvPackets;
   uint32_t pktSize = 100;
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

   protected:
       void OnDataFromClient(const std::string &clientId, const std::string &data) override {
         logger_->error("[{}] {} from {} #{}", __func__, data.size()
            , BinaryData(clientId).toHexStr(), clientPktCnt_);
         if (clientPktCnt_ < clientPackets.size()) {
            if (clientPackets[clientPktCnt_++] != data) {
               packetsMatch_ = false;
               logger_->error("[{}] packet #{} mismatch", __func__, clientPktCnt_ - 1);
            }
         }
         if (!failed_ && (clientPktCnt_ == clientPackets.size())) {
            clientPktsProm.set_value(packetsMatch_);
         }
      }
      void onClientError(const std::string &clientId, const std::string &errStr) override {
         logger_->debug("[{}] {}: {}", __func__, BinaryData(clientId).toHexStr(), errStr);
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
      size_t clientPktCnt_ = 0;
      bool packetsMatch_ = true;
   };

   class ClientConnListener : public DataConnectionListener
   {
   public:
      ClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : DataConnectionListener(), logger_(logger) {}

      void OnDataReceived(const std::string &data) override {
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
      void OnConnected() override {
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
      void OnDisconnected() override {
         logger_->debug("[{}]", __func__);
      }
      void OnError(DataConnectionError errorCode) override {
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

   const auto srvLsn = std::make_unique<ServerConnListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_unique<ClientConnListener>(StaticLogger::loggerPtr);

   const auto clientConn = std::make_unique<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, getTestParams());
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   auto serverConn = std::make_unique<ZmqBIP15XServerConnection>(
      StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
//   serverConn->enableClientCookieUsage();
   const auto serverKey = serverConn->getOwnPubKey();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));
   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(connectFut.get());

   for (const auto &clientPkt : clientPackets) {
      clientConn->send(clientPkt);
   }
   EXPECT_TRUE(clientPktsFut.get());

   for (const auto &srvPkt : srvPackets) {
      serverConn->SendDataToAllClients(srvPkt);
   }
   EXPECT_TRUE(srvPktsFut.get());

   ASSERT_TRUE(clientConn->closeConnection());
}


TEST(TestNetwork, ZMQ_BIP15X_Rekey)
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
            , BinaryData(clientId).toHexStr(), clientPktCnt);
         if (clientPktCnt < packets.size()) {
            if (packets[clientPktCnt++] != data) {
               logger_->error("[{}] packet #{} mismatch", __func__, clientPktCnt - 1);
            }
         }
         else {
            logger_->debug("[{}] rekeying client {} after packet {}", __func__
               , BinaryData(clientId).toHexStr(), data.size());
            clientPktCnt = 0;
            connection_->rekey(clientId);
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
      void onClientError(const std::string &clientId, const std::string &errStr) override {
         logger_->debug("[{}] {}: {}", __func__, BinaryData(clientId).toHexStr(), errStr);
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
      void OnClientConnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
      }
      void OnClientDisconnected(const std::string &clientId) override {
         logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
      }

      std::shared_ptr<spdlog::logger>  logger_;
      ZmqBIP15XServerConnection *connection_{};
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

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, getTestParams());
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
      StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
   srvLsn->connection_ = serverConn.get();
   const auto serverKey = serverConn->getOwnPubKey();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));
   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   EXPECT_TRUE(connectFut1.get());

   for (const auto &clientPkt : packets) {
      clientConn->send(clientPkt);
   }
   EXPECT_TRUE(clientPktsFut1.get());

   clientConn->rekey();
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

   const auto client2Conn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, getTestParams());
   client2Conn->addAuthPeer(getPeerKey(host, port, serverConn.get()));
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
       ++dataRecv_;
       logger_->debug("[{}] {} from {}", __func__, data.size()
           , BinaryData(clientId).toHexStr());
    }
    void onClientError(const std::string &clientId, const std::string &errStr) override {
       ++error_;
       logger_->debug("[{}] {}: {}", __func__, BinaryData(clientId).toHexStr(), errStr);
    }
    void OnClientConnected(const std::string &clientId) override {
       lastConnectedClient_ = clientId;
       ++connected_;
       if (server_) {
          lastConnectedKey_ = server_->getClientKey(clientId);
          ASSERT_TRUE(lastConnectedKey_);
       }
       logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
    }
    void OnClientDisconnected(const std::string &clientId) override {
       lastDisconnectedClient_ = clientId;
       ++disconnected_;
       logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
    }

    std::atomic<int> dataRecv_{};
    std::atomic<int> connected_{};
    std::atomic<int> disconnected_{};
    std::atomic<int> error_{};
    std::string lastConnectedClient_;
    std::string lastDisconnectedClient_;
    std::unique_ptr<ZmqBIP15XPeer> lastConnectedKey_;

    ZmqBIP15XServerConnection *server_{};
    std::shared_ptr<spdlog::logger>  logger_;
};


TEST(TestNetwork, ZMQ_BIP15X_ClientClose)
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
        const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
        const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
        std::vector<std::string> trustedClients = {
            std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
        auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
            StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
        const auto serverKey = serverConn->getOwnPubKey();

        clientLsn->connected_ = 0;
        clientLsn->disconnected_ = 0;
        srvLsn->connected_ = 0;
        srvLsn->disconnected_ = 0;

        const std::string host = "127.0.0.1";
        std::string port;
        do {
            port = std::to_string((rand() % 50000) + 10000);
        } while (!serverConn->BindConnection(host, port, srvLsn.get()));

        serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
        clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

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


TEST(TestNetwork, ZMQ_BIP15X_ClientReopen)
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

    const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
        StaticLogger::loggerPtr, getTestParams());
    const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
    std::vector<std::string> trustedClients = {
        std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
    auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
        StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
    const auto serverKey = serverConn->getOwnPubKey();

    const std::string host = "127.0.0.1";
    std::string port;
    do {
        port = std::to_string((rand() % 50000) + 10000);
    } while (!serverConn->BindConnection(host, port, srvLsn.get()));

    serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
    clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

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
        clientLsn->connected_ = false;
        srvLsn->connected_ = false;

        ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));

        ASSERT_TRUE(await(clientLsn->connected_));
        ASSERT_TRUE(await(srvLsn->connected_));

        for (size_t j = 0; j < pass[i].clientMsgs; ++j) {
            clientConn->send(clientPackets.at(j));
        }
        for (size_t j = 0; j < pass[i].serverMsgs; ++j) {
            serverConn->SendDataToAllClients(srvPackets.at(j));
        }

        clientLsn->disconnected_ = false;
        srvLsn->disconnected_ = false;

        ASSERT_TRUE(clientConn->closeConnection());
        ASSERT_TRUE(await(clientLsn->disconnected_));
        ASSERT_TRUE(await(srvLsn->disconnected_));
        ASSERT_FALSE(clientLsn->error_.load());
        ASSERT_FALSE(srvLsn->error_.load());
    }
    serverConn.reset();  // This is needed to detach listener before it's destroyed
}


TEST(TestNetwork, DISABLED_ZMQ_BIP15X_Heartbeat)
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
        auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
        const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
        std::vector<std::string> trustedClients = {
            std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
        auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
            StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
        const auto serverKey = serverConn->getOwnPubKey();

        clientLsn->connected_ = false;
        srvLsn->connected_ = false;

        const std::string host = "127.0.0.1";
        std::string port;
        do {
            port = std::to_string((rand() % 50000) + 10000);
        } while (!serverConn->BindConnection(host, port, srvLsn.get()));

        serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
        clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

        ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));

        ASSERT_TRUE(await(clientLsn->connected_));
        ASSERT_TRUE(await(srvLsn->connected_));

        for (size_t j = 0; j < pass[i].clientMsgs; ++j) {
            clientConn->send(clientPackets.at(j));
        }
        for (size_t j = 0; j < pass[i].serverMsgs; ++j) {
            serverConn->SendDataToAllClients(srvPackets.at(j));
        }

        srvLsn->disconnected_ = false;

        const auto allowedJitter = 1000ms;

        std::this_thread::sleep_for(2 * ZmqBIP15XServerConnection::getDefaultHeartbeatInterval() + allowedJitter);
        ASSERT_FALSE(clientLsn->disconnected_.load());
        ASSERT_FALSE(srvLsn->disconnected_.load());

        clientConn.reset();

        ASSERT_TRUE(await(srvLsn->disconnected_, 2 * ZmqBIP15XServerConnection::getDefaultHeartbeatInterval() + allowedJitter));
        ASSERT_FALSE(clientLsn->error_.load());
        ASSERT_FALSE(srvLsn->error_.load());

        serverConn.reset();  // This is needed to detach listener before it's destroyed
    }
}

TEST(TestNetwork, ZMQ_BIP15X_DisconnectCounters)
{
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);

   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, getTestParams());
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
      StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
   const auto serverKey = serverConn->getOwnPubKey();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

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

TEST(TestNetwork, ZMQ_BIP15X_ConnectionTimeout)
{
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);

   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   auto params = getTestParams();
   params.heartbeatInterval = std::chrono::milliseconds{1};
   params.connectionTimeout = std::chrono::milliseconds{1};

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, params);

   ASSERT_TRUE(clientConn->openConnection("localhost", "64000", clientLsn.get()));

   ASSERT_TRUE(await(clientLsn->error_));

   ASSERT_EQ(clientLsn->connected_.load(), 0);
   ASSERT_EQ(clientLsn->disconnected_.load(), 0);
   ASSERT_EQ(clientLsn->error_.load(), 1);
}

// Disabled because it's not really a unit test
TEST(TestNetwork, DISABLED_ZMQ_BIP15X_StressTest)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
            StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
   const auto serverKey = serverConn->getOwnPubKey();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

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

TEST(TestNetwork, ZMQ_BIP15X_MalformedData)
{
   std::uniform_int_distribution<uint32_t> distribution(0, 255);
   std::mt19937 generator;
   generator.seed(1);

   for (int i = 0; i < 10; ++i) {
      const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
      const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);
      const auto clientLsn2 = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

      const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
               StaticLogger::loggerPtr, getTestParams());
      const auto clientConn2 = std::make_shared<ZmqBIP15XDataConnection>(
               StaticLogger::loggerPtr, getTestParams());
      const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
      auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
               StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
      const auto serverKey = serverConn->getOwnPubKey();

      const std::string host = "127.0.0.1";
      std::string port;
      do {
         port = std::to_string((rand() % 50000) + 10000);
      } while (!serverConn->BindConnection(host, port, srvLsn.get()));

      serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
      serverConn->addAuthPeer(getPeerKey("client2", clientConn2.get()));
      clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));
      clientConn2->addAuthPeer(getPeerKey(host, port, serverConn.get()));

      auto badContext = zmq_ctx_new();
      auto badSocket = zmq_socket(badContext, ZMQ_DEALER);

      ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
      ASSERT_TRUE(await(clientLsn->connected_));

      ASSERT_TRUE(clientConn->send("test"));
      ASSERT_TRUE(await(srvLsn->dataRecv_));
      srvLsn->dataRecv_ = 0;

      ASSERT_EQ(zmq_connect(badSocket, fmt::format("tcp://{}:{}", host, port).c_str()), 0);

      auto badSize = distribution(generator);
      auto badData = CryptoPRNG::generateRandom(badSize).toBinStr();
      ASSERT_EQ(zmq_send(badSocket, badData.data(), badData.size(), 0), badData.size());

      ASSERT_TRUE(await(srvLsn->error_));

      ASSERT_TRUE(clientConn->send("test2"));
      ASSERT_TRUE(await(srvLsn->dataRecv_));

      ASSERT_TRUE(clientConn2->openConnection(host, port, clientLsn2.get()));
      ASSERT_TRUE(await(clientLsn2->connected_));

      ASSERT_EQ(zmq_close(badSocket), 0);
      ASSERT_EQ(zmq_ctx_term(badContext), 0);
   }
}

TEST(TestNetwork, ZMQ_BIP15X_MalformedSndMore)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);
   const auto clientLsn2 = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
   const auto clientConn2 = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
            StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
   const auto serverKey = serverConn->getOwnPubKey();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   serverConn->addAuthPeer(getPeerKey("client2", clientConn2.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));
   clientConn2->addAuthPeer(getPeerKey(host, port, serverConn.get()));

   auto badContext = zmq_ctx_new();
   auto badSocket = zmq_socket(badContext, ZMQ_DEALER);

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(await(clientLsn->connected_));

   ASSERT_TRUE(clientConn->send("test"));
   ASSERT_TRUE(await(srvLsn->dataRecv_));
   srvLsn->dataRecv_ = 0;

   ASSERT_EQ(zmq_connect(badSocket, fmt::format("tcp://{}:{}", host, port).c_str()), 0);

   auto badData = CryptoPRNG::generateRandom(10).toBinStr();
   ASSERT_EQ(zmq_send(badSocket, badData.data(), badData.size(), ZMQ_SNDMORE), badData.size());
   ASSERT_EQ(zmq_send(badSocket, badData.data(), badData.size(), 0), badData.size());

   ASSERT_TRUE(await(srvLsn->error_));

   ASSERT_TRUE(clientConn->send("test2"));
   ASSERT_TRUE(await(srvLsn->dataRecv_));

   srvLsn->connected_ = 0;
   ASSERT_TRUE(clientConn2->openConnection(host, port, clientLsn2.get()));
   ASSERT_TRUE(await(clientLsn2->connected_));
   ASSERT_TRUE(await(srvLsn->connected_));
   ASSERT_TRUE(!srvLsn->lastConnectedClient_.empty());

   srvLsn->dataRecv_ = 0;
   clientConn2->send("request 2");
   ASSERT_TRUE(await(srvLsn->dataRecv_));

   clientLsn2->dataRecv_ = 0;
   serverConn->SendDataToClient(srvLsn->lastConnectedClient_, "reply 2");
   ASSERT_TRUE(await(clientLsn2->dataRecv_));

   ASSERT_EQ(zmq_close(badSocket), 0);
   ASSERT_EQ(zmq_ctx_term(badContext), 0);
}

TEST(TestNetwork, ZMQ_BIP15X_ClientKey)
{
   const auto srvLsn = std::make_shared<TstServerListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<TstClientListener>(StaticLogger::loggerPtr);

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
            StaticLogger::loggerPtr, getTestParams());
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
            StaticLogger::loggerPtr, zmqContext, getEmptyPeersCallback());
   const auto serverKey = serverConn->getOwnPubKey();

   srvLsn->server_ = serverConn.get();

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(getPeerKey("client", clientConn.get()));
   clientConn->addAuthPeer(getPeerKey(host, port, serverConn.get()));

   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   ASSERT_TRUE(await(clientLsn->connected_));

   ASSERT_TRUE(clientConn->send("test"));
   ASSERT_TRUE(await(srvLsn->dataRecv_));
   ASSERT_TRUE(serverConn->SendDataToAllClients("test2"));
   ASSERT_TRUE(await(clientLsn->dataRecv_));

   ASSERT_TRUE(srvLsn->lastConnectedKey_);
   ASSERT_TRUE(clientConn->getOwnPubKey().getSize() == 33);
   EXPECT_TRUE(srvLsn->lastConnectedKey_->pubKey() == clientConn->getOwnPubKey());
}

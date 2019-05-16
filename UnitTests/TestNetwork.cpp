#include <gtest/gtest.h>
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
   for (int i = 1; i < 10; ++i) {
      EXPECT_EQ(gen.getNextId(), std::to_string(i));
   }
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
   EXPECT_TRUE(payins.get(key).isNull());
   EXPECT_FALSE(payins.erase(key));
}

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
         fail();
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

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, true, "", "", true);
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
      StaticLogger::loggerPtr, zmqContext, [trustedClients] { return trustedClients; });
//   serverConn->enableClientCookieUsage();
   const auto serverKey = serverConn->getOwnPubKey();
   clientConn->SetContext(zmqContext);
   const auto srvLsn = std::make_shared<ServerConnListener>(StaticLogger::loggerPtr);
   const auto clientLsn = std::make_shared<ClientConnListener>(StaticLogger::loggerPtr);

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
   clientConn->addAuthPeer(serverKey, host + ":" + port);
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
   serverConn.reset();  // This is needed to detach listener before it's destroyed
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
   for (int i = 0; i < 5; ++i) {
      packets.push_back(CryptoPRNG::generateRandom(230).toBinStr());
   }

   class ServerConnListener : public ServerConnectionListener
   {
   public:
      ServerConnListener(const std::shared_ptr<spdlog::logger> &logger
         , const std::shared_ptr<ZmqBIP15XServerConnection> &conn)
         : ServerConnectionListener(), logger_(logger), connection_(conn) {}
      ~ServerConnListener() noexcept override = default;

      bool onReady(int cur = 0, int total = 0);

   protected:
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
         else {
             if (!conn2Reported) {
                 connectProm2.set_value(false);
                 conn2Reported = true;
             }
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

   private:
      std::shared_ptr<spdlog::logger>  logger_;
      std::shared_ptr<ZmqBIP15XServerConnection>   connection_;
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

   const auto clientConn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, true, "", "", true);
   const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
   std::vector<std::string> trustedClients = {
      std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
   auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
      StaticLogger::loggerPtr, zmqContext, [trustedClients] { return trustedClients; });
   const auto serverKey = serverConn->getOwnPubKey();
   clientConn->SetContext(zmqContext);
   const auto srvLsn = std::make_shared<ServerConnListener>(StaticLogger::loggerPtr, serverConn);
   const auto clientLsn = std::make_shared<ClientConnListener>(StaticLogger::loggerPtr);

   const std::string host = "127.0.0.1";
   std::string port;
   do {
      port = std::to_string((rand() % 50000) + 10000);
   } while (!serverConn->BindConnection(host, port, srvLsn.get()));

   serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
   clientConn->addAuthPeer(serverKey, host + ":" + port);
   ASSERT_TRUE(clientConn->openConnection(host, port, clientLsn.get()));
   const bool connResult = connectFut1.get();
   EXPECT_TRUE(connResult);

   if (connResult) {
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
       EXPECT_TRUE(clientPktsFut2.get());
   }

   EXPECT_TRUE(clientConn->closeConnection());
   const auto client2Lsn = std::make_shared<AnotherClientConnListener>(StaticLogger::loggerPtr);
   const auto client2Conn = std::make_shared<ZmqBIP15XDataConnection>(
      StaticLogger::loggerPtr, true, "", "", true);
   client2Conn->SetContext(zmqContext);
   client2Conn->addAuthPeer(serverKey, host + ":" + port);
   ASSERT_TRUE(client2Conn->openConnection(host, port, client2Lsn.get()));
   EXPECT_TRUE(connectFut2.get());

   EXPECT_TRUE(client2Conn->closeConnection());

   serverConn.reset();  // This is needed to detach listener before it's destroyed
}


static auto await(std::atomic<bool>& what, std::chrono::milliseconds deadline = std::chrono::milliseconds{ 100 }) {
    using namespace std::chrono_literals;
    const auto napTime = 10ms;
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
        logger_->debug("[{}] {}", __func__, data.size());
    }
    void OnConnected() override {
        logger_->debug("[{}]", __func__);
        connected_ = true;
    }
    void OnDisconnected() override {
        logger_->debug("[{}]", __func__);
        disconnected_ = true;
    }
    void OnError(DataConnectionError errorCode) override {
        logger_->debug("[{}] {}", __func__, int(errorCode));
        error_ = true;
    }

    std::atomic<bool> connected_{ false };
    std::atomic<bool> disconnected_{ false };
    std::atomic<bool> error_{ false };

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
        logger_->debug("[{}] {} from {}", __func__, data.size()
            , BinaryData(clientId).toHexStr());
    }
    void onClientError(const std::string &clientId, const std::string &errStr) override {
        logger_->debug("[{}] {}: {}", __func__, BinaryData(clientId).toHexStr(), errStr);
        error_ = true;
    }
    void OnClientConnected(const std::string &clientId) override {
        logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
        connected_ = true;
    }
    void OnClientDisconnected(const std::string &clientId) override {
        logger_->debug("[{}] {}", __func__, BinaryData(clientId).toHexStr());
        disconnected_ = true;
    }

    std::atomic<bool> connected_{ false };
    std::atomic<bool> disconnected_{ false };
    std::atomic<bool> error_{ false };

private:
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
           StaticLogger::loggerPtr, true, "", "", true);
        const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
        std::vector<std::string> trustedClients = {
            std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
        auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
           StaticLogger::loggerPtr, zmqContext, [trustedClients] { return trustedClients; });
        const auto serverKey = serverConn->getOwnPubKey();
        clientConn->SetContext(zmqContext);

        clientLsn->connected_ = false;
        srvLsn->connected_ = false;

        const std::string host = "127.0.0.1";
        std::string port;
        do {
            port = std::to_string((rand() % 50000) + 10000);
        } while (!serverConn->BindConnection(host, port, srvLsn.get()));

        serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
        clientConn->addAuthPeer(serverKey, host + ":" + port);

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
        serverConn.reset();  // This is needed to detach listener before it's destroyed
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
       StaticLogger::loggerPtr, true, "", "", true);
    const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
    std::vector<std::string> trustedClients = {
        std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
    auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
       StaticLogger::loggerPtr, zmqContext, [trustedClients] { return trustedClients; });
    const auto serverKey = serverConn->getOwnPubKey();
    clientConn->SetContext(zmqContext);

    const std::string host = "127.0.0.1";
    std::string port;
    do {
        port = std::to_string((rand() % 50000) + 10000);
    } while (!serverConn->BindConnection(host, port, srvLsn.get()));

    serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
    clientConn->addAuthPeer(serverKey, host + ":" + port);

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


// This test is actually fine, but, takes a _long_ time (10 min last time it was run),
// so it's disabled as to not run by default. You can still run it if you want to check
// the heartbeat handling.
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
    using namespace std::chrono_literals;

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
           StaticLogger::loggerPtr, true, "", "", true);
        const auto zmqContext = std::make_shared<ZmqContext>(StaticLogger::loggerPtr);
        std::vector<std::string> trustedClients = {
            std::string("test:") + clientConn->getOwnPubKey().toHexStr() };
        auto serverConn = std::make_shared<ZmqBIP15XServerConnection>(
           StaticLogger::loggerPtr, zmqContext, [trustedClients] { return trustedClients; });
        const auto serverKey = serverConn->getOwnPubKey();
        clientConn->SetContext(zmqContext);

        clientLsn->connected_ = false;
        srvLsn->connected_ = false;

        const std::string host = "127.0.0.1";
        std::string port;
        do {
            port = std::to_string((rand() % 50000) + 10000);
        } while (!serverConn->BindConnection(host, port, srvLsn.get()));

        serverConn->addAuthPeer(clientConn->getOwnPubKey(), host + ":" + port);
        clientConn->addAuthPeer(serverKey, host + ":" + port);

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

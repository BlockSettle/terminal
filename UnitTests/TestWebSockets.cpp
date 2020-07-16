/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <random>
#include <gtest/gtest.h>
#include <libwebsockets.h>


#include "Bip15xDataConnection.h"
#include "Bip15xServerConnection.h"
#include "DataConnectionListener.h"
#include "RouterServerConnection.h"
#include "ServerConnectionListener.h"
#include "StringUtils.h"
#include "TestEnv.h"
#include "TransportBIP15x.h"
#include "TransportBIP15xServer.h"
#include "WsDataConnection.h"
#include "WsServerConnection.h"

#include <QProcess>

using namespace bs::network;
using namespace std::chrono_literals;

class TestTcpProxyProcess
{
   QProcess process;
public:
   TestTcpProxyProcess(int listenPort, int connectPort)
   {
      {  QProcess killOldProcess;
         QStringList args;
         args.push_back(QStringLiteral("socat"));
         process.start(QStringLiteral("pkill"), args);
         process.waitForFinished();
      }

      QStringList args;
      args.push_back(QStringLiteral("TCP-LISTEN:%1,reuseaddr").arg(listenPort));
      args.push_back(QStringLiteral("TCP:127.0.0.1:%1").arg(connectPort));
      process.start(QStringLiteral("socat"), args);
      bool result = process.waitForStarted(1000);
      assert(result);
   }

   ~TestTcpProxyProcess()
   {
      process.kill();
      bool result = process.waitForFinished(1000);
      assert(result);
   }
};

namespace  {

   const auto kTestTcpHost = "127.0.0.1";
   const auto kTestTcpPort = "54361";

   class TestServerConnListener : public ServerConnectionListener
   {
   public:
      TestServerConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : logger_(logger) {}
      ~TestServerConnListener() noexcept override = default;

      void OnDataFromClient(const std::string &clientId, const std::string &data) override
      {
         logger_->debug("[{}] {} [{}]", __func__, bs::toHex(clientId), data.size());
         data_.set_value(std::make_pair(clientId, data));
      }
      void onClientError(const std::string &clientId, ClientError error, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, bs::toHex(clientId));
      }
      void OnClientConnected(const std::string &clientId, const Details &details) override
      {
         logger_->debug("[{}] {}", __func__, bs::toHex(clientId));
         connected_.set_value(clientId);
      }
      void OnClientDisconnected(const std::string &clientId) override
      {
         logger_->debug("[{}] {}", __func__, bs::toHex(clientId));
         disconnected_.set_value(clientId);
      }

      std::shared_ptr<spdlog::logger>  logger_;
      std::promise<std::string> connected_;
      std::promise<std::string> disconnected_;
      std::promise<std::pair<std::string, std::string>> data_;
   };


   class TestClientConnListener : public DataConnectionListener
   {
   public:
      TestClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : logger_(logger) {}

      void OnDataReceived(const std::string &data) override
      {
         logger_->debug("[{}] {} bytes", __func__, data.size());
         data_.set_value(data);
      }
      void OnConnected() override
      {
         logger_->debug("[{}]", __func__);
         connected_.set_value();
      }
      void OnDisconnected() override
      {
         logger_->debug("[{}]", __func__);
         disconnected_.set_value();
      }
      void OnError(DataConnectionError errorCode) override
      {
         logger_->debug("[{}] {}", __func__, (int)errorCode);
         error_.set_value(errorCode);
      }

      std::shared_ptr<spdlog::logger>  logger_;
      std::promise<void> connected_;
      std::promise<void> disconnected_;
      std::promise<std::string> data_;
      std::promise<DataConnectionError> error_;
   };

   template<class T>
   T getFeature(std::promise<T> &prom)
   {
      auto feature = prom.get_future();
      if (feature.wait_for(1000ms) != std::future_status::ready) {
         throw std::runtime_error("feature wait failed");
      }
      auto result = feature.get();
      prom = {};
      return result;
   }

   void waitFeature(std::promise<void> &prom)
   {
      auto feature = prom.get_future();
      if (feature.wait_for(1000ms) != std::future_status::ready) {
         throw std::runtime_error("feature wait failed");
      }
      feature.get();
      prom = {};
   }
}

class TestWebSocket : public testing::Test
{
public:
   void SetUp()
   {
      //lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, nullptr);
   }

   void TearDown()
   {
      // Revert to default level
      lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, nullptr);
   }

   // Listeners must be declared before connections
   std::unique_ptr<TestServerConnListener> serverListener_;
   std::unique_ptr<TestClientConnListener> clientListener_;
   std::unique_ptr<ServerConnection> server_;
   std::unique_ptr<DataConnection> client_;

   enum class FirstStart
   {
      Server,
      Client,
   };

   enum class FirstStop
   {
      Server,
      Client,
   };

   void doTest(const std::string &serverHost, const std::string &serverPort
      , const std::string &clientHost, const std::string &clientPort
      , FirstStart firstStart
      , std::function<void()> callback = nullptr, FirstStop firstStop = FirstStop::Client)
   {
      serverListener_ = std::make_unique<TestServerConnListener>(StaticLogger::loggerPtr);
      clientListener_ = std::make_unique<TestClientConnListener>(StaticLogger::loggerPtr);
      if (firstStart == FirstStart::Server) {
         ASSERT_TRUE(server_->BindConnection(serverHost, serverPort, serverListener_.get()));
         ASSERT_TRUE(client_->openConnection(clientHost, clientPort, clientListener_.get()));
      } else {
         ASSERT_TRUE(client_->openConnection(clientHost, clientPort, clientListener_.get()));
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         ASSERT_TRUE(server_->BindConnection(serverHost, serverPort, serverListener_.get()));
      }

      waitFeature(clientListener_->connected_);
      auto clientId = getFeature(serverListener_->connected_);

      for (int i = 0; i < 5; ++i) {
         {
            auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
            ASSERT_TRUE(client_->send(packet));
            auto data = getFeature(serverListener_->data_);
            ASSERT_EQ(data.first, clientId);
            ASSERT_EQ(data.second, packet);
         }
         {
            auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
            ASSERT_TRUE(server_->SendDataToClient(clientId, packet));
            auto data = getFeature(clientListener_->data_);
            ASSERT_EQ(data, packet);
         }
      }

      if (callback) {
         callback();
      }

      for (int i = 0; i < 5; ++i) {
         {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
            ASSERT_TRUE(server_->SendDataToAllClients(packet));
            auto data = getFeature(clientListener_->data_);
            ASSERT_EQ(data, packet);
         }

         {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
            ASSERT_TRUE(client_->send(packet));
            auto data = getFeature(serverListener_->data_);
            ASSERT_EQ(data.first, clientId);
            ASSERT_EQ(data.second, packet);
         }
      }
      if (firstStop == FirstStop::Client) {
         client_.reset();
         ASSERT_EQ(clientId, getFeature(serverListener_->disconnected_));
      } else {
         server_.reset();
         waitFeature(clientListener_->disconnected_);
      }
   }
};

static bs::network::BIP15xParams getTestParams()
{
   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;
   return params;
}

static bs::network::TransportBIP15xServer::TrustedClientsCallback getEmptyPeersCallback()
{
   return []() {
      return bs::network::BIP15xPeers();
   };
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


TEST_F(TestWebSocket, Basic)
{
   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, ClientStartsFirst)
{
   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Client);
}

TEST_F(TestWebSocket, ServerStopsFirst)
{
   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});
   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server, nullptr, FirstStop::Server);
}

TEST_F(TestWebSocket, BindFailed)
{
   serverListener_ = std::make_unique<TestServerConnListener>(StaticLogger::loggerPtr);
   auto server1 = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto server2 = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   ASSERT_TRUE(server1->BindConnection(kTestTcpHost, kTestTcpPort, serverListener_.get()));
   ASSERT_FALSE(server2->BindConnection(kTestTcpHost, kTestTcpPort, serverListener_.get()));
}


// Disabled because test requires socat installed
TEST_F(TestWebSocket, DISABLE_Restart)
{
   int clientPort = 19501;
   int serverPort = 19502;
   auto proxy = std::make_unique<TestTcpProxyProcess>(clientPort, serverPort);

   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   auto callback = [&] {
      proxy.reset();
      proxy = std::make_unique<TestTcpProxyProcess>(clientPort, serverPort);
   };

   doTest(kTestTcpHost, std::to_string(serverPort), kTestTcpHost, std::to_string(clientPort), FirstStart::Server, callback);
}

TEST_F(TestWebSocket, Router)
{
   RouterServerConnectionParams::Server server1;
   server1.host = kTestTcpHost;
   server1.port = kTestTcpPort;
   server1.server = std::make_shared<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});

   RouterServerConnectionParams routerServerParams;
   routerServerParams.servers.push_back(std::move(server1));
   server_ = std::make_unique<RouterServerConnection>(StaticLogger::loggerPtr, routerServerParams);

   client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   // RouterServerConnection ignores host and port used to bind
   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, Bip15X)
{
   auto server = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto client = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   auto srvTransport = std::make_unique<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, getEmptyPeersCallback());
   auto clientTransport = std::make_unique<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());

   srvTransport->addAuthPeer(getPeerKey("client", clientTransport.get()));
   clientTransport->addAuthPeer(getPeerKey(kTestTcpHost, kTestTcpPort, srvTransport.get()));

   server_ = std::make_unique<Bip15xServerConnection>(StaticLogger::loggerPtr, std::move(server), std::move(srvTransport));
   client_ = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(client), std::move(clientTransport));

   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST(WebSocketHelpers, WsPacket)
{
   auto cookie = "<COOKIE>";
   auto data = "<DATA>";
   uint64_t recvCounter = 1234567890;

   auto parsePacket = [](WsRawPacket p) -> WsPacket {
      return WsPacket::parsePacket(std::string(p.getPtr(), p.getPtr() + p.getSize()), StaticLogger::loggerPtr);
   };

   auto packet = parsePacket(WsPacket::requestNew());
   EXPECT_EQ(packet.type, WsPacket::Type::RequestNew);

   packet = parsePacket(WsPacket::requestResumed(cookie, recvCounter));
   EXPECT_EQ(packet.type, WsPacket::Type::RequestResumed);
   EXPECT_EQ(packet.payload, cookie);
   EXPECT_EQ(packet.recvCounter, recvCounter);

   packet = parsePacket(WsPacket::responseNew(cookie));
   EXPECT_EQ(packet.type, WsPacket::Type::ResponseNew);
   EXPECT_EQ(packet.payload, cookie);

   packet = parsePacket(WsPacket::responseResumed(recvCounter));
   EXPECT_EQ(packet.type, WsPacket::Type::ResponseResumed);
   EXPECT_EQ(packet.recvCounter, recvCounter);

   packet = parsePacket(WsPacket::responseUnknown());
   EXPECT_EQ(packet.type, WsPacket::Type::ResponseUnknown);

   packet = parsePacket(WsPacket::data(data));
   EXPECT_EQ(packet.type, WsPacket::Type::Data);
   EXPECT_EQ(packet.payload, data);

   packet = parsePacket(WsPacket::ack(recvCounter));
   EXPECT_EQ(packet.type, WsPacket::Type::Ack);
   EXPECT_EQ(packet.recvCounter, recvCounter);
}

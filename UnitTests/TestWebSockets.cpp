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

#include "DataConnectionListener.h"
#include "RouterServerConnection.h"
#include "ServerConnectionListener.h"
#include "TestEnv.h"
#include "WsDataConnection.h"
#include "WsServerConnection.h"

using namespace std::chrono_literals;

namespace  {

   const auto kTestTcpHost = "127.0.0.1";
   const auto kTestTcpPort = "54361";

   class TestServerConnListener : public ServerConnectionListener
   {
   public:
      TestServerConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : logger_(logger) {}
      ~TestServerConnListener() noexcept override = default;

      void OnDataFromClient(const std::string &clientId, const std::string &data) override {
         data_.set_value(std::make_pair(clientId, data));
      }
      void onClientError(const std::string &clientId, const std::string &errStr) override {
      }
      void OnClientConnected(const std::string &clientId) override {
         connected_.set_value(clientId);
      }
      void OnClientDisconnected(const std::string &clientId) override {
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

      void OnDataReceived(const std::string &data) override {
         data_.set_value(data);
      }
      void OnConnected() override {
         connected_.set_value();
      }
      void OnDisconnected() override {
         disconnected_.set_value();
      }
      void OnError(DataConnectionError errorCode) override {
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
   void SetUp()
   {
      //lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, nullptr);
   }

   void TearDown()
   {
      // Revert to default level
      lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, nullptr);
   }

public:
   SecureBinaryData passphrase_;
   std::shared_ptr<TestEnv> envPtr_;
   std::string walletFolder_;
};

void doTest(std::shared_ptr<ServerConnection> server, std::shared_ptr<DataConnection> client
   , const std::string &serverHost, const std::string &serverPort
   , const std::string &clientHost, const std::string &clientPort)
{
  TestServerConnListener serverListener(StaticLogger::loggerPtr);
  TestClientConnListener clientListener(StaticLogger::loggerPtr);

  ASSERT_TRUE(server->BindConnection(serverHost, serverPort, &serverListener));
  ASSERT_TRUE(client->openConnection(clientHost, clientPort, &clientListener));

  waitFeature(clientListener.connected_);
  auto clientId = getFeature(serverListener.connected_);

  for (int i = 0; i < 5; ++i) {
     {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
        ASSERT_TRUE(client->send(packet));
        auto data = getFeature(serverListener.data_);
        ASSERT_EQ(data.first, clientId);
        ASSERT_EQ(data.second, packet);
     }

     {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
        ASSERT_TRUE(server->SendDataToClient(clientId, packet));
        auto data = getFeature(clientListener.data_);
        ASSERT_EQ(data, packet);
     }
  }

  for (int i = 0; i < 5; ++i) {
     {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
        ASSERT_TRUE(server->SendDataToAllClients(packet));
        auto data = getFeature(clientListener.data_);
        ASSERT_EQ(data, packet);
     }

     {  auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
        ASSERT_TRUE(client->send(packet));
        auto data = getFeature(serverListener.data_);
        ASSERT_EQ(data.first, clientId);
        ASSERT_EQ(data.second, packet);
     }
  }
  client.reset();
  ASSERT_EQ(clientId, getFeature(serverListener.disconnected_));
  server.reset();
}

TEST_F(TestWebSocket, Basic)
{

   WsServerConnectionParams serverParams;
   auto server = std::make_shared<WsServerConnection>(StaticLogger::loggerPtr, serverParams);

   WsDataConnectionParams clientParams;
   auto client = std::make_shared<WsDataConnection>(StaticLogger::loggerPtr, clientParams);

   doTest(std::move(server), std::move(client), kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort);
}

TEST_F(TestWebSocket, Router)
{
   WsServerConnectionParams serverParams;

   RouterServerConnectionParams::Server server1;
   server1.host = kTestTcpHost;
   server1.port = kTestTcpPort;
   server1.server = std::make_shared<WsServerConnection>(StaticLogger::loggerPtr, serverParams);

   RouterServerConnectionParams routerServerParams;
   routerServerParams.servers.push_back(std::move(server1));
   auto server = std::make_shared<RouterServerConnection>(StaticLogger::loggerPtr, routerServerParams);

   WsDataConnectionParams clientParams;
   auto client = std::make_shared<WsDataConnection>(StaticLogger::loggerPtr, clientParams);

   // RouterServerConnection ignores host and port used to bind
   doTest(std::move(server), std::move(client), "", "", kTestTcpHost, kTestTcpPort);
}

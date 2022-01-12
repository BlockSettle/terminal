/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
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
#include "RetryingDataConnection.h"
#include "RouterServerConnection.h"
#include "ServerConnectionListener.h"
#include "SslDataConnection.h"
#include "SslServerConnection.h"
#include "StringUtils.h"
#include "TestEnv.h"
#include "ThreadSafeClasses.h"
#include "TransportBIP15x.h"
#include "TransportBIP15xServer.h"
#include "WsDataConnection.h"
#include "WsServerConnection.h"
#include "FutureValue.h"

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
      args.push_back(QStringLiteral("TCP-LISTEN:%1,reuseaddr,fork").arg(listenPort));
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
         data_.push_back(std::make_pair(clientId, data));
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
      ArmoryThreading::TimedQueue<std::pair<std::string, std::string>> data_;
   };

   class TestClientConnListener : public DataConnectionListener
   {
   public:
      TestClientConnListener(const std::shared_ptr<spdlog::logger> &logger)
         : logger_(logger) {}

      void OnDataReceived(const std::string &data) override
      {
         logger_->debug("[{}] {} bytes", __func__, data.size());
         data_.push_back(std::string(data));
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
      ArmoryThreading::TimedQueue<std::string> data_;
      std::promise<DataConnectionError> error_;
   };

   class WsDataConnectionBroken : public WsDataConnection
   {
   public:
      WsPacket::Type typeToMangle_{};

      WsDataConnectionBroken(WsPacket::Type typeToMangle)
         : WsDataConnection(StaticLogger::loggerPtr, WsDataConnectionParams{})
         , typeToMangle_(typeToMangle)
      {
      }

      WsRawPacket filterRawPacket(WsRawPacket rawPacket) override
      {
         auto packet = bs::network::WsPacket::parsePacket(
            std::string(rawPacket.getPtr(), rawPacket.getPtr() + rawPacket.getSize()), StaticLogger::loggerPtr);
         assert(packet.type != WsPacket::Type::Invalid);
         if (packet.type != typeToMangle_) {
            return rawPacket;
         }
         return WsRawPacket(CryptoPRNG::generateRandom(8).toBinStr());
      }
   };

   const auto kDefaultTimeout = 10000ms;

   template<class T>
   T getFeature(ArmoryThreading::TimedQueue<T> &data, std::chrono::milliseconds timeout = kDefaultTimeout)
   {
      return data.pop_front(timeout);
   }

   template<class T>
   T getFeature(std::promise<T> &prom, std::chrono::milliseconds timeout = kDefaultTimeout)
   {
      auto feature = prom.get_future();
      if (feature.wait_for(timeout) != std::future_status::ready) {
         SPDLOG_LOGGER_ERROR(StaticLogger::loggerPtr, "feature wait failed");
         throw std::runtime_error("feature wait failed");
      }
      auto result = feature.get();
      prom = {};
      return result;
   }

   void waitFeature(std::promise<void> &prom, std::chrono::milliseconds timeout = kDefaultTimeout)
   {
      auto feature = prom.get_future();
      if (feature.wait_for(timeout) != std::future_status::ready) {
         SPDLOG_LOGGER_ERROR(StaticLogger::loggerPtr, "void feature wait failed");
         throw std::runtime_error("void feature wait failed");
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

static bs::network::TransportBIP15xServer::TrustedClientsCallback constructTrustedPeersCallback(
   const bs::network::BIP15xPeers& peers)
{
   return [peers] () {
      return peers;
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

TEST_F(TestWebSocket, BrokenConnect)
{
   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnectionBroken>(WsPacket::Type::RequestNew);
   serverListener_ = std::make_unique<TestServerConnListener>(StaticLogger::loggerPtr);
   clientListener_ = std::make_unique<TestClientConnListener>(StaticLogger::loggerPtr);

   ASSERT_TRUE(server_->BindConnection(kTestTcpHost, kTestTcpPort, serverListener_.get()));
   ASSERT_TRUE(client_->openConnection(kTestTcpHost, kTestTcpPort, clientListener_.get()));

   ASSERT_THROW(waitFeature(clientListener_->connected_, 100ms), std::runtime_error);
}

TEST_F(TestWebSocket, BrokenData)
{
   server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   client_ = std::make_unique<WsDataConnectionBroken>(WsPacket::Type::Data);
   serverListener_ = std::make_unique<TestServerConnListener>(StaticLogger::loggerPtr);
   clientListener_ = std::make_unique<TestClientConnListener>(StaticLogger::loggerPtr);

   ASSERT_TRUE(server_->BindConnection(kTestTcpHost, kTestTcpPort, serverListener_.get()));
   ASSERT_TRUE(client_->openConnection(kTestTcpHost, kTestTcpPort, clientListener_.get()));

   waitFeature(clientListener_->connected_);
   auto clientId = getFeature(serverListener_->connected_);

   auto packet = CryptoPRNG::generateRandom(rand() % 10000).toBinStr();
   ASSERT_TRUE(client_->send(packet));
   ASSERT_THROW(getFeature(serverListener_->data_, 100ms), ArmoryThreading::StackTimedOutException);
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

TEST_F(TestWebSocket, DISABLED_StressTest)
{
   auto logger = StaticLogger::loggerPtr;
   auto clientPort = 19501;
   auto serverPort = 19502;

   std::thread([&] {
      while (true) {
         TestTcpProxyProcess proxy(clientPort, serverPort);
         std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
      }
   }).detach();

   while (true) {
      WsDataConnectionParams clientParams;
      clientParams.delaysTableMs = std::vector<uint32_t>(1000, 10);
      WsServerConnectionParams serverParams;
      serverParams.clientTimeout = std::chrono::seconds(5);
      server_ = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, serverParams);
      client_ = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, clientParams);
      serverListener_ = std::make_unique<TestServerConnListener>(StaticLogger::loggerPtr);
      clientListener_ = std::make_unique<TestClientConnListener>(StaticLogger::loggerPtr);
      ASSERT_TRUE(server_->BindConnection(kTestTcpHost, std::to_string(serverPort), serverListener_.get()));
      ASSERT_TRUE(client_->openConnection(kTestTcpHost, std::to_string(clientPort), clientListener_.get()));

      SPDLOG_LOGGER_DEBUG(logger, "wait connected on client...");
      waitFeature(clientListener_->connected_);
      SPDLOG_LOGGER_DEBUG(logger, "wait connected on server...");
      auto clientId = getFeature(serverListener_->connected_);

      for (int i = 0; i < 200; ++i) {
         std::vector<std::string> clientPackets;
         std::vector<std::string> serverPackets;
         int clientPacketCount = rand() % 10;
         int serverPacketCount = rand() % 10;

         for (int i = 0; i < clientPacketCount; ++i) {
            clientPackets.push_back(std::string(size_t(rand() % 1000 + 1), 'a'));
            ASSERT_TRUE(client_->send(clientPackets.back()));
         }
         for (int i = 0; i < serverPacketCount; ++i) {
            serverPackets.push_back(std::string(size_t(rand() % 1000 + 1), 'b'));
            ASSERT_TRUE(server_->SendDataToClient(clientId, serverPackets.back()));
         }

         for (const auto &clientPacket : clientPackets) {
            auto data = getFeature(serverListener_->data_);
            SPDLOG_LOGGER_DEBUG(logger, "wait data on server...");
            ASSERT_EQ(data.first, clientId);
            ASSERT_EQ(data.second, clientPacket);
         }
         for (const auto &serverPacket : serverPackets) {
            SPDLOG_LOGGER_DEBUG(logger, "wait data on client...");
            auto data = getFeature(clientListener_->data_);
            ASSERT_EQ(data, serverPacket);
         }
      }

      client_.reset();
      SPDLOG_LOGGER_DEBUG(logger, "wait diconnected on server...");
      ASSERT_EQ(clientId, getFeature(serverListener_->disconnected_));
      server_.reset();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
}

// Disabled because test requires socat installed
TEST_F(TestWebSocket, DISABLED_Restart)
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

TEST_F(TestWebSocket, Bip15X_1Way)
{
   auto server = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto client = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   auto params = getTestParams();
   params.authMode = bs::network::BIP15xAuthMode::OneWay;
   auto srvTransport = std::make_unique<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, getEmptyPeersCallback(), bs::network::BIP15xAuthMode::OneWay);
   auto clientTransport = std::make_unique<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, params);

   clientTransport->addAuthPeer(getPeerKey(kTestTcpHost, kTestTcpPort, srvTransport.get()));

   server_ = std::make_unique<Bip15xServerConnection>(StaticLogger::loggerPtr, std::move(server), std::move(srvTransport));
   client_ = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(client), std::move(clientTransport));

   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, Bip15X_1Way_Announce)
{
   auto server = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto client = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   auto params = getTestParams();
   params.authMode = bs::network::BIP15xAuthMode::OneWay;
   auto srvTransport = std::make_unique<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, getEmptyPeersCallback(), bs::network::BIP15xAuthMode::OneWay);
   auto clientTransport = std::make_unique<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, params);

   auto serverKey = getPeerKey(kTestTcpHost, kTestTcpPort, srvTransport.get());
   auto newKeyCb = [serverKey](const std::string& oldKey
      , const std::string& newKeyHex, const std::string& srvAddrPort
      , const std::shared_ptr<FutureValue<bool>> &newKeyProm)
   {
      EXPECT_EQ(srvAddrPort, serverKey.name());
      
      auto newKey = BinaryData::CreateFromHex(newKeyHex);
      EXPECT_EQ(newKey, serverKey.pubKey());

      newKeyProm->setValue(true);
   };
   clientTransport->setKeyCb(newKeyCb);

   server_ = std::make_unique<Bip15xServerConnection>(StaticLogger::loggerPtr, std::move(server), std::move(srvTransport));
   client_ = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(client), std::move(clientTransport));

   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, Bip15X_2Way)
{
   auto server = std::make_unique<WsServerConnection>(StaticLogger::loggerPtr, WsServerConnectionParams{});
   auto client = std::make_unique<WsDataConnection>(StaticLogger::loggerPtr, WsDataConnectionParams{});

   auto clientTransport = std::make_unique<bs::network::TransportBIP15xClient>(
      StaticLogger::loggerPtr, getTestParams());
      
   auto clientKey = getPeerKey("client", clientTransport.get());
   auto serverPeersCb = constructTrustedPeersCallback({clientKey});
   auto srvTransport = std::make_unique<bs::network::TransportBIP15xServer>(
      StaticLogger::loggerPtr, serverPeersCb, bs::network::BIP15xAuthMode::TwoWay);

   clientTransport->addAuthPeer(getPeerKey(kTestTcpHost, kTestTcpPort, srvTransport.get()));

   server_ = std::make_unique<Bip15xServerConnection>(StaticLogger::loggerPtr, std::move(server), std::move(srvTransport));
   client_ = std::make_unique<Bip15xDataConnection>(StaticLogger::loggerPtr, std::move(client), std::move(clientTransport));

   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, SslConnectionPlain)
{
   server_ = std::make_unique<SslServerConnection>(StaticLogger::loggerPtr, SslServerConnectionParams{});
   client_ = std::make_unique<SslDataConnection>(StaticLogger::loggerPtr, SslDataConnectionParams{});
   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);
}

TEST_F(TestWebSocket, SslConnectionSelfSigned)
{
   auto privKeyServer = bs::network::ws::generatePrivKey();
   auto certServer = bs::network::ws::generateSelfSignedCert(privKeyServer);
   auto pubKeyServer = bs::network::ws::publicKey(privKeyServer);
   SPDLOG_LOGGER_DEBUG(StaticLogger::loggerPtr, "server public key: {}", bs::toHex(pubKeyServer));

   auto privKeyClient = bs::network::ws::generatePrivKey();
   auto certClient = bs::network::ws::generateSelfSignedCert(privKeyClient);
   auto pubKeyClient = bs::network::ws::publicKey(privKeyClient);
   SPDLOG_LOGGER_DEBUG(StaticLogger::loggerPtr, "client public key: {}", bs::toHex(pubKeyClient));

   bool serverPubKeyValid = false;
   bool clientPubKeyValid = false;

   SslServerConnectionParams serverParams;
   serverParams.useSsl = true;
   serverParams.requireClientCert = true;
   serverParams.cert = certServer;
   serverParams.privKey = privKeyServer;
   serverParams.verifyCallback = [&clientPubKeyValid, pubKeyClient](const std::string &pubKey) -> bool {
      SPDLOG_LOGGER_DEBUG(StaticLogger::loggerPtr, "client public key: {}", bs::toHex(pubKey));
      clientPubKeyValid = pubKeyClient == pubKey;
      return clientPubKeyValid;
   };

   SslDataConnectionParams clientParams;
   clientParams.useSsl = true;
   clientParams.cert = certClient;
   clientParams.privKey = privKeyClient;
   clientParams.allowSelfSigned = true;
   clientParams.skipHostNameChecks = true;
   clientParams.verifyCallback = [&serverPubKeyValid, pubKeyServer](const std::string &pubKey) -> bool {
      SPDLOG_LOGGER_DEBUG(StaticLogger::loggerPtr, "server public key: {}", bs::toHex(pubKey));
      serverPubKeyValid = pubKeyServer == pubKey;
      return serverPubKeyValid;
   };

   client_ = std::make_unique<SslDataConnection>(StaticLogger::loggerPtr, clientParams);
   server_ = std::make_unique<SslServerConnection>(StaticLogger::loggerPtr, serverParams);

   doTest(kTestTcpHost, kTestTcpPort, kTestTcpHost, kTestTcpPort, FirstStart::Server);

   EXPECT_TRUE(serverPubKeyValid);
   EXPECT_TRUE(clientPubKeyValid);
}

TEST_F(TestWebSocket, RetryingDataConnection)
{
   server_ = std::make_unique<SslServerConnection>(StaticLogger::loggerPtr, SslServerConnectionParams{});

   RetryingDataConnectionParams clientParams;
   clientParams.connection = std::make_unique<SslDataConnection>(StaticLogger::loggerPtr, SslDataConnectionParams{});
   client_ = std::make_unique<RetryingDataConnection>(StaticLogger::loggerPtr, std::move(clientParams));

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

TEST(WebSocketHelpers, Split)
{
   EXPECT_EQ(bs::split("a", ','), std::vector<std::string>({"a"}));
   EXPECT_EQ(bs::split("a,b", ','), std::vector<std::string>({"a", "b"}));
   EXPECT_EQ(bs::split("", ','), std::vector<std::string>({""}));
   EXPECT_EQ(bs::split("a,", ','), std::vector<std::string>({"a", ""}));
   EXPECT_EQ(bs::split(",b", ','), std::vector<std::string>({"", "b"}));
   EXPECT_EQ(bs::split(",", ','), std::vector<std::string>({"", ""}));
}

TEST(WebSocketHelpers, Trim)
{
   EXPECT_EQ(bs::trim("a"), "a");
   EXPECT_EQ(bs::trim(""), "");
   EXPECT_EQ(bs::trim(" "), "");
   EXPECT_EQ(bs::trim("   "), "");
   EXPECT_EQ(bs::trim(" a"), "a");
   EXPECT_EQ(bs::trim("a "), "a");
   EXPECT_EQ(bs::trim(" a a "), "a a");
   EXPECT_EQ(bs::trim("  a  "), "a");
}

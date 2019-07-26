#include "ConnectionManager.h"
#include <QNetworkAccessManager>
#include "CelerClientConnection.h"
#include "CelerStreamServerConnection.h"
#include "GenoaConnection.h"
#include "GenoaStreamServerConnection.h"
#include "PublisherConnection.h"
#include "SubscriberConnection.h"
#include "ZmqContext.h"
#include "ZmqDataConnection.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_ServerConnection.h"

#ifdef Q_OS_WIN
   #include <Winsock2.h>
#endif

#include <zmq.h>
#include <spdlog/spdlog.h>

ConnectionManager::ConnectionManager(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
{
   // init network
   isInitialized_ = InitNetworkLibs();
}

ConnectionManager::ConnectionManager(const std::shared_ptr<spdlog::logger>& logger
   , const ZmqBIP15XPeers &zmqTrustedTerminals)
   : logger_(logger), zmqTrustedTerminals_(zmqTrustedTerminals)
{
   // init network
   isInitialized_ = InitNetworkLibs();
}

ConnectionManager::ConnectionManager(const std::shared_ptr<spdlog::logger>& logger
   , std::shared_ptr<ArmoryServersProvider> armoryServers)
   : logger_(logger), armoryServers_(armoryServers)
{
   // init network
   isInitialized_ = InitNetworkLibs();
}

bool ConnectionManager::InitNetworkLibs()
{
#ifdef Q_OS_WIN
   WORD  wVersion;
   WSADATA wsaData;
   int err;

   wVersion = MAKEWORD(2,0);
   err = WSAStartup( wVersion, &wsaData );
   if (err) {
      return false;
   }
#endif

   zmqContext_ = std::make_shared<ZmqContext>(logger_);

   return true;
}

void ConnectionManager::DeinitNetworkLibs()
{
#ifdef Q_OS_WIN
   WSACleanup();
#endif
}

ConnectionManager::~ConnectionManager() noexcept
{
   DeinitNetworkLibs();
}

std::shared_ptr<spdlog::logger> ConnectionManager::GetLogger() const
{
   return logger_;
}

std::shared_ptr<ServerConnection> ConnectionManager::CreateGenoaAPIServerConnection() const
{
   return std::make_shared<GenoaStreamServerConnection>(logger_, zmqContext_);
}

std::shared_ptr<ServerConnection> ConnectionManager::CreateCelerAPIServerConnection() const
{
   return std::make_shared<CelerStreamServerConnection>(logger_, zmqContext_);
}

std::shared_ptr<DataConnection> ConnectionManager::CreateCelerClientConnection() const
{
   auto connection = std::make_shared< CelerClientConnection<ZmqDataConnection> >(logger_);
   connection->SetContext(zmqContext_);

   return connection;
}

std::shared_ptr<DataConnection> ConnectionManager::CreateGenoaClientConnection(bool monitored) const
{
   auto connection = std::make_shared< GenoaConnection<ZmqDataConnection> >(logger_, monitored);
   connection->SetContext(zmqContext_);

   return connection;
}

std::shared_ptr<ZmqBIP15XServerConnection> ConnectionManager::CreateZMQBIP15XChatServerConnection(
   bool ephemeral, const std::string& ownKeyFileDir, const std::string& ownKeyFileName) const
{
   auto cbTrustedClients = [this]() {
      return zmqTrustedTerminals_;
   };

   return std::make_shared<ZmqBIP15XServerConnection>(logger_, zmqContext_
      , cbTrustedClients, ephemeral
      , ownKeyFileDir, ownKeyFileName, false);
}

ZmqBIP15XDataConnectionPtr ConnectionManager::CreateZMQBIP15XDataConnection(const ZmqBIP15XDataConnectionParams &params) const
{
   auto connection = std::make_shared<ZmqBIP15XDataConnection>(logger_, params);
   return connection;
}

ZmqBIP15XDataConnectionPtr ConnectionManager::CreateZMQBIP15XDataConnection() const
{
   ZmqBIP15XDataConnectionParams params;
   params.ephemeralPeers = true;
   return CreateZMQBIP15XDataConnection(params);
}

std::shared_ptr<ServerConnection> ConnectionManager::CreatePubBridgeServerConnection() const
{
   return std::make_shared<GenoaStreamServerConnection>(logger_, zmqContext_);
}

// MD will be sent as HTTP packet
// each genoa message ( send or received ) ends with double CRLF.
std::shared_ptr<ServerConnection> ConnectionManager::CreateMDRestServerConnection() const
{
   return std::make_shared<GenoaStreamServerConnection>(logger_, zmqContext_);
}

std::shared_ptr<PublisherConnection> ConnectionManager::CreatePublisherConnection() const
{
   return std::make_shared<PublisherConnection>(logger_, zmqContext_);
}

std::shared_ptr<SubscriberConnection> ConnectionManager::CreateSubscriberConnection() const
{
   return std::make_shared<SubscriberConnection>(logger_, zmqContext_);
}

const std::shared_ptr<QNetworkAccessManager> &ConnectionManager::GetNAM()
{
   if (!nam_) {
      nam_.reset(new QNetworkAccessManager);
   }

   return nam_;
}

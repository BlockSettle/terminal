#include "ConnectionManager.h"

#include "CelerClientConnection.h"
#include "CelerStreamServerConnection.h"
#include "GenoaConnection.h"
#include "GenoaStreamServerConnection.h"
#include "PublisherConnection.h"
#include "SubscriberConnection.h"
#include "ZmqContext.h"
#include "ZmqDataConnection.h"
#include "ZmqSecuredDataConnection.h"
#include "ZmqSecuredServerConnection.h"

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

std::shared_ptr<ZmqSecuredServerConnection> ConnectionManager::CreateSecuredServerConnection() const
{
   return std::make_shared<ZmqSecuredServerConnection>(logger_, zmqContext_);
}

std::shared_ptr<ZmqSecuredDataConnection> ConnectionManager::CreateSecuredDataConnection(bool monitored) const
{
   auto connection = std::make_shared<ZmqSecuredDataConnection>(logger_, monitored);
   connection->SetContext(zmqContext_);

   return connection;
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

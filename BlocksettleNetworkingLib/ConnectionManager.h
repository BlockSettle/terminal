#ifndef __CONNECTION_MANAGER_H__
#define __CONNECTION_MANAGER_H__

#include <memory>

namespace spdlog {
   class logger;
};

class DataConnection;
class PublisherConnection;
class ServerConnection;
class SubscriberConnection;
class ZmqContext;
class ZmqSecuredDataConnection;
class ZmqSecuredServerConnection;

class ConnectionManager
{
public:
   ConnectionManager(const std::shared_ptr<spdlog::logger>& logger);
   ~ConnectionManager() noexcept;

   ConnectionManager(const ConnectionManager&) = delete;
   ConnectionManager& operator = (const ConnectionManager&) = delete;
   ConnectionManager(ConnectionManager&&) = delete;
   ConnectionManager& operator = (ConnectionManager&&) = delete;

   bool IsInitialized() const { return isInitialized_; }

   std::shared_ptr<spdlog::logger>     GetLogger() const;

   std::shared_ptr<ServerConnection>   CreateGenoaAPIServerConnection() const;
   std::shared_ptr<ServerConnection>   CreateCelerAPIServerConnection() const;

   std::shared_ptr<DataConnection>     CreateCelerClientConnection() const;
   std::shared_ptr<DataConnection>     CreateGenoaClientConnection(bool monitored = false) const;

   std::shared_ptr<ZmqSecuredServerConnection>  CreateSecuredServerConnection() const;
   std::shared_ptr<ZmqSecuredDataConnection>    CreateSecuredDataConnection(bool monitored = false) const;

   std::shared_ptr<ServerConnection>   CreatePubBridgeServerConnection() const;

   std::shared_ptr<ServerConnection>   CreateMDRestServerConnection() const;

   std::shared_ptr<PublisherConnection>   CreatePublisherConnection() const;
   std::shared_ptr<SubscriberConnection>  CreateSubscriberConnection() const;

private:
   bool InitNetworkLibs();
   void DeinitNetworkLibs();
private:
   bool isInitialized_;

   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqContext>      zmqContext_;
};

#endif // __CONNECTION_MANAGER_H__

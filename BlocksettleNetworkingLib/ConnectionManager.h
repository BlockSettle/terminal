#ifndef __CONNECTION_MANAGER_H__
#define __CONNECTION_MANAGER_H__

#include <memory>
#include <vector>
#include <string>
#include "ZMQ_BIP15X_Helpers.h"

namespace spdlog {
   class logger;
};

struct ZmqBIP15XDataConnectionParams;
class ArmoryServersProvider;
class DataConnection;
class PublisherConnection;
class ServerConnection;
class SubscriberConnection;
class ZmqContext;
class QNetworkAccessManager;
class ZmqBIP15XDataConnection;
class ZmqBIP15XServerConnection;

class ConnectionManager
{
public:
   ConnectionManager(const std::shared_ptr<spdlog::logger>& logger);
   ConnectionManager(const std::shared_ptr<spdlog::logger>& logger
      , const ZmqBIP15XPeers &zmqTrustedTerminals);
   ConnectionManager(const std::shared_ptr<spdlog::logger>& logger
      , std::shared_ptr<ArmoryServersProvider> armoryServers);
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
   std::shared_ptr<DataConnection>     CreateGenoaClientConnection(
      bool monitored = false) const;

   ZmqBIP15XDataConnectionPtr   CreateZMQBIP15XDataConnection(
         const ZmqBIP15XDataConnectionParams &params) const;
   ZmqBIP15XDataConnectionPtr   CreateZMQBIP15XDataConnection() const;
   std::shared_ptr<ZmqBIP15XServerConnection> CreateZMQBIP15XChatServerConnection(
      bool ephemeral = false, const std::string& ownKeyFileDir = ""
      , const std::string& ownKeyFileName = "") const;

   std::shared_ptr<ServerConnection>   CreatePubBridgeServerConnection() const;

   std::shared_ptr<ServerConnection>   CreateMDRestServerConnection() const;

   std::shared_ptr<PublisherConnection>   CreatePublisherConnection() const;
   std::shared_ptr<SubscriberConnection>  CreateSubscriberConnection() const;

   const std::shared_ptr<QNetworkAccessManager> &GetNAM();

private:
   bool InitNetworkLibs();
   void DeinitNetworkLibs();
private:
   bool isInitialized_;

   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ZmqContext>            zmqContext_;
   std::shared_ptr<QNetworkAccessManager> nam_;
   std::shared_ptr<ArmoryServersProvider> armoryServers_;
   ZmqBIP15XPeers                         zmqTrustedTerminals_;
};

#endif // __CONNECTION_MANAGER_H__

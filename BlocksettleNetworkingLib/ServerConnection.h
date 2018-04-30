#ifndef __SERVER_CONNECTION_H__
#define __SERVER_CONNECTION_H__

#include "ServerConnectionListener.h"

#include <string>
#include <memory>

class ServerConnection
{
public:
   ServerConnection() = default;
   virtual ~ServerConnection() noexcept = default;

   ServerConnection(const ServerConnection&) = delete;
   ServerConnection& operator = (const ServerConnection&) = delete;

   ServerConnection(ServerConnection&&) = delete;
   ServerConnection& operator = (ServerConnection&&) = delete;

public:
   virtual bool BindConnection(const std::string& host, const std::string& port
      , ServerConnectionListener* listener) = 0;

   virtual std::string GetClientInfo(const std::string &clientId) const = 0;

   virtual bool SendDataToClient(const std::string& clientId, const std::string& data) = 0;
   virtual bool SendDataToAllClients(const std::string& data) { return false; }
};

#endif // __SERVER_CONNECTION_H__
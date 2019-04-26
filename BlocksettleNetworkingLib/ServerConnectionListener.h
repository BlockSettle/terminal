#ifndef __SERVER_CONNECTION_LISTENER_H__
#define __SERVER_CONNECTION_LISTENER_H__

#include <memory>
#include <string>

class ServerConnectionListener
{
public:
   ServerConnectionListener() = default;
   virtual ~ServerConnectionListener() noexcept = default;

   ServerConnectionListener(const ServerConnectionListener&) = delete;
   ServerConnectionListener& operator = (const ServerConnectionListener&) = delete;

   ServerConnectionListener(ServerConnectionListener&&) = delete;
   ServerConnectionListener& operator = (ServerConnectionListener&&) = delete;

public:
   virtual void OnDataFromClient(const std::string& clientId, const std::string& data) = 0;

   virtual void OnClientConnected(const std::string& clientId) = 0;
   virtual void OnClientDisconnected(const std::string& clientId) = 0;

   virtual void OnPeerConnected(const std::string &) {}
   virtual void OnPeerDisconnected(const std::string &) {}

   virtual void onClientError(const std::string &clientId, const std::string &error) {}
};

#endif // __SERVER_CONNECTION_LISTENER_H__

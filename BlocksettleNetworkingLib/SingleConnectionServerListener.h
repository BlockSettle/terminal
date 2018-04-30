#ifndef __SINGLE_CONNECTION_SERVER_LISTENER_H__
#define __SINGLE_CONNECTION_SERVER_LISTENER_H__

#include "ServerConnection.h"
#include "ServerConnectionListener.h"

#include <memory>
#include <string>

namespace spdlog {
   class logger;
};

// server connection listener that make P2P connection possible when you acting
// like a server
class SingleConnectionServerListener : public ServerConnectionListener
{
public:
   SingleConnectionServerListener(const std::shared_ptr<ServerConnection>& connection
      , const std::shared_ptr<spdlog::logger>& logger
      , const std::string& name);
   ~SingleConnectionServerListener() noexcept override = default;

   SingleConnectionServerListener(const SingleConnectionServerListener&) = delete;
   SingleConnectionServerListener& operator = (const SingleConnectionServerListener&) = delete;

   SingleConnectionServerListener(SingleConnectionServerListener&&) = delete;
   SingleConnectionServerListener& operator = (SingleConnectionServerListener&&) = delete;

   bool BindServerConnection(const std::string& host, const std::string& port);

   bool IsConnected() const;

protected:
   virtual void onSingleClientConnected() = 0;
   virtual void onSingleClientDisconnected() = 0;

   virtual bool ProcessDataFromClient(const std::string& data) = 0;
   bool SendDataToClient(const std::string& data);

private:
   void OnDataFromClient(const std::string& clientId, const std::string& data) override;
   void OnClientConnected(const std::string& clientId) override;
   void OnClientDisconnected(const std::string& clientId) override;

protected:
   std::shared_ptr<spdlog::logger>     logger_;
   std::string GetName() const { return name_; }

private:
   std::shared_ptr<ServerConnection>   serverConnection_;
   std::string                         name_;

   std::string connectedClientId_;
};

#endif // __SINGLE_CONNECTION_SERVER_LISTENER_H__

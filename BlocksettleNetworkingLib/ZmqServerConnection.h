#ifndef __ZEROMQ_SERVER_CONNECTION_H__
#define __ZEROMQ_SERVER_CONNECTION_H__

#include "ServerConnection.h"
#include "ZmqContext.h"

#include <atomic>
#include <thread>
#include <unordered_map>

namespace spdlog
{
   class logger;
}

class ZmqServerConnection : public ServerConnection
{
public:
   ZmqServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , bool extraLogging = false);

   ~ZmqServerConnection() noexcept override;

   ZmqServerConnection(const ZmqServerConnection&) = delete;
   ZmqServerConnection& operator = (const ZmqServerConnection&) = delete;

   ZmqServerConnection(ZmqServerConnection&&) = delete;
   ZmqServerConnection& operator = (ZmqServerConnection&&) = delete;

public:
   bool BindConnection(const std::string& host, const std::string& port
      , ServerConnectionListener* listener) override;

   std::string GetClientInfo(const std::string &clientId) const override;

protected:
   bool isActive() const;

   // interface for active connection listener
   void notifyListenerOnData(const std::string& clientId, const std::string& data);

   void notifyListenerOnNewConnection(const std::string& clientId);
   void notifyListenerOnDisconnectedClient(const std::string& clientId);

   virtual ZmqContext::sock_ptr CreateDataSocket() = 0;
   virtual bool ConfigDataSocket(const ZmqContext::sock_ptr& dataSocket) = 0;

   virtual bool ReadFromDataSocket() = 0;

protected:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqContext>      context_;

   std::string                      connectionName_;

   std::atomic_flag                 socketLockFlag_ = ATOMIC_FLAG_INIT;
   ZmqContext::sock_ptr             dataSocket_;
   std::unordered_map<std::string, std::string> clientInfo_;

private:
   void stopServer();

   // run in thread
   void listenFunction();

   enum SocketIndex {
      ControlSocketIndex = 0,
      DataSocketIndex
   };

   enum InternalCommandCode {
      CommandSend = 0,
      CommandStop
   };

private:
   std::thread                      listenThread_;

   ZmqContext::sock_ptr             threadMasterSocket_;
   ZmqContext::sock_ptr             threadSlaveSocket_;

   ServerConnectionListener*        listener_;

   bool extraLogging_;
};

#endif // __ZEROMQ_SERVER_CONNECTION_H__

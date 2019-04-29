#ifndef __SERVER_CONNECTION_H__
#define __SERVER_CONNECTION_H__

#include "ServerConnectionListener.h"

#include <functional>
#include <memory>
#include <string>

class ServerConnection
{
public:
   ServerConnection() = default;
   virtual ~ServerConnection() noexcept = default;

   ServerConnection(const ServerConnection&) = delete;
   ServerConnection& operator = (const ServerConnection&) = delete;

   ServerConnection(ServerConnection&&) = delete;
   ServerConnection& operator = (ServerConnection&&) = delete;

   virtual bool BindConnection(const std::string& host, const std::string& port
      , ServerConnectionListener* listener) = 0;

   virtual std::string GetClientInfo(const std::string &clientId) const = 0;

   using SendResultCb = std::function<void(const std::string &clientId, const std::string &data, bool)>;
   virtual bool SendDataToClient(const std::string& clientId, const std::string& data
      , const SendResultCb &cb = nullptr) = 0;
   virtual bool SendDataToAllClients(const std::string&, const SendResultCb &cb = nullptr) { return false; }

   void callConnAcceptedCB(const std::string& clientID) {
      // If the function doesn't exist yet, catch the error and move on.
      try {
         cbConnAccepted_(clientID);
      }
      catch(const std::bad_function_call& e) {}
   }
   void callConnClosedCB(const std::string& clientID) {
      // If the function doesn't exist yet, catch the error and move on.
      try {
         cbConnClosed_(clientID);
      }
      catch(const std::bad_function_call& e) {}
   }

protected:
   void setConnAcceptedCB(
      const std::function<void(const std::string&)> cbConnAccepted) {
      cbConnAccepted_ = cbConnAccepted;
   }
   void setConnClosedCB(
      const std::function<void(const std::string&)> cbConnClosed) {
      cbConnClosed_ = cbConnClosed;
   }

   std::function<void(const std::string&)> cbConnAccepted_ = nullptr;
   std::function<void(const std::string&)> cbConnClosed_ = nullptr;
};

#endif // __SERVER_CONNECTION_H__

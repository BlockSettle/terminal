#include "SingleConnectionServerListener.h"

#include <spdlog/spdlog.h>

SingleConnectionServerListener::SingleConnectionServerListener(const std::shared_ptr<ServerConnection>& connection
      , const std::shared_ptr<spdlog::logger>& logger, const std::string& name)
 : serverConnection_(connection)
 , logger_(logger)
 , name_(name)
{}

bool SingleConnectionServerListener::BindServerConnection(const std::string& host, const std::string& port)
{
   logger_->debug("[SingleConnectionServerListener::BindServerConnection] {} binding to {} : {}"
      , name_, host, port);

   if (!serverConnection_->BindConnection(host, port, this)) {
      logger_->error("[SingleConnectionServerListener::BindServerConnection] failed to bind {}", name_);
      return false;
   }

   return true;
}

bool SingleConnectionServerListener::IsConnected() const
{
   return !connectedClientId_.empty();
}

bool SingleConnectionServerListener::SendDataToClient(const std::string& data)
{
   if (!IsConnected()) {
      logger_->error("[SingleConnectionServerListener::SendDataToClient] client is not connected to {}", name_);
      return false;
   }

   return serverConnection_->SendDataToClient(connectedClientId_, data);
}

void SingleConnectionServerListener::OnDataFromClient(const std::string& clientId, const std::string& data)
{
   if (!IsConnected()) {
      OnClientConnected(clientId);
   }
   if (clientId != connectedClientId_) {
      logger_->error("[SingleConnectionServerListener::OnDataFromClient] {} get data from secondary connection. Ignore."
         , name_);
   } else {
      ProcessDataFromClient(data);
   }
}

void SingleConnectionServerListener::OnClientConnected(const std::string& clientId)
{
   if (!IsConnected()) {
      connectedClientId_ = clientId;
      onSingleClientConnected();
   } else {
      logger_->debug("[SingleConnectionServerListener::OnClientConnected] already have connection. Ignored");
   }
}

void SingleConnectionServerListener::OnClientDisconnected(const std::string& clientId)
{
   if (IsConnected() && (connectedClientId_ == clientId)) {
      connectedClientId_.clear();
      onSingleClientDisconnected();
   }
}

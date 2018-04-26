#include "ActiveStreamClient.h"

#include "ZmqStreamServerConnection.h"

#include <spdlog/spdlog.h>

ActiveStreamClient::ActiveStreamClient(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
{}

void ActiveStreamClient::InitConnection(const std::string& connectionId, ZmqStreamServerConnection* serverConnection)
{
   connectionId_ = connectionId;
   serverConnection_ = serverConnection;
}

bool ActiveStreamClient::sendRawData(const std::string& data)
{
   return serverConnection_->sendRawData(connectionId_, data);
}

bool ActiveStreamClient::sendRawData(const char* data, size_t size)
{
   return serverConnection_->sendRawData(connectionId_, data, size);
}

void ActiveStreamClient::notifyOnData(const std::string& data)
{
   serverConnection_->notifyListenerOnData(connectionId_, data);
}
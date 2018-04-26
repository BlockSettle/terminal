#include "DataConnection.h"

#include <cassert>

void DataConnection::notifyOnData(const std::string& data)
{
   assert(listener_ != nullptr);
   listener_->OnDataReceived(data);
}

void DataConnection::notifyOnConnected()
{
   assert(listener_ != nullptr);
   listener_->OnConnected();
}

void DataConnection::notifyOnDisconnected()
{
   assert(listener_ != nullptr);
   listener_->OnDisconnected();
}

void DataConnection::notifyOnError(DataConnectionListener::DataConnectionError errorCode)
{
   assert(listener_ != nullptr);
   listener_->OnError(errorCode);
}
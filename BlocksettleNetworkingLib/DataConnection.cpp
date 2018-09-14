#include "DataConnection.h"

#include <cassert>

void DataConnection::detachFromListener()
{
   listener_ = nullptr;
}

void DataConnection::notifyOnData(const std::string& data)
{
   if (listener_) {
      listener_->OnDataReceived(data);
   }
}

void DataConnection::notifyOnConnected()
{
   if (listener_) {
      listener_->OnConnected();
   }
}

void DataConnection::notifyOnDisconnected()
{
   if (listener_) {
      listener_->OnDisconnected();
   }
}

void DataConnection::notifyOnError(DataConnectionListener::DataConnectionError errorCode)
{
   if (listener_) {
      listener_->OnError(errorCode);
   }
}
#include "ZmqStreamServerConnection.h"

#include "ActiveStreamClient.h"
#include "FastLock.h"
#include "MessageHolder.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

ZmqStreamServerConnection::ZmqStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : ZmqServerConnection(logger, context)
{}

ZmqContext::sock_ptr ZmqStreamServerConnection::CreateDataSocket()
{
   return context_->CreateStreamSocket();
}

bool ZmqStreamServerConnection::ReadFromDataSocket()
{
   // it is client connection. since it is a stream, we will get two frames
   // first - connection ID
   // second - data frame. if data frame is zero length - it means we are connected or disconnected
   MessageHolder id;
   MessageHolder data;

   int result = zmq_msg_recv(&id, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqStreamServerConnection::listenFunction] {} failed to recv ID frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqStreamServerConnection::listenFunction] {} failed to recv data frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   if (data.GetSize() == 0) {
      //we are either connected or disconncted
      onZeroFrame(id.ToString());
   } else {
      onDataFrameReceived(id.ToString(), data.ToString());
   }

   return true;
}

void ZmqStreamServerConnection::onZeroFrame(const std::string& clientId)
{
   bool clientConnected = false;
   {
      FastLock locker(connectionsLockFlag_);

      auto connectionIt = activeConnections_.find(clientId);
      if (connectionIt == activeConnections_.end()) {
         SPDLOG_LOGGER_TRACE(logger_, "have new client connection on {}", connectionName_);

         auto newConnection = CreateActiveConnection();
         newConnection->InitConnection(clientId, this);

         activeConnections_.emplace(clientId, newConnection);

         clientConnected = true;
      } else {
         SPDLOG_LOGGER_TRACE(logger_, "client disconnected on {}", connectionName_);
         activeConnections_.erase(connectionIt);

         clientConnected = false;
      }
   }

   if (clientConnected) {
      notifyListenerOnNewConnection(clientId);
   } else {
      notifyListenerOnDisconnectedClient(clientId);
   }
}

void ZmqStreamServerConnection::onDataFrameReceived(const std::string& clientId, const std::string& data)
{
   auto connection = findConnection(clientId);
   if (connection == nullptr) {
      logger_->error("[ZmqStreamServerConnection::onDataFrameReceived] {} receied data for closed connection {}"
         , connectionName_, clientId);
   } else {
      connection->onRawDataReceived(data);
   }
}

bool ZmqStreamServerConnection::sendRawData(const std::string& clientId, const std::string& rawData, const SendResultCb &cb)
{
   if (!isActive()) {
      logger_->error("[ZmqStreamServerConnection::sendRawData] cound not send. not connected");
      return false;
   }

   QueueDataToSend(clientId, rawData, cb, true);

   return true;
}

bool ZmqStreamServerConnection::SendDataToClient(const std::string& clientId, const std::string& data, const SendResultCb &cb)
{
   auto connection = findConnection(clientId);
   if (connection == nullptr) {
      logger_->error("[ZmqStreamServerConnection::SendDataToClient] {} send data to closed connection {}"
         , connectionName_, clientId);
      if (cb) {
         cb(clientId, data, false);
      }
      return false;
   }

   const bool result = connection->send(data);
   if (cb) {
      cb(clientId, data, result);
   }
   return result;
}

bool ZmqStreamServerConnection::SendDataToAllClients(const std::string& data, const SendResultCb &cb)
{
   unsigned int successCount = 0;

   FastLock locker(connectionsLockFlag_);
   for (auto & it : activeConnections_) {
      const bool result = it.second->send(data);
      if (result) {
         successCount++;
      }
      if (cb) {
         cb(it.first, data, result);
      }
   }
   return (successCount == activeConnections_.size());
}

ZmqStreamServerConnection::server_connection_ptr
ZmqStreamServerConnection::findConnection(const std::string& clientId)
{
   FastLock locker(connectionsLockFlag_);
   auto connectionIt = activeConnections_.find(clientId);
   if (connectionIt == activeConnections_.end()) {
      return nullptr;
   }

   return connectionIt->second;
}

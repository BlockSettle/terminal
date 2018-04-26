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

bool ZmqStreamServerConnection::ConfigDataSocket(const ZmqContext::sock_ptr& dataSocket)
{
   int lingerPeriod = 0;
   int result = zmq_setsockopt (dataSocket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[ZmqStreamServerConnection::ConfigDataSocket] {} failed to set linger interval: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   return true;
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
   FastLock locker(connectionsLockFlag_);

   auto connectionIt = activeConnections_.find(clientId);
   if (connectionIt == activeConnections_.end()) {
      SPDLOG_DEBUG(logger_, "[ZmqStreamServerConnection::onZeroFrame] have new client connection on {}", connectionName_);

      auto newConnection = CreateActiveConnection();
      newConnection->InitConnection(clientId, this);

      activeConnections_.emplace(clientId, newConnection);

      notifyListenerOnNewConnection(clientId);
   } else {
      SPDLOG_DEBUG(logger_, "[ZmqStreamServerConnection::onZeroFrame] client disconnected on {}", connectionName_);
      activeConnections_.erase(connectionIt);

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

bool ZmqStreamServerConnection::sendRawData(const std::string& clientId, const std::string& rawData)
{
   return sendRawData(clientId, rawData.c_str(), rawData.size());
}

bool ZmqStreamServerConnection::sendRawData(const std::string& clientId, const char* data, size_t size)
{
   if (!isActive()) {
      logger_->error("[ZmqStreamServerConnection::sendRawData] cound not send. not connected");
      return false;
   }

   FastLock locker(socketLockFlag_);

   // XXX - since rate will not be huge and will go through one thread for now
   // send directly to stream socket
   // ZMQ_SNDMORE - should be set every time for stream socket

   int result = zmq_send(dataSocket_.get(), clientId.c_str(), clientId.size(), ZMQ_SNDMORE);
   if (result != clientId.size()) {
      logger_->error("[ZmqStreamServerConnection::sendRawData] {} failed to send client id {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_send(dataSocket_.get(), data, size, ZMQ_SNDMORE);
   if (result != size) {
      logger_->error("[ZmqStreamServerConnection::sendRawData] {} failed to send data frame {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   return true;
}

bool ZmqStreamServerConnection::SendDataToClient(const std::string& clientId, const std::string& data)
{
   auto connection = findConnection(clientId);
   if (connection == nullptr) {
      logger_->error("[ZmqStreamServerConnection::SendDataToClient] {} send data to closed connection {}"
         , connectionName_, clientId);
      return false;
   }

   return connection->send(data);
}

bool ZmqStreamServerConnection::SendDataToAllClients(const std::string& data)
{
   logger_->debug("[ZmqStreamServerConnection::SendDataToAllClients] start sending");

   int successCount = 0;

   FastLock locker(connectionsLockFlag_);
   for (auto & it : activeConnections_) {
      if (it.second->send(data)) {
         successCount++;
      }
   }

   logger_->debug("[ZmqStreamServerConnection::SendDataToAllClients] done sending {} out of {}"
      , successCount, activeConnections_.size());

   return true;
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

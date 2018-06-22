#include "PublisherConnection.h"

#include "FastLock.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

PublisherConnection::PublisherConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
   : logger_{logger}
   , context_{context}
   , dataSocket_{ZmqContext::CreateNullSocket()}
{
   assert(logger_ != nullptr);
   assert(context_ != nullptr);
}

bool PublisherConnection::BindPublishingConnection(const std::string& host, const std::string& port)
{
   auto tempDataSocket = context_->CreatePublishSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[PublisherConnection::BindPublishingConnection] failed to create data socket: {}", zmq_strerror(zmq_errno()));
      return false;
   }

   // epgm - is encapsulated PGM over UDP. No speed limit (default 40 Mbps, ZMQ_RATE)
   // and no additional priviligies required (PGM use raw socket and IP datagramms )
   // in case switch to PGM - check ZMQ_MULTICAST_HOPS ( Maximum network hops for multicast packets)
   std::string endpoint = std::string("epgm://") + host + ":" + port;
   int result = zmq_bind(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqServerConnection::openConnection] failed to bind socket to {} : {}"
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   dataSocket_ = std::move(tempDataSocket);
   return true;
}

bool PublisherConnection::PublishData(const std::string& data)
{
   assert(dataSocket_ != nullptr);
   int sentResult = 0;

   {
      FastLock locker{dataSocketLock_};
      sentResult = zmq_send(dataSocket_.get(), data.c_str(), data.size(), 0);
   }

   if (sentResult != data.size()) {
      logger_->error("[PublisherConnection::PublishData] publish failed: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   return true;
}
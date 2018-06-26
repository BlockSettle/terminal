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

   // Why not PGM
   // 1. PGM is based on IP datagram, and worldwide IP multicast is require global multicast IP
   // 2. IP multicast traffic usually filtered by providers
   // 3. ePGM is UDP based. And this is unicast with overhead, also UDP is not faster any more, since TCP/IP usually is prioritized
   // 4. PGM protocol is still in draft phase ( for along time )
   // 5. OpenPGM implementation is abandoned by Google and not developed any more
   // So TCP/IP is good enough for us. And lets zmq take care about delivery, just use API.
   std::string endpoint = std::string("tcp://") + host + ":" + port;
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
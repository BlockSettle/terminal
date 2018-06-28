#ifndef __PUBLISHER_CONNECTION_H__
#define __PUBLISHER_CONNECTION_H__

#include "ZmqContext.h"

#include <atomic>

class PublisherConnection
{
public:
   PublisherConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~PublisherConnection() noexcept = default;

   PublisherConnection(const PublisherConnection&) = delete;
   PublisherConnection& operator = (const PublisherConnection&) = delete;

   PublisherConnection(PublisherConnection&&) = delete;
   PublisherConnection& operator = (PublisherConnection&&) = delete;

   bool BindPublishingConnection(const std::string& host, const std::string& port);

   bool PublishData(const std::string& data);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqContext>      context_;

   std::atomic_flag                 dataSocketLock_ = ATOMIC_FLAG_INIT;
   ZmqContext::sock_ptr             dataSocket_;
};

#endif // __PUBLISHER_CONNECTION_H__
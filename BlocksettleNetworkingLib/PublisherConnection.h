#ifndef __PUBLISHER_CONNECTION_H__
#define __PUBLISHER_CONNECTION_H__

#include "ZmqContext.h"

#include <atomic>
#include <deque>
#include <string>
#include <thread>

class PublisherConnection
{
public:
   PublisherConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~PublisherConnection() noexcept;

   PublisherConnection(const PublisherConnection&) = delete;
   PublisherConnection& operator = (const PublisherConnection&) = delete;

   PublisherConnection(PublisherConnection&&) = delete;
   PublisherConnection& operator = (PublisherConnection&&) = delete;

   bool InitConnection();
   bool SetWelcomeMessage(const std::string& data);

   bool BindPublishingConnection(const std::string& host, const std::string& port);

   bool PublishData(const std::string& data);

private:
   void stopServer();

   // run in thread
   void listenFunction();

   enum SocketIndex {
      ControlSocketIndex = 0,
      DataSocketIndex
   };

   enum InternalCommandCode {
      CommandSend = 0,
      CommandStop
   };

   void BroadcastPendingData();

   void ReadReceivedData();
private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<ZmqContext>      context_;

   ZmqContext::sock_ptr             dataSocket_;

   std::thread                      listenThread_;

   std::atomic_flag                 controlSocketLockFlag_ = ATOMIC_FLAG_INIT;

   ZmqContext::sock_ptr             threadMasterSocket_;
   ZmqContext::sock_ptr             threadSlaveSocket_;

   std::atomic_flag                 dataQueueLock_ = ATOMIC_FLAG_INIT;
   std::deque<std::string>          dataQueue_;
   std::string                      connectionName_;
};

#endif // __PUBLISHER_CONNECTION_H__
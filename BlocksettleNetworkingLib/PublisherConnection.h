/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

   std::string GetCurrentWelcomeMessage() const;

   // bind to network address. TCP will be used
   bool BindPublishingConnection(const std::string& host, const std::string& port);
   // bind to inproc endpoint
   bool BindPublishingConnection(const std::string& endpoint_name);

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
      CommandUpdateWelcomeMessage,
      CommandStop
   };

   void BroadcastPendingData();

   void ReadReceivedData();

   bool BindConnection(const std::string& endpoint);

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

   mutable std::atomic_flag         welcomeMessageLock_ = ATOMIC_FLAG_INIT;
   std::string                      welcomeMessage_;
};

#endif // __PUBLISHER_CONNECTION_H__
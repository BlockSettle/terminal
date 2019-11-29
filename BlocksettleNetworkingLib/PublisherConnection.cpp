/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PublisherConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"
//BinaryData - for debug purpose only
#include "BinaryData.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

PublisherConnection::PublisherConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
   : logger_{logger}
   , context_{context}
   , dataSocket_{ZmqContext::CreateNullSocket()}
   , threadMasterSocket_{ZmqContext::CreateNullSocket()}
   , threadSlaveSocket_{ZmqContext::CreateNullSocket()}
{
   assert(logger_ != nullptr);
   assert(context_ != nullptr);
}

PublisherConnection::~PublisherConnection() noexcept
{
   stopServer();
}

bool PublisherConnection::InitConnection()
{
   if (dataSocket_ != nullptr) {
      logger_->warn("[PublisherConnection::InitConnection] already initialized");
      return true;
   }

   auto tempDataSocket = context_->CreatePublishSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[PublisherConnection::InitConnection] failed to create data socket: {}", zmq_strerror(zmq_errno()));
      return false;
   }

   const int lingerPeriod = 0;
   int result = zmq_setsockopt(tempDataSocket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[PublisherConnection::InitConnection] failed to set linger interval: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   const int immediate = 1;
   result = zmq_setsockopt(tempDataSocket.get(), ZMQ_IMMEDIATE, &immediate, sizeof(immediate));
   if (result != 0) {
      logger_->error("[PublisherConnection::InitConnection] failed to set immediate flag: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   const int noDrop = 1;
   result = zmq_setsockopt(tempDataSocket.get(), ZMQ_XPUB_NODROP, &noDrop, sizeof(noDrop));
   if (result != 0) {
      logger_->error("[PublisherConnection::InitConnection] failed to set no drop flag: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   dataSocket_ = std::move(tempDataSocket);
   return true;
}


std::string PublisherConnection::GetCurrentWelcomeMessage() const
{
   FastLock locker{welcomeMessageLock_};
   return   welcomeMessage_;
}

bool PublisherConnection::SetWelcomeMessage(const std::string& data)
{
   if (dataSocket_ == nullptr) {
      logger_->error("[PublisherConnection::SetWelcomeMessage] socket not initialized");
      return false;
   }

   {
      FastLock locker{welcomeMessageLock_};
      welcomeMessage_ = data;
   }

   int command = PublisherConnection::CommandUpdateWelcomeMessage;
   int result = 0;

   {
      FastLock locker{controlSocketLockFlag_};
      result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   }

   return result != -1;
}

bool PublisherConnection::BindPublishingConnection(const std::string& endpoint_name)
{
   const std::string endpoint = std::string("inproc://") + endpoint_name;

   return BindConnection(endpoint);
}

bool PublisherConnection::BindPublishingConnection(const std::string& host, const std::string& port)
{
   // Why not PGM
   // 1. PGM is based on IP datagram, and worldwide IP multicast is require global multicast IP
   // 2. IP multicast traffic usually filtered by providers
   // 3. ePGM is UDP based. And this is unicast with overhead, also UDP is not faster any more, since TCP/IP usually is prioritized
   // 4. PGM protocol is still in draft phase ( for along time )
   // 5. OpenPGM implementation is abandoned by Google and not developed any more
   // So TCP/IP is good enough for us. And lets zmq take care about delivery, just use API.
   const std::string endpoint = std::string("tcp://") + host + ":" + port;
   return BindConnection(endpoint);
}


bool PublisherConnection::BindConnection(const std::string& endpoint)
{
   if (dataSocket_ == nullptr) {
      logger_->error("[PublisherConnection::BindConnection] connection not initialized");
      return false;
   }

   int result = zmq_bind(dataSocket_.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[PublisherConnection::BindConnection] failed to bind socket to {} : {}"
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(endpoint);
   std::string controlEndpoint = std::string("inproc://server_") + tempConnectionName;

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[PublisherConnection::BindConnection] failed to create ThreadMasterSocket socket {}"
         , tempConnectionName);
      return false;
   }

   result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[PublisherConnection::BindConnection] failed to bind ThreadMasterSocket socket {}"
         , tempConnectionName);
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[PublisherConnection::BindConnection] failed to create ThreadSlaveSocket socket {}"
         , tempConnectionName);
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[PublisherConnection::BindConnection] failed to connect ThreadSlaveSocket socket {}"
         , tempConnectionName);
      return false;
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);

   // and start thread
   listenThread_ = std::thread(&PublisherConnection::listenFunction, this);

   logger_->debug("[PublisherConnection::BindConnection] starting connection for {}"
      , connectionName_);

   return true;
}

void PublisherConnection::listenFunction()
{
   zmq_pollitem_t  poll_items[2];

   poll_items[PublisherConnection::ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[PublisherConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[PublisherConnection::DataSocketIndex].socket = dataSocket_.get();
   poll_items[PublisherConnection::DataSocketIndex].events = ZMQ_POLLIN;


   logger_->debug("[PublisherConnection::listenFunction] poll thread started for {}"
      , connectionName_);

   int result;

   int errorCount = 0;

   while(true) {
      result = zmq_poll(poll_items, 2, -1);
      if (result == -1) {
         errorCount++;
         if ((zmq_errno() != EINTR) || (errorCount > 10)) {
            logger_->error("[PublisherConnection::listenFunction] poll failed for {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         } else {
            logger_->debug("[PublisherConnection::listenFunction] interrupted");
            continue;
         }
      }

      errorCount = 0;

      if (poll_items[PublisherConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[PublisherConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            logger_->error("[PublisherConnection::listenFunction] failed to recv command on {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == PublisherConnection::CommandSend) {
            BroadcastPendingData();
         } else if (command_code == PublisherConnection::CommandUpdateWelcomeMessage) {
            std::string welcomeMessageCopy;
            {
               FastLock locker{welcomeMessageLock_};
               welcomeMessageCopy = welcomeMessage_;
            }
            int result = zmq_setsockopt(dataSocket_.get(), ZMQ_XPUB_WELCOME_MSG, welcomeMessageCopy.c_str(), welcomeMessageCopy.size());
            if (result != 0) {
               logger_->error("[PublisherConnection::SetWelcomeMessage] failed to set no welcome message: {}"
                  , zmq_strerror(zmq_errno()));
            }
         } else if (command_code == PublisherConnection::CommandStop) {
            break;
         } else {
            logger_->error("[PublisherConnection::listenFunction] unexpected command code {} for {}"
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[PublisherConnection::DataSocketIndex].revents & ZMQ_POLLIN) {
         ReadReceivedData();
      }
   }

   dataSocket_ = context_->CreateNullSocket();

   logger_->debug("[PublisherConnection::listenFunction] poll thread stopped for {}", connectionName_);
}

void PublisherConnection::ReadReceivedData()
{
   while (true) {
      MessageHolder msg;
      int result = zmq_msg_recv(&msg, dataSocket_.get(), ZMQ_DONTWAIT);
      if (result == - 1 ) {
         logger_->error("[PublisherConnection::ReadReceivedData] failed to receive");
         return;
      }

      logger_->debug("[PublisherConnection::ReadReceivedData] received {} bytes: {}. ( {} )"
         , result, BinaryData(msg.ToString()).toHexStr(), (msg.IsLast() ? "last message" : "there is more") );

      if (msg.IsLast()) {
         break;
      }
   }
}

void PublisherConnection::stopServer()
{
   if (!listenThread_.joinable()) {
      logger_->error("[PublisherConnection::stopServer] not running");
      return;
   }
   logger_->debug("[PublisherConnection::stopServer] stopping {}", connectionName_);

   int command = PublisherConnection::CommandStop;
   int result = 0;

   {
      FastLock locker{controlSocketLockFlag_};
      result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   }

   if (result == -1) {
      logger_->error("[PublisherConnection::stopServer] failed to send stop comamnd for {} : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return;
   }

   listenThread_.join();
}

void PublisherConnection::BroadcastPendingData()
{
   std::deque<std::string> pendingData;

   {
      FastLock locker{dataQueueLock_};
      pendingData.swap(dataQueue_);
   }

   for (const auto &data : pendingData) {
      auto result = zmq_send(dataSocket_.get(), data.c_str(), data.size(), 0);
      if (result != data.size()) {
         logger_->error("[PublisherConnection::SendDataToDataSocket] {} failed to send client id {}. {} packets dropped"
            , connectionName_, zmq_strerror(zmq_errno())
            , pendingData.size());
         break;
      }
   }
}

bool PublisherConnection::PublishData(const std::string& data)
{
   assert(dataSocket_ != nullptr);
   {
      FastLock locker{dataQueueLock_};
      dataQueue_.emplace_back( data );
   }

   int command = PublisherConnection::CommandSend;
   int result = 0;

   {
      FastLock locker{controlSocketLockFlag_};
      result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   }

   return result != -1;
}
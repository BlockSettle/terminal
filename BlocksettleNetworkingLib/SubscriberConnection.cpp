/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SubscriberConnection.h"

#include "MessageHolder.h"
#include "ZmqHelperFunctions.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

SubscriberConnectionListenerCB::SubscriberConnectionListenerCB(const dataReceivedCB& onDataReceived
   , const connectedCB& onConnected
   , const disconnectedCB& onDisconnected)
 : onDataReceived_{onDataReceived}
 , onConnected_{onConnected}
 , onDisconnected_{onDisconnected}
{}

void SubscriberConnectionListenerCB::OnDataReceived(const std::string& data)
{
   if (onDataReceived_) {
      onDataReceived_(data);
   }
}
void SubscriberConnectionListenerCB::OnConnected()
{
   if (onConnected_) {
      onConnected_();
   }
}
void SubscriberConnectionListenerCB::OnDisconnected()
{
   if (onDisconnected_) {
      onDisconnected_();
   }
}

SubscriberConnection::SubscriberConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : logger_{logger}
 , context_{context}
 , dataSocket_{ZmqContext::CreateNullSocket()}
 , threadMasterSocket_{ZmqContext::CreateNullSocket()}
 , threadSlaveSocket_{ZmqContext::CreateNullSocket()}
 , monSocket_{ZmqContext::CreateNullSocket()}
{}

SubscriberConnection::~SubscriberConnection() noexcept
{
   stopListen();
}

bool SubscriberConnection::isActive() const
{
   return listenThread_.joinable();
}

bool SubscriberConnection::ConnectToPublisher(const std::string& endpointName, SubscriberConnectionListener* listener)
{
   const std::string endpoint = std::string("inproc://") + endpointName;

   return ConnectToPublisherEndpoint(endpoint, listener);
}

bool SubscriberConnection::ConnectToPublisher(const std::string& host, const std::string& port, SubscriberConnectionListener* listener)
{
   const std::string endpoint = std::string("tcp://") + host + ":" + port;

   return ConnectToPublisherEndpoint(endpoint, listener);
}

bool SubscriberConnection::ConnectToPublisherEndpoint(const std::string& endpoint, SubscriberConnectionListener* listener)
{
   if (listener == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] empty listener not allowed");
      return false;
   }

   if (isActive()) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] connection active.");
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(endpoint);
   std::string controlEndpoint = std::string("inproc://") + tempConnectionName;

   ZmqContext::sock_ptr tempDataSocket = context_->CreateSubscribeSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to create socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to create ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   int result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to bind ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to create ThreadSlaveSocket socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to connect ThreadSlaveSocket socket {}. {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   std::string monitorConnectionName = "inproc://mon-" + tempConnectionName;

   result = zmq_socket_monitor(tempDataSocket.get(), monitorConnectionName.c_str(), ZMQ_EVENT_ALL);
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] Failed to create monitor socket: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }
   auto tempMonSocket = context_->CreateMonitorSocket();
   result = zmq_connect(tempMonSocket.get(), monitorConnectionName.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] Failed to connect monitor socket: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   // connect subscriber socket
   result = zmq_connect(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to connect socket to {}. {}"
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   // subscribe to data (all messages, no filtering)
   result = zmq_setsockopt(tempDataSocket.get(), ZMQ_SUBSCRIBE, nullptr, 0);
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisherEndpoint] failed to subscribe: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   monSocket_ = std::move(tempMonSocket);
   dataSocket_ = std::move(tempDataSocket);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);
   isConnected_ = false;
   listener_ = listener;

   // and start thread
   listenThread_ = std::thread(&SubscriberConnection::listenFunction, this);

   return true;
}

void SubscriberConnection::stopListen()
{
   listener_ = nullptr;

   if (!isActive()) {
      return;
   }

   int command = SubscriberConnection::CommandStop;
   int result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   if (result == -1) {
      logger_->error("[SubscriberConnection::stopListen] failed to send stop comamnd for {} : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return;
   }

   if (std::this_thread::get_id() == listenThread_.get_id()) {
      listenThread_.detach();
   } else {
      try {
         listenThread_.join();
      }
      catch (const std::exception &e) {
         logger_->error("[SubscriberConnection::stopListen] failed to join thread: {}", e.what());
      }
   }

   return;
}

void SubscriberConnection::listenFunction()
{
   // save smart pointers to avoid closing connection in listen thread
   auto loggerCopy = logger_;
   auto threadSlaveSocketCopy = std::move(threadSlaveSocket_);
   auto dataSocketCopy = std::move(dataSocket_);
   auto monSocketCopy = std::move(monSocket_);

   zmq_pollitem_t  poll_items[3];
   memset(&poll_items, 0, sizeof(poll_items));

   poll_items[SubscriberConnection::ControlSocketIndex].socket = threadSlaveSocketCopy.get();
   poll_items[SubscriberConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[SubscriberConnection::StreamSocketIndex].socket = dataSocketCopy.get();
   poll_items[SubscriberConnection::StreamSocketIndex].events = ZMQ_POLLIN;

   poll_items[SubscriberConnection::MonitorSocketIndex].socket = monSocketCopy.get();
   poll_items[SubscriberConnection::MonitorSocketIndex].events = ZMQ_POLLIN;

   int result;

   while(true) {
      result = zmq_poll(poll_items, 3, -1);
      if (result == -1) {
         loggerCopy->error("[SubscriberConnection::listenFunction] poll failed for {} : {}"
            , connectionName_, zmq_strerror(zmq_errno()));
         break;
      }

      if (poll_items[SubscriberConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[SubscriberConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            loggerCopy->error("[SubscriberConnection::listenFunction] failed to recv command on {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == SubscriberConnection::CommandStop) {
            break;
         } else {
            loggerCopy->error("[SubscriberConnection::listenFunction] unexpected command code {} for {}"
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[SubscriberConnection::StreamSocketIndex].revents & ZMQ_POLLIN) {
         if (!recvData(dataSocketCopy)) {
            break;
         }
      }

      if (monSocketCopy && (poll_items[SubscriberConnection::MonitorSocketIndex].revents & ZMQ_POLLIN)) {
         switch (bs::network::get_monitor_event(monSocketCopy.get())) {
         case ZMQ_EVENT_CONNECTED:
            if (!isConnected_) {
               if (listener_) {
                  listener_->OnConnected();
               }
               isConnected_ = true;
            }
            break;

         case ZMQ_EVENT_DISCONNECTED:
            if (isConnected_) {
               if (listener_) {
                  listener_->OnDisconnected();
               }
               isConnected_ = false;
            }
            break;
         }
      }
   }

   // clean monitor states
   zmq_socket_monitor(dataSocketCopy.get(), nullptr, ZMQ_EVENT_ALL);
}

bool SubscriberConnection::recvData(const ZmqContext::sock_ptr& dataSocket)
{
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[SubscriberConnection::recvData] {} failed to recv data frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   if (listener_) {
      listener_->OnDataReceived(data.ToString());
   }

   return true;
}

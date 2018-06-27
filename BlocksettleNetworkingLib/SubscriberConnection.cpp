#include "SubscriberConnection.h"

#include "MessageHolder.h"
#include "ZMQHelperFunctions.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

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
   return listener_ != nullptr;
}

bool SubscriberConnection::ConnectToPublisher(const std::string& host, const std::string& port, SubscriberConnectionListener* listener)
{
   if (listener == nullptr) {
      return false;
   }

   if (isActive()) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] connection active.");
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(host, port);
   std::string controlEndpoint = std::string("inproc://") + tempConnectionName;

   ZmqContext::sock_ptr tempDataSocket = context_->CreateSubscribeSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to create socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to create ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   int result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to bind ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to create ThreadSlaveSocket socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to connect ThreadSlaveSocket socket {}. {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   std::string monitorConnectionName = "inproc://mon-" + tempConnectionName;

   result = zmq_socket_monitor(tempDataSocket.get(), monitorConnectionName.c_str(), ZMQ_EVENT_ALL);
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] Failed to create monitor socket: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }
   auto tempMonSocket = context_->CreateMonitorSocket();
   result = zmq_connect(tempMonSocket.get(), monitorConnectionName.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] Failed to connect monitor socket: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   // connect subscriber socket
   std::string endpoint = std::string("tcp://") + host + ":" + port;
   result = zmq_connect(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to connect socket to {}. {}"
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   // subscribe to data (all messages, no filtering)
   result = zmq_setsockopt(tempDataSocket.get(), ZMQ_SUBSCRIBE, nullptr, 0);
   if (result != 0) {
      logger_->error("[SubscriberConnection::ConnectToPublisher] failed to subscribe: {}"
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

   listenThread_.join();
   return;
}

void SubscriberConnection::listenFunction()
{
   zmq_pollitem_t  poll_items[3];
   memset(&poll_items, 0, sizeof(poll_items));

   poll_items[SubscriberConnection::ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[SubscriberConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[SubscriberConnection::StreamSocketIndex].socket = dataSocket_.get();
   poll_items[SubscriberConnection::StreamSocketIndex].events = ZMQ_POLLIN;

   poll_items[SubscriberConnection::MonitorSocketIndex].socket = monSocket_.get();
   poll_items[SubscriberConnection::MonitorSocketIndex].events = ZMQ_POLLIN;

   int result;

   while(true) {
      result = zmq_poll(poll_items, 3, -1);
      if (result == -1) {
         logger_->error("[SubscriberConnection::listenFunction] poll failed for {} : {}"
            , connectionName_, zmq_strerror(zmq_errno()));
         break;
      }

      if (poll_items[SubscriberConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[SubscriberConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            logger_->error("[SubscriberConnection::listenFunction] failed to recv command on {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == SubscriberConnection::CommandStop) {
            break;
         } else {
            logger_->error("[SubscriberConnection::listenFunction] unexpected command code {} for {}"
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[SubscriberConnection::StreamSocketIndex].revents & ZMQ_POLLIN) {
         if (!recvData()) {
            break;
         }
      }

      if (monSocket_ && (poll_items[SubscriberConnection::MonitorSocketIndex].revents & ZMQ_POLLIN)) {
         switch (bs::network::get_monitor_event(monSocket_.get())) {
         case ZMQ_EVENT_CONNECTED:
            if (!isConnected_) {
               listener_->OnConnected();
               isConnected_ = true;
            }
            break;

         case ZMQ_EVENT_DISCONNECTED:
            if (isConnected_) {
               listener_->OnDisconnected();
               isConnected_ = false;
            }
            break;
         }
      }
   }

   if (isConnected_) {
      listener_->OnDisconnected();
   }
}

bool SubscriberConnection::recvData()
{
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[SubscriberConnection::recvData] {} failed to recv data frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   listener_->OnDataReceived(data.ToString());

   return true;
}
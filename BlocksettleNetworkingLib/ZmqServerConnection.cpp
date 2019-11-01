#include "ZmqServerConnection.h"
#include "ZmqHelperFunctions.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "ThreadName.h"

#include <spdlog/spdlog.h>
#include <zmq.h>

namespace
{

   const std::chrono::seconds kHearthbeatCheckPeriod(1);

} // namespace

ZmqServerConnection::ZmqServerConnection(
   const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context)
   : logger_(logger)
   , context_(context)
   , dataSocket_(ZmqContext::CreateNullSocket())
   , monSocket_(ZmqContext::CreateNullSocket())
   , threadMasterSocket_(ZmqContext::CreateNullSocket())
   , threadSlaveSocket_(ZmqContext::CreateNullSocket())
   , threadName_("ZmqSrv")
{
   assert(logger_ != nullptr);
   assert(context_ != nullptr);
}

ZmqServerConnection::~ZmqServerConnection() noexcept
{
   stopServer();

   // Update listener after thread is stopped
   listener_ = nullptr;

   if (listenThread_.joinable()) {
      // This is not normally needed but good to have to prevent crash in case stopServer fails
      listenThread_.join();
   }
}

bool ZmqServerConnection::isActive() const
{
   return dataSocket_ != nullptr;
}

// it is long but straight forward, just init all the objects
// if ok - move temp objects to members
// if failed - it is all on stack and smart pointers
//  that will take care of closing and cleaning up
bool ZmqServerConnection::BindConnection(const std::string& host , const std::string& port
   , ServerConnectionListener* listener)
{
   assert(context_ != nullptr);
   assert(listener != nullptr);

   if (isActive()) {
      logger_->error("[{}] connection active. You should close it first: {}."
         , __func__, connectionName_);
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(host, port);

   // create stream socket ( connected to server )
   ZmqContext::sock_ptr tempDataSocket = CreateDataSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[{}] failed to create data socket socket {}", __func__
         , tempConnectionName);
      return false;
   }

   if (!ConfigDataSocket(tempDataSocket)) {
      logger_->error("[{}] failed to config data socket {}", __func__
         , tempConnectionName);
      return false;
   }

   ZmqContext::sock_ptr tempMonSocket = context_->CreateMonitorSocket();
   if (tempMonSocket == nullptr) {
      logger_->error("[{}] failed to open monitor socket {}", __func__
         , tempConnectionName);
      return false;
   }

   int lingerPeriod = 0;
   int result = zmq_setsockopt(tempMonSocket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[{}] failed to config monitor socket on {}", __func__
         , tempConnectionName);
      return false;
   }

   monitorConnectionName_ = "inproc://monitor-" + tempConnectionName;

   result = zmq_socket_monitor(tempDataSocket.get(), monitorConnectionName_.c_str(),
      ZMQ_EVENT_ALL);
   if (result != 0) {
      logger_->error("[{}] failed to create monitor {}", __func__
         , tempConnectionName);
      return false;
   }

   for (const std::string &fromAddress : fromAddresses_) {
      // ZMQ_TCP_ACCEPT_FILTER is deprecated in favor of ZAP API.
      // But let's use it for now because our future ZMQ usage is not yet clear.
      int result = zmq_setsockopt(tempDataSocket.get(), ZMQ_TCP_ACCEPT_FILTER, fromAddress.c_str(), fromAddress.size());
      if (result != 0) {
         SPDLOG_LOGGER_ERROR(logger_, "can't set ZMQ_TCP_ACCEPT_FILTER for {}", fromAddress);
         return false;
      }
   }

   result = zmq_connect(tempMonSocket.get(), monitorConnectionName_.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to connect to monitor {}", __func__
         , tempConnectionName);
      return false;
   }

   // connect socket to server ( connection state will be changed in listen thread )
   std::string endpoint = ZmqContext::CreateConnectionEndpoint(zmqTransport_, host, port);
   if (endpoint.empty()) {
      logger_->error("[{}] failed to generate connection address", __func__);
      return false;
   }

   result = zmq_bind(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to bind socket to {} : {}", __func__
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   std::string controlEndpoint = std::string("inproc://server_") + tempConnectionName;

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[{}] failed to create ThreadMasterSocket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to bind ThreadMasterSocket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[{}] failed to create ThreadSlaveSocket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to connect ThreadSlaveSocket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   dataSocket_ = std::move(tempDataSocket);
   monSocket_ = std::move(tempMonSocket);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);

   listener_ = listener;

   // and start thread
   listenThread_ = std::thread(&ZmqServerConnection::listenFunction, this);

   logger_->debug("[{}] starting connection for {}", __func__, connectionName_);

   return true;
}

void ZmqServerConnection::listenFunction()
{
   bs::setCurrentThreadName(threadName_);

   zmq_pollitem_t  poll_items[3];

   poll_items[ZmqServerConnection::ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[ZmqServerConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[ZmqServerConnection::DataSocketIndex].socket = dataSocket_.get();
   poll_items[ZmqServerConnection::DataSocketIndex].events = ZMQ_POLLIN;

   poll_items[ZmqServerConnection::MonitorSocketIndex].socket = monSocket_.get();
   poll_items[ZmqServerConnection::MonitorSocketIndex].events = ZMQ_POLLIN;

   logger_->debug("[{}] poll thread started for {}", __func__, connectionName_);

   int errorCount = 0;

   while (true) {
      int periodMs = std::chrono::duration_cast<std::chrono::milliseconds>(kHearthbeatCheckPeriod).count();
      int result = zmq_poll(poll_items, 3, periodMs);

      if (result == -1) {
         errorCount++;
         if ((zmq_errno() != EINTR) || (errorCount > 10)) {
            logger_->error("[{}] poll failed for {} : {}", __func__
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         } else {
            logger_->debug("[{}] interrupted", __func__);
            continue;
         }
      }

      errorCount = 0;

      if (poll_items[ZmqServerConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[ZmqServerConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            logger_->error("[{}] failed to recv command on {} : {}", __func__
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == ZmqServerConnection::CommandSend) {
            SendDataToDataSocket();
         } else if (command_code == ZmqServerConnection::CommandStop) {
            break;
         } else {
            logger_->error("[{}] unexpected command code {} for {}", __func__
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[ZmqServerConnection::DataSocketIndex].revents & ZMQ_POLLIN) {
         if (!ReadFromDataSocket()) {
            logger_->error("[{}] failed to read from data socket on {}"
               , __func__, connectionName_);
            break;
         }
      }

      if (poll_items[ZmqServerConnection::MonitorSocketIndex].revents & ZMQ_POLLIN) {
         int sock = 0;
         switch (bs::network::get_monitor_event(monSocket_.get(), &sock)) {
            case ZMQ_EVENT_ACCEPTED :
            {
               std::string cliIP = bs::network::peerAddressString(sock);
               connectedPeers_.emplace(std::make_pair(sock, cliIP));
               if (listener_) {
                  listener_->OnPeerConnected(cliIP);
               }
            }
            break;

            case ZMQ_EVENT_DISCONNECTED :
            case ZMQ_EVENT_CLOSED :
            {
               const auto it = connectedPeers_.find(sock);

               if (it != connectedPeers_.cend()) {
                  if (listener_) {
                     listener_->OnPeerDisconnected(it->second);
                  }
                  connectedPeers_.erase(it);
               }
            }
            break;
         }
      }

      onPeriodicCheck();
   }

   zmq_socket_monitor(dataSocket_.get(), nullptr, ZMQ_EVENT_ALL);
   dataSocket_ = context_->CreateNullSocket();
   monSocket_ = context_->CreateNullSocket();

   if (listener_) {
      for (const auto &peer : connectedPeers_) {
         listener_->OnPeerDisconnected(peer.second);
      }
   }
   connectedPeers_.clear();

   logger_->debug("[{}] poll thread stopped for {}", __func__, connectionName_);
}

void ZmqServerConnection::stopServer()
{
   if (!isActive()) {
      return;
   }

   logger_->debug("[{}] stopping {}", __func__, connectionName_);

   int command = ZmqServerConnection::CommandStop;
   int result = 0;

   {
      FastLock locker{controlSocketLockFlag_};
      result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   }

   if (result == -1) {
      logger_->error("[{}] failed to send stop comamnd for {} : {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return;
   }

   listenThread_.join();
}

void ZmqServerConnection::requestPeriodicCheck()
{
   SendDataCommand();
}

std::thread::id ZmqServerConnection::listenThreadId() const
{
   return listenThread_.get_id();
}

bool ZmqServerConnection::SendDataCommand()
{
   int command = ZmqServerConnection::CommandSend;
   int result = 0;

   {
      FastLock locker{controlSocketLockFlag_};
      result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), ZMQ_DONTWAIT);
   }

   if (result == -1) {
      logger_->error("[{}] failed to send data command for {} : {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
   }

   return result != -1;
}

void ZmqServerConnection::notifyListenerOnData(const std::string& clientId, const std::string& data)
{
   if (listener_) {
      listener_->OnDataFromClient(clientId, data);
   }
}

void ZmqServerConnection::notifyListenerOnNewConnection(const std::string& clientId)
{
   if (listener_) {
      listener_->OnClientConnected(clientId);
   }
}

void ZmqServerConnection::notifyListenerOnDisconnectedClient(const std::string& clientId)
{
   if (listener_) {
      listener_->OnClientDisconnected(clientId);
   }
   clientInfo_.erase(clientId);
}

void ZmqServerConnection::notifyListenerOnClientError(const std::string& clientId, const std::string &error)
{
   if (listener_) {
      listener_->onClientError(clientId, error);
   }
}

void ZmqServerConnection::notifyListenerOnClientError(const std::string &clientId, ServerConnectionListener::ClientError errorCode, int socket)
{
   if (listener_) {
      listener_->onClientError(clientId, errorCode, socket);
   }
}

std::string ZmqServerConnection::GetClientInfo(const std::string &clientId) const
{
   const auto &it = clientInfo_.find(clientId);
   if (it != clientInfo_.end()) {
      return it->second;
   }
   return "Unknown";
}

bool ZmqServerConnection::QueueDataToSend(const std::string& clientId, const std::string& data
   , const SendResultCb &cb, bool sendMore)
{
   {
      FastLock locker{dataQueueLock_};
      dataQueue_.emplace_back( DataToSend{clientId, data, cb, sendMore});
   }

   return SendDataCommand();
}

void ZmqServerConnection::onPeriodicCheck()
{
}

void ZmqServerConnection::SendDataToDataSocket()
{
   std::deque<DataToSend> pendingData;

   {
      FastLock locker{dataQueueLock_};
      pendingData.swap(dataQueue_);
   }

   for (const auto &dataPacket : pendingData) {
      int result = zmq_send(dataSocket_.get(), dataPacket.clientId.c_str(), dataPacket.clientId.size(), ZMQ_SNDMORE);
      if (result != dataPacket.clientId.size()) {
         logger_->error("[{}] {} failed to send client id {}", __func__
            , connectionName_, zmq_strerror(zmq_errno()));
         if (dataPacket.cb) {
            dataPacket.cb(dataPacket.clientId, dataPacket.data, false);
         }
         continue;
      }

      result = zmq_send(dataSocket_.get(), dataPacket.data.data(), dataPacket.data.size(), (dataPacket.sendMore ? ZMQ_SNDMORE : 0));
      if (result != dataPacket.data.size()) {
         logger_->error("[{}] {} failed to send data frame {} to {}", __func__
            , connectionName_, zmq_strerror(zmq_errno()), dataPacket.clientId);
         if (dataPacket.cb) {
            dataPacket.cb(dataPacket.clientId, dataPacket.data, false);
         }
         continue;
      }

      if (dataPacket.cb) {
         dataPacket.cb(dataPacket.clientId, dataPacket.data, true);
      }
   }
}

bool ZmqServerConnection::SetZMQTransport(ZMQTransport transport)
{
   switch(transport) {
   case ZMQTransport::TCPTransport:
   case ZMQTransport::InprocTransport:
      zmqTransport_ = transport;
      return true;
   }

   logger_->error("[{}] undefined transport", __func__);
   return false;
}

void ZmqServerConnection::setListenFrom(const std::vector<std::string> &fromAddresses)
{
   fromAddresses_ = fromAddresses;
}

void ZmqServerConnection::setThreadName(const std::string &name)
{
   threadName_ = name;
}

bool ZmqServerConnection::ConfigDataSocket(const ZmqContext::sock_ptr &dataSocket)
{
   int immediate = immediate_ ? 1 : 0;
   if (zmq_setsockopt(dataSocket.get(), ZMQ_IMMEDIATE, &immediate, sizeof(immediate)) != 0) {
      logger_->error("[{}] {} failed to set immediate flag: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   if (!identity_.empty()) {
      if (zmq_setsockopt(dataSocket.get(), ZMQ_IDENTITY, identity_.c_str(), identity_.size()) != 0) {
         logger_->error("[{}] {} failed to set server identity {}", __func__
            , connectionName_, zmq_strerror(zmq_errno()));
         return false;
      }
   }

   if (zmq_setsockopt(dataSocket.get(), ZMQ_SNDTIMEO, &sendTimeoutInMs_, sizeof(sendTimeoutInMs_)) != 0) {
      logger_->error("[{}] {} failed to set send timeout {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   int lingerPeriod = 0;
   if (zmq_setsockopt(dataSocket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod)) != 0) {
      logger_->error("[{}] {} failed to set linger interval {}: {}", __func__
         , connectionName_, lingerPeriod, zmq_strerror(zmq_errno()));
      return false;
   }

   return true;
}

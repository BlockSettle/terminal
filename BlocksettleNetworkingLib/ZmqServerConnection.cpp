#include "ZmqServerConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"

#include <spdlog/spdlog.h>
#include <zmq.h>

ZmqServerConnection::ZmqServerConnection(const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context)
   : logger_(logger)
   , context_(context)
   , dataSocket_(ZmqContext::CreateNullSocket())
   , threadMasterSocket_(ZmqContext::CreateNullSocket())
   , threadSlaveSocket_(ZmqContext::CreateNullSocket())
{
   assert(logger_ != nullptr);
   assert(context_ != nullptr);
}

ZmqServerConnection::~ZmqServerConnection()
{
   stopServer();
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
      logger_->error("[ZmqServerConnection::openConnection] connection active. You should close it first: {}."
         , connectionName_);
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(host, port);

   // create stream socket ( connected to server )
   ZmqContext::sock_ptr tempDataSocket = CreateDataSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[ZmqServerConnection::openConnection] failed to create data socket socket {}"
         , tempConnectionName);
      return false;
   }

   if (!ConfigDataSocket(tempDataSocket)) {
      logger_->error("[ZmqServerConnection::openConnection] failed to config data socket {}"
         , tempConnectionName);
      return false;
   }

   // connect socket to server ( connection state will be changed in listen thread )
   std::string endpoint = std::string("tcp://") + host + ":" + port;
   int result = zmq_bind(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqServerConnection::openConnection] failed to bind socket to {} : {}"
         , endpoint, zmq_strerror(zmq_errno()));
      return false;
   }

   std::string controlEndpoint = std::string("inproc://server_") + tempConnectionName;

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[ZmqServerConnection::openConnection] failed to create ThreadMasterSocket socket {}"
         , tempConnectionName);
      return false;
   }

   result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqServerConnection::openConnection] failed to bind ThreadMasterSocket socket {}"
         , tempConnectionName);
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[ZmqServerConnection::openConnection] failed to create ThreadSlaveSocket socket {}"
         , tempConnectionName);
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqServerConnection::openConnection] failed to connect ThreadSlaveSocket socket {}"
         , tempConnectionName);
      return false;
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   dataSocket_ = std::move(tempDataSocket);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);

   listener_ = listener;

   // and start thread
   listenThread_ = std::thread(&ZmqServerConnection::listenFunction, this);

   logger_->debug("[ZmqServerConnection::openConnection] starting connection for {}"
      , connectionName_);

   return true;
}

void ZmqServerConnection::listenFunction()
{
   zmq_pollitem_t  poll_items[2];

   poll_items[ZmqServerConnection::ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[ZmqServerConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[ZmqServerConnection::DataSocketIndex].socket = dataSocket_.get();
   poll_items[ZmqServerConnection::DataSocketIndex].events = ZMQ_POLLIN;

   logger_->debug("[ZmqServerConnection::listenFunction] poll thread started for {}"
      , connectionName_);

   int result;

   int errorCount = 0;

   while(true) {
      result = zmq_poll(poll_items, 2, -1);
      if (result == -1) {
         errorCount++;
         if ((zmq_errno() != EINTR) || (errorCount > 10)) {
            logger_->error("[ZmqServerConnection::listenFunction] poll failed for {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         } else {
            logger_->debug("[ZmqServerConnection::listenFunction] interrupted");
            continue;
         }
      }

      errorCount = 0;

      if (poll_items[ZmqServerConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[ZmqServerConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            logger_->error("[ZmqServerConnection::listenFunction] failed to recv command on {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == ZmqServerConnection::CommandStop) {
            break;
         } else {
            logger_->error("[ZmqServerConnection::listenFunction] unexpected command code {} for {}"
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[ZmqServerConnection::DataSocketIndex].revents & ZMQ_POLLIN) {
         if (!ReadFromDataSocket()) {
            logger_->error("[ZmqServerConnection::listenFunction] failed to read from data socket on {}"
               , connectionName_);
            break;
         }
      }
   }

   logger_->debug("[ZmqServerConnection::listenFunction] poll thread stopped for {}", connectionName_);
}

void ZmqServerConnection::stopServer()
{
   if (!isActive()) {
      logger_->debug("[ZmqServerConnection::stopServer] connection already stopped {}"
         , connectionName_);
      return;
   }

   logger_->debug("[ZmqServerConnection::stopServer] stopping {}", connectionName_);

   int command = ZmqServerConnection::CommandStop;
   int result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   if (result == -1) {
      logger_->error("[ZmqServerConnection::stopServer] failed to send stop comamnd for {} : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return;
   }

   listenThread_.join();
}

void ZmqServerConnection::notifyListenerOnData(const std::string& clientId, const std::string& data)
{
   listener_->OnDataFromClient(clientId, data);
}

void ZmqServerConnection::notifyListenerOnNewConnection(const std::string& clientId)
{
   listener_->OnClientConnected(clientId);
}

void ZmqServerConnection::notifyListenerOnDisconnectedClient(const std::string& clientId)
{
   listener_->OnClientDisconnected(clientId);
   clientInfo_.erase(clientId);
}

std::string ZmqServerConnection::GetClientInfo(const std::string &clientId) const
{
   const auto &it = clientInfo_.find(clientId);
   if (it != clientInfo_.end()) {
      return it->second;
   }
   return "Unknown";
}

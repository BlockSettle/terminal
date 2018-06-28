#include "ZmqDataConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "ZMQHelperFunctions.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

ZmqDataConnection::ZmqDataConnection(const std::shared_ptr<spdlog::logger>& logger, bool useMonitor)
   : logger_(logger), useMonitor_(useMonitor)
   , dataSocket_(ZmqContext::CreateNullSocket())
   , monSocket_(ZmqContext::CreateNullSocket())
   , threadMasterSocket_(ZmqContext::CreateNullSocket())
   , threadSlaveSocket_(ZmqContext::CreateNullSocket())
   , isConnected_(false)
{
   assert(logger_);
}

ZmqDataConnection::~ZmqDataConnection()
{
   closeConnection();
}

bool ZmqDataConnection::isActive() const
{
   return dataSocket_ != nullptr;
}

void ZmqDataConnection::resetConnectionObjects()
{
   // do not clean connectionName_ for debug purpose
   socketId_.clear();

   dataSocket_.reset();
   threadMasterSocket_.reset();
   threadSlaveSocket_.reset();
}

// it is long but straight forward, just init all the objects
// if ok - move temp objects to members
// if failed - it is all on stack and smart pointers 
//  that will take care of closing and cleaning up
bool ZmqDataConnection::openConnection(const std::string& host , const std::string& port
   , DataConnectionListener* listener)
{
   assert(context_ != nullptr);
   assert(listener != nullptr);

   if (isActive()) {
      logger_->error("[ZmqDataConnection::openConnection] connection active. You should close it first: {}."
         , connectionName_);
      return false;
   }

   std::string tempConnectionName = context_->GenerateConnectionName(host, port);

   char buf[256];
   size_t  buf_size = 256;

   // create stream socket ( connected to server )
   ZmqContext::sock_ptr tempDataSocket = CreateDataSocket();
   if (tempDataSocket == nullptr) {
      logger_->error("[ZmqDataConnection::openConnection] failed to create data socket socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   if (!ConfigureDataSocket(tempDataSocket)) {
      logger_->error("[ZmqDataConnection::openConnection] failed to configure data socket socket {}"
         , tempConnectionName);
      return false;
   }

   // connect socket to server ( connection state will be changed in listen thread )
   std::string endpoint = std::string("tcp://") + host + ":" + port;
   int result = zmq_connect(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqDataConnection::openConnection] failed to connect socket to {}"
         , endpoint);
      return false;
   }

   // get socket id
   result = zmq_getsockopt(tempDataSocket.get(), ZMQ_IDENTITY, buf, &buf_size);
   if (result != 0) {
      logger_->error("[ZmqDataConnection::openConnection] failed to get socket Id {}"
         , tempConnectionName);
      return false;
   }

   std::string controlEndpoint = std::string("inproc://") + tempConnectionName;

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   if (tempThreadMasterSocket == nullptr) {
      logger_->error("[ZmqDataConnection::openConnection] failed to create ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqDataConnection::openConnection] failed to bind ThreadMasterSocket socket {}: {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   if (tempThreadSlaveSocket == nullptr) {
      logger_->error("[ZmqDataConnection::openConnection] failed to create ThreadSlaveSocket socket {} : {}"
         , tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[ZmqDataConnection::openConnection] failed to connect ThreadSlaveSocket socket {}"
         , tempConnectionName);
      return false;
   }

   if (useMonitor_) {
      int rc = zmq_socket_monitor(tempDataSocket.get(), ("inproc://mon-" + tempConnectionName).c_str(), ZMQ_EVENT_ALL);
      if (rc != 0) {
         logger_->error("Failed to create monitor socket: {}", zmq_strerror(zmq_errno()));
         return false;
      }
      auto tempMonSocket = context_->CreateMonitorSocket();
      rc = zmq_connect(tempMonSocket.get(), ("inproc://mon-" + tempConnectionName).c_str());
      if (rc != 0) {
         logger_->error("Failed to connect monitor socket: {}", zmq_strerror(zmq_errno()));
         return false;
      }

      monSocket_ = std::move(tempMonSocket);
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   socketId_ = std::string(buf, buf_size);
   dataSocket_ = std::move(tempDataSocket);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);
   isConnected_ = false;

   setListener(listener);

   // and start thread
   listenThread_ = std::thread(&ZmqDataConnection::listenFunction, this);

   SPDLOG_DEBUG(logger_, "[ZmqDataConnection::openConnection] starting connection for {}"
      , connectionName_);

   return true;
}

bool ZmqDataConnection::ConfigureDataSocket(const ZmqContext::sock_ptr& socket)
{
   int lingerPeriod = 0;
   int result = zmq_setsockopt(socket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[ZmqDataConnection::ConfigureDataSocket] {} failed to set linger interval: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }
   return true;
}

void ZmqDataConnection::listenFunction()
{
   zmq_pollitem_t  poll_items[3];
   memset(&poll_items, 0, sizeof(poll_items));

   poll_items[ZmqDataConnection::ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[ZmqDataConnection::ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[ZmqDataConnection::StreamSocketIndex].socket = dataSocket_.get();
   poll_items[ZmqDataConnection::StreamSocketIndex].events = ZMQ_POLLIN;

   if (monSocket_) {
      poll_items[ZmqDataConnection::MonitorSocketIndex].socket = monSocket_.get();
      poll_items[ZmqDataConnection::MonitorSocketIndex].events = ZMQ_POLLIN;
   }

   SPDLOG_DEBUG(logger_, "[ZmqDataConnection::listenFunction] poll thread started for {}"
      , connectionName_);

   int result;

   while(true) {
      result = zmq_poll(poll_items, monSocket_ ? 3 : 2, -1);
      if (result == -1) {
         logger_->error("[ZmqDataConnection::listenFunction] poll failed for {} : {}"
            , connectionName_, zmq_strerror(zmq_errno()));
         break;
      }

      if (poll_items[ZmqDataConnection::ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recv_result = zmq_msg_recv(&command, poll_items[ZmqDataConnection::ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recv_result == -1) {
            logger_->error("[ZmqDataConnection::listenFunction] failed to recv command on {} : {}"
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         auto command_code = command.ToInt();
         if (command_code == ZmqDataConnection::CommandSend) {
            std::vector<std::string> tmpBuf;
            {
               FastLock locker(lockFlag_);
               tmpBuf = std::move(sendQueue_);
               sendQueue_.clear();
            }
            for (const auto &sendBuf : tmpBuf) {
               int result = zmq_send(dataSocket_.get(), socketId_.c_str(), socketId_.size(), ZMQ_SNDMORE);
               if (result != socketId_.size()) {
                  logger_->error("[ZmqDataConnection::sendRawData] {} failed to send socket id {}"
                     , connectionName_, zmq_strerror(zmq_errno()));
                  continue;
               }

               result = zmq_send(dataSocket_.get(), sendBuf.data(), sendBuf.size(), ZMQ_SNDMORE);
               if (result != sendBuf.size()) {
                  logger_->error("[ZmqDataConnection::sendRawData] {} failed to send data frame {}"
                     , connectionName_, zmq_strerror(zmq_errno()));
                  continue;
               }
            }
         }
         else if (command_code == ZmqDataConnection::CommandStop) {
            break;
         } else {
            logger_->error("[ZmqDataConnection::listenFunction] unexpected command code {} for {}"
               , command_code, connectionName_);
            break;
         }
      }

      if (poll_items[ZmqDataConnection::StreamSocketIndex].revents & ZMQ_POLLIN) {
         if (!recvData()) {
            break;
         }
      }

      if (monSocket_ && (poll_items[ZmqDataConnection::MonitorSocketIndex].revents & ZMQ_POLLIN)) {
         switch (bs::network::get_monitor_event(monSocket_.get())) {
         case ZMQ_EVENT_CONNECTED:
            if (!isConnected_) {
               notifyOnConnected();
               isConnected_ = true;
            }
            break;

         case ZMQ_EVENT_DISCONNECTED:
            if (isConnected_) {
               notifyOnDisconnected();
               isConnected_ = false;
            }
            break;
         }
      }
   }

   if (isConnected_) {
      notifyOnDisconnected();
   }
   SPDLOG_DEBUG(logger_, "[ZmqDataConnection::listenFunction] poll thread stopped for {}", connectionName_);
}

bool ZmqDataConnection::recvData()
{
   // it is client connection. since it is a stream, we will get two frames
   // first - connection ID
   // second - data frame. if data frame is zero length - it means we are connected or disconnected
   MessageHolder id;
   MessageHolder data;

   int result = zmq_msg_recv(&id, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqDataConnection::recvData] {} failed to recv ID frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqDataConnection::recvData] {} failed to recv data frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   if (data.GetSize() == 0) {
      //we are either connected or disconncted
      zeroFrameReceived();
   } else {
      onRawDataReceived(data.ToString());
   }

   return true;
}

void ZmqDataConnection::zeroFrameReceived()
{
   if (isConnected_) {
      SPDLOG_DEBUG(logger_, "[ZmqDataConnection] {} received 0 frame. Disconnected.", connectionName_);
      isConnected_ = false;
      notifyOnDisconnected();
   } else {
      SPDLOG_DEBUG(logger_, "[ZmqDataConnection] {} received 0 frame. Connected.", connectionName_);
      isConnected_ = true;
      notifyOnConnected();
   }
}

bool ZmqDataConnection::closeConnection()
{
   if (!isActive()) {
      SPDLOG_DEBUG(logger_, "[ZmqDataConnection::closeConnection] connection already stopped {}"
         , connectionName_);
      return true;
   }

   SPDLOG_DEBUG(logger_, "[ZmqDataConnection::closeConnection] stopping {}", connectionName_);

   int command = ZmqDataConnection::CommandStop;
   int result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   if (result == -1) {
      logger_->error("[ZmqDataConnection::closeConnection] failed to send stop comamnd for {} : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   listenThread_.join();
   resetConnectionObjects();

   return true;
}

bool ZmqDataConnection::sendRawData(const std::string& rawData)
{
   if (!isActive()) {
      logger_->error("[ZmqDataConnection::sendRawData] could not send. not connected");
      return false;
   }

   {
      FastLock locker(lockFlag_);
      sendQueue_.push_back(rawData);
   }

   int command = ZmqDataConnection::CommandSend;
   FastLock lock(controlSocketLock_);
   int result = zmq_send(threadMasterSocket_.get(), static_cast<void*>(&command), sizeof(command), 0);
   if (result == -1) {
      logger_->error("[ZmqDataConnection::sendRawData] failed to send command for {} : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }
   return true;
}

ZmqContext::sock_ptr ZmqDataConnection::CreateDataSocket()
{
   return context_->CreateStreamSocket();
}

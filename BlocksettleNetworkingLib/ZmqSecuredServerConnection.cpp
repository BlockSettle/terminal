#include "ZmqSecuredServerConnection.h"
#include "FastLock.h"
#include "MessageHolder.h"

#include <zmq.h>
#include <spdlog/spdlog.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif

ZmqSecuredServerConnection::ZmqSecuredServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : ZmqServerConnection(logger, context)
{}

bool ZmqSecuredServerConnection::SetKeyPair(const SecureBinaryData& zmqPubKey
   , const SecureBinaryData& zmqPrvKey)
{
   publicKey_ = zmqPubKey;
   privateKey_ = zmqPrvKey;

   return true;
}

ZmqContext::sock_ptr ZmqSecuredServerConnection::CreateDataSocket()
{
   return context_->CreateServerSocket();
}

bool ZmqSecuredServerConnection::ConfigDataSocket(const ZmqContext::sock_ptr& dataSocket)
{
   if (publicKey_.getSize() == 0 || privateKey_.getSize() == 0) {
      logger_->error("[ZmqSecuredServerConnection::ConfigDataSocket] missing key pair for {}"
         , connectionName_);
      return false;
   }

   int isServer = 1;
   int result = zmq_setsockopt(dataSocket.get(), ZMQ_CURVE_SERVER, &isServer, sizeof(isServer));
   if (result != 0) {
      logger_->error("[ZmqSecuredServerConnection::ConfigDataSocket] {} failed to config socket to be a server : {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_setsockopt(dataSocket.get(), ZMQ_CURVE_SECRETKEY, privateKey_.getCharPtr(), privateKey_.getSize());
   if (result != 0) {
      logger_->error("[ZmqSecuredServerConnection::ConfigDataSocket] {} failed to set server private key: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }
   return ZmqServerConnection::ConfigDataSocket(dataSocket);
}

bool ZmqSecuredServerConnection::ReadFromDataSocket()
{
   MessageHolder clientId;
   MessageHolder data;

   int result = zmq_msg_recv(&clientId, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqSecuredServerConnection::ReadFromDataSocket] {} failed to recv header: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   const auto &clientIdStr = clientId.ToString();
   if (clientInfo_.find(clientIdStr) == clientInfo_.end()) {
#ifdef WIN32
      SOCKET socket = 0;
#else
      int socket = 0;
#endif
      size_t sockSize = sizeof(socket);
      if (zmq_getsockopt(dataSocket_.get(), ZMQ_FD, &socket, &sockSize) == 0) {
         clientInfo_[clientIdStr] = peerAddressString(static_cast<int>(socket));
      }
   }

   result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[ZmqSecuredServerConnection::ReadFromDataSocket] {} failed to recv message data: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   if (!data.IsLast()) {
      logger_->error("[ZmqSecuredServerConnection::ReadFromDataSocket] {} broken protocol"
         , connectionName_);
      return false;
   }

   notifyListenerOnData(clientIdStr, data.ToString());
   return true;
}

bool ZmqSecuredServerConnection::SendDataToClient(const std::string& clientId, const std::string& data, const SendResultCb &cb)
{
   return QueueDataToSend(clientId, data, cb, false);
}

// Copied from ZMQHelperFunctions. Placed here to eliminate file dependence.
std::string ZmqSecuredServerConnection::peerAddressString(int socket)
{
#ifdef WIN32
   SOCKET sock = socket;
   SOCKADDR_IN peerInfo = { 0 };
   int peerLen = sizeof(peerInfo);
#else
   int sock = socket;
   sockaddr_in peerInfo = { 0 };
   socklen_t peerLen = sizeof(peerInfo);
#endif

   const auto rc = getpeername(sock, (sockaddr*)&peerInfo, &peerLen);
   if (rc == 0) {
      return inet_ntoa(peerInfo.sin_addr);
   } else {
      return "Not detected";
   }
}

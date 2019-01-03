#include "ZmqSecuredDataConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "ZMQHelperFunctions.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

ZmqSecuredDataConnection::ZmqSecuredDataConnection(const std::shared_ptr<spdlog::logger>& logger
                                                   , bool monitored)
 : ZmqDataConnection(logger, monitored)
{}

bool ZmqSecuredDataConnection::SetServerPublicKey(const BinaryData& key)
{
   // The incoming key buffer length may need to change later, once the server
   // key is read from a file. For now, we're assuming it's read from a buffer.
   if (key.getSize() != 40) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] invalid length of "
            "server public key: {}", __func__, key.getSize());
      }
      return false;
   }

   std::pair<BinaryData, SecureBinaryData> inKeyPair;
   int result = bs::network::getCurveZMQKeyPair(inKeyPair);
   if (result == -1) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] failed to generate key "
            "pair: {}", __func__, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   publicKey_ = inKeyPair.first;
   privateKey_ = inKeyPair.second;
   serverPublicKey_ = key;

   return true;
}

bool ZmqSecuredDataConnection::ConfigureDataSocket(const ZmqContext::sock_ptr& s)
{
   if (serverPublicKey_.getSize() == 0) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] server public key is "
            "not set", __func__);
      }
      return false;
   }

   int result = zmq_setsockopt(s.get(), ZMQ_CURVE_SERVERKEY
                               , serverPublicKey_.toBinStr().c_str()
                               , CURVEZMQPUBKEYBUFFERSIZE);
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::{}] failed to set server "
         "public key: {}", __func__, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_setsockopt(s.get(), ZMQ_CURVE_PUBLICKEY
                           , publicKey_.toBinStr().c_str()
                           , CURVEZMQPUBKEYBUFFERSIZE);
   if (result != 0) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] failed to set client "
            "public key: {}", __func__, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   result = zmq_setsockopt(s.get(), ZMQ_CURVE_SECRETKEY
                           , privateKey_.toBinStr().c_str()
                           , CURVEZMQPRVKEYBUFFERSIZE);
   if (result != 0) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] failed to set client "
            "private key: {}", __func__ , zmq_strerror(zmq_errno()));
      }
      return false;
   }

   int lingerPeriod = 0;
   result = zmq_setsockopt(s.get(), ZMQ_LINGER, &lingerPeriod
                           , sizeof(lingerPeriod));
   if (result != 0) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] {} failed to set "
            "linger interval: {}", __func__ , connectionName_
            , zmq_strerror(zmq_errno()));
      }
      return false;
   }

   return true;
}

ZmqContext::sock_ptr ZmqSecuredDataConnection::CreateDataSocket()
{
   return context_->CreateClientSocket();
}

bool ZmqSecuredDataConnection::recvData()
{
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] {} failed to recv data "
            "frame from stream: {}" , __func__, connectionName_
            , zmq_strerror(zmq_errno()));
      }
      return false;
   }

   onRawDataReceived(data.ToString());

   return true;
}

bool ZmqSecuredDataConnection::send(const std::string& data)
{
   int result = -1;
   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), data.c_str(), data.size(), 0);
   }
   if (result != (int)data.size()) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] {} failed to send "
            "data: {}", __func__, connectionName_, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   return true;
}

void ZmqSecuredDataConnection::onRawDataReceived(const std::string& rawData)
{
   notifyOnData(rawData);
}

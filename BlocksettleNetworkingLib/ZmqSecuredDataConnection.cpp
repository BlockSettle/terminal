#include "ZmqSecuredDataConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"

#include <zmq.h>
#include <spdlog/spdlog.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif

ZmqSecuredDataConnection::ZmqSecuredDataConnection(const std::shared_ptr<spdlog::logger>& logger
                                                   , bool monitored)
 : ZmqDataConnection(logger, monitored)
{
   char pubKey[41];
   char privKey[41];
   if (zmq_curve_keypair(pubKey, privKey) != 0) {
      throw std::runtime_error("failed to generate CurveZMQ key pair");
   }

   publicKey_ = SecureBinaryData(pubKey);
   privateKey_ = SecureBinaryData(privKey);
}

bool ZmqSecuredDataConnection::SetServerPublicKey(const BinaryData& key)
{
   if (key.getSize() != CURVEZMQPUBKEYBUFFERSIZE) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] invalid length of "
            "server public key ({} bytes).", __func__, key.getSize());
      }
      return false;
   }

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
                               , serverPublicKey_.toCharPtr()
                               , serverPublicKey_.getSize());
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::{}] failed to set server "
         "public key: {}", __func__, zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_setsockopt(s.get(), ZMQ_CURVE_PUBLICKEY
                           , publicKey_.toCharPtr()
                           , publicKey_.getSize());
   if (result != 0) {
      if (logger_) {
         logger_->error("[ZmqSecuredDataConnection::{}] failed to set client "
            "public key: {}", __func__, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   result = zmq_setsockopt(s.get(), ZMQ_CURVE_SECRETKEY
                           , privateKey_.toCharPtr()
                           , privateKey_.getSize());
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

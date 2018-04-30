#include "ZmqSecuredDataConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"

#include <zmq.h>
#include <spdlog/spdlog.h>

ZmqSecuredDataConnection::ZmqSecuredDataConnection(const std::shared_ptr<spdlog::logger>& logger, bool monitored)
 : ZmqDataConnection(logger, monitored)
{}

bool ZmqSecuredDataConnection::SetServerPublicKey(const std::string& key)
{
   if (key.size() != 40) {
      logger_->error("[ZmqSecuredDataConnection::SetServerPublicKey] invalid length of server public key: {}"
         , key.size());
      return false;
   }

   char pubKey[41];
   char prKey[41];

   int result = zmq_curve_keypair(pubKey, prKey);
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::SetServerPublicKey] failed to generate key pair: {}"
          , zmq_strerror(zmq_errno()));
      return false;
   }

   publicKey_ = std::string(pubKey, 41);
   privateKey_ = std::string(prKey, 41);
   serverPublicKey_ = key;

   return true;
}

bool ZmqSecuredDataConnection::ConfigureDataSocket(const ZmqContext::sock_ptr& s)
{
   if (serverPublicKey_.empty()) {
      logger_->error("[ZmqSecuredDataConnection::ConfigureDataSocket] server public key is not set");
      return false;
   }

   int result = zmq_setsockopt (s.get(), ZMQ_CURVE_SERVERKEY, serverPublicKey_.c_str(), 41);
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::ConfigureDataSocket] failed to set server public key: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_setsockopt (s.get(), ZMQ_CURVE_PUBLICKEY, publicKey_.c_str(), 41);
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::ConfigureDataSocket] failed to set client public key: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   result = zmq_setsockopt (s.get(), ZMQ_CURVE_SECRETKEY, privateKey_.c_str(), 41);
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::ConfigureDataSocket] failed to set client private key: {}"
         , zmq_strerror(zmq_errno()));
      return false;
   }

   int lingerPeriod = 0;
   result = zmq_setsockopt (s.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[ZmqSecuredDataConnection::ConfigureDataSocket] {} failed to set linger interval: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
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
      logger_->error("[ZmqSecuredDataConnection::recvData] {} failed to recv data frame from stream: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
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
   if (result != data.size()) {
      logger_->error("[ZmqSecuredDataConnection::send] {} failed to send data: {}"
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   return true;
}

void ZmqSecuredDataConnection::onRawDataReceived(const std::string& rawData)
{
   notifyOnData(rawData);
}

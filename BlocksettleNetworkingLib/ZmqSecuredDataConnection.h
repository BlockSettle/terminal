#ifndef __ZMQ_SECURED_DATA_CONNECTION_H__
#define __ZMQ_SECURED_DATA_CONNECTION_H__

#include <atomic>
#include "ZmqDataConnection.h"

class ZmqSecuredDataConnection : public ZmqDataConnection
{
public:
   ZmqSecuredDataConnection(const std::shared_ptr<spdlog::logger>& logger, bool monitored = false);
   ~ZmqSecuredDataConnection() noexcept override = default;

   ZmqSecuredDataConnection(const ZmqSecuredDataConnection&) = delete;
   ZmqSecuredDataConnection& operator = (const ZmqSecuredDataConnection&) = delete;

   ZmqSecuredDataConnection(ZmqSecuredDataConnection&&) = delete;
   ZmqSecuredDataConnection& operator = (ZmqSecuredDataConnection&&) = delete;

   bool SetServerPublicKey(const std::string& key);

   bool send(const std::string& data) override;

protected:
   bool recvData() override;
   void onRawDataReceived(const std::string& rawData) override;

   ZmqContext::sock_ptr CreateDataSocket() override;
   bool ConfigureDataSocket(const ZmqContext::sock_ptr& socket) override;

private:
   std::string publicKey_;
   std::string privateKey_;
   std::string serverPublicKey_;
   std::atomic_flag lockSocket_ = ATOMIC_FLAG_INIT;
};

#endif // __ZMQ_SECURED_DATA_CONNECTION_H__
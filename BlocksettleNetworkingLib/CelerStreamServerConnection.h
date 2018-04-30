#ifndef __CELER_STREAM_SERVER_CONNECTION_H__
#define __CELER_STREAM_SERVER_CONNECTION_H__

#include "ZmqStreamServerConnection.h"

class CelerStreamServerConnection : public ZmqStreamServerConnection
{
public:
   CelerStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~CelerStreamServerConnection() noexcept = default;

   CelerStreamServerConnection(const CelerStreamServerConnection&) = delete;
   CelerStreamServerConnection& operator = (const CelerStreamServerConnection&) = delete;

   CelerStreamServerConnection(CelerStreamServerConnection&&) = delete;
   CelerStreamServerConnection& operator = (CelerStreamServerConnection&&) = delete;

protected:
   server_connection_ptr CreateActiveConnection() override;
};

#endif // __CELER_STREAM_SERVER_CONNECTION_H__
#ifndef __GENOA_STREAM_SERVER_CONNECTION_H__
#define __GENOA_STREAM_SERVER_CONNECTION_H__

#include "ZmqStreamServerConnection.h"

class GenoaStreamServerConnection : public ZmqStreamServerConnection
{
public:
   GenoaStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context);
   ~GenoaStreamServerConnection() noexcept = default;

   GenoaStreamServerConnection(const GenoaStreamServerConnection&) = delete;
   GenoaStreamServerConnection& operator = (const GenoaStreamServerConnection&) = delete;

   GenoaStreamServerConnection(GenoaStreamServerConnection&&) = delete;
   GenoaStreamServerConnection& operator = (GenoaStreamServerConnection&&) = delete;

protected:
   server_connection_ptr CreateActiveConnection() override;
};

#endif // __GENOA_STREAM_SERVER_CONNECTION_H__
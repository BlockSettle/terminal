#include "GenoaStreamServerConnection.h"

#include "GenoaConnection.h"
#include "ActiveStreamClient.h"

GenoaStreamServerConnection::GenoaStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context)
 : ZmqStreamServerConnection(logger, context)
{}

ZmqStreamServerConnection::server_connection_ptr GenoaStreamServerConnection::CreateActiveConnection()
{
   return std::make_shared<GenoaConnection<ActiveStreamClient>>(logger_);
}
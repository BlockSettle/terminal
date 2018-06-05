#include "CelerStreamServerConnection.h"

#include "CelerClientConnection.h"
#include "ActiveStreamClient.h"

CelerStreamServerConnection::CelerStreamServerConnection(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ZmqContext>& context
      , bool extraLogging)
 : ZmqStreamServerConnection(logger, context, extraLogging)
{}

ZmqStreamServerConnection::server_connection_ptr CelerStreamServerConnection::CreateActiveConnection()
{
   return std::make_shared<CelerClientConnection<ActiveStreamClient>>(logger_);
}
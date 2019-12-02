/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ZEROMQ_CONTEXT_H__
#define __ZEROMQ_CONTEXT_H__

#include <string>
#include <memory>

#include "IdStringGenerator.h"

namespace spdlog
{
   class logger;
}

enum class ZMQTransport
{
   TCPTransport,
   InprocTransport
};


class ZmqContext
{
private:
   using ctx_ptr = std::unique_ptr<void, int (*)(void*)>;

public:
   using sock_ptr = std::unique_ptr<void, int (*)(void*)>;

public:
   ZmqContext(const std::shared_ptr<spdlog::logger>& logger);
   ~ZmqContext() noexcept = default;

   ZmqContext(const ZmqContext&) = delete;
   ZmqContext& operator = (const ZmqContext&) = delete;

   ZmqContext(ZmqContext&&) = delete;
   ZmqContext& operator = (ZmqContext&&) = delete;

   std::string GenerateConnectionName(const std::string& host, const std::string& port);
   std::string GenerateConnectionName(const std::string& endpoint);

   static std::string CreateConnectionEndpoint(ZMQTransport transport, const std::string& host, const std::string& port);
public:
   sock_ptr    CreateInternalControlSocket();
   sock_ptr    CreateMonitorSocket();
   sock_ptr    CreateStreamSocket();

   sock_ptr    CreateServerSocket();
   sock_ptr    CreateClientSocket();

   sock_ptr    CreatePublishSocket();
   sock_ptr    CreateSubscribeSocket();

   static sock_ptr  CreateNullSocket();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   ctx_ptr                          context_;
   std::atomic_flag  lockerFlag_ = ATOMIC_FLAG_INIT;
   IdStringGenerator                idGenerator_;
};

#endif // __ZEROMQ_CONTEXT_H__

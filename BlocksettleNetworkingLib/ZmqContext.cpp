/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ZmqContext.h"

#include <zmq.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "FastLock.h"


ZmqContext::ZmqContext(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
   , context_( nullptr, zmq_ctx_term )
{
   context_.reset(zmq_ctx_new());
   if (!context_) {
      if (logger_) {
         logger_->error("[ZmqContext] failed to initialize ZeroMQ context");
      }
      throw std::runtime_error("Failed to init zeromq");
   }
}

ZmqContext::sock_ptr ZmqContext::CreateInternalControlSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_PAIR), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateMonitorSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_PAIR), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateStreamSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_STREAM), zmq_close };
}

ZmqContext::sock_ptr    ZmqContext::CreateServerSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_ROUTER), zmq_close };
}
ZmqContext::sock_ptr    ZmqContext::CreateClientSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_DEALER), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateNullSocket()
{
   return { nullptr, zmq_close };
}


std::string ZmqContext::GenerateConnectionName(const std::string& host, const std::string& port)
{
   return host + ":" + port + "_" + idGenerator_.getNextId();
}

std::string ZmqContext::GenerateConnectionName(const std::string& endpoint)
{
   return endpoint + "_" + idGenerator_.getNextId();
}

ZmqContext::sock_ptr ZmqContext::CreatePublishSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_XPUB), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateSubscribeSocket()
{
   FastLock lock(lockerFlag_);
   return { zmq_socket(context_.get(), ZMQ_SUB), zmq_close };
}

std::string ZmqContext::CreateConnectionEndpoint(ZMQTransport transport, const std::string& host, const std::string& port)
{
   std::string transportString;
   switch (transport) {
   case ZMQTransport::TCPTransport:
      transportString = "tcp";
      break;
   case ZMQTransport::InprocTransport:
      transportString = "inproc";
      break;
   default:
      return std::string{};
   }
   return transportString + "://" + host + ":" + port;
}

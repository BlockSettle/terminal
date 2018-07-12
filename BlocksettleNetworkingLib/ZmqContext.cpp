#include "ZmqContext.h"

#include <zmq.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

ZmqContext::ZmqContext(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
   , context_( nullptr, zmq_ctx_term )
{
   context_.reset(zmq_ctx_new());
   if (!context_)
   {
      logger_->error("[ZmqContext] failed to initialize ZeroMQ context");
      throw std::runtime_error("Failed to init zeromq");
   }
}

ZmqContext::sock_ptr ZmqContext::CreateInternalControlSocket()
{
   return { zmq_socket(context_.get(), ZMQ_PAIR), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateMonitorSocket()
{
   return { zmq_socket(context_.get(), ZMQ_PAIR), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateStreamSocket()
{
   return { zmq_socket(context_.get(), ZMQ_STREAM), zmq_close };
}

ZmqContext::sock_ptr    ZmqContext::CreateServerSocket()
{
   return { zmq_socket(context_.get(), ZMQ_ROUTER), zmq_close };
}
ZmqContext::sock_ptr    ZmqContext::CreateClientSocket()
{
   return { zmq_socket(context_.get(), ZMQ_DEALER), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateNullSocket()
{
   return { nullptr, zmq_close };
}

std::string ZmqContext::GenerateConnectionName(const std::string& host, const std::string& port)
{
   return host+":"+port+"_" + idGenerator_.getNextId();
}

ZmqContext::sock_ptr ZmqContext::CreatePublishSocket()
{
   return { zmq_socket(context_.get(), ZMQ_PUB), zmq_close };
}

ZmqContext::sock_ptr ZmqContext::CreateSubscribeSocket()
{
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
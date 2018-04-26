#include "CelerFindSocketIdSequence.h"

#include "UpstreamSocketProto.pb.h"
#include "DownstreamSocketProto.pb.h"
#include "NettyCommunication.pb.h"

#include <spdlog/spdlog.h>

using namespace com::celertech::baseserver::api::socket;
using namespace com::celertech::baseserver::api::enums;
using namespace com::celertech::baseserver::api::session;
using namespace com::celertech::baseserver::communication::protobuf;

//FindAllSockets
//MultiResponseMessage
//SocketConfigurationDownstreamEvent

CelerFindSocketIdSequence::CelerFindSocketIdSequence(const std::shared_ptr<spdlog::logger>& logger, uint16_t port)
  : CelerCommandSequence("CelerFindSocketIdSequence",
   {
      { false, nullptr, &CelerFindSocketIdSequence::sendFindAllSocketsRequest }
    , { true, &CelerFindSocketIdSequence::processFindSocketsResponse, nullptr }
   })
   , logger_(logger)
   , port_(port)
   , socketId_(-1)
{}

CelerMessage CelerFindSocketIdSequence::sendFindAllSocketsRequest()
{
   FindAllSockets request;
   request.set_clientrequestid(GetSequenceId());

   CelerMessage message;
   message.messageType = CelerAPI::FindAllSocketsType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerFindSocketIdSequence::processFindSocketsResponse(const CelerMessage& message)
{
   if (message.messageType != CelerAPI::MultiResponseMessageType) {
      logger_->error("[CelerFindSocketIdSequence::processFindSocketsResponse] invalid response type {}", message.messageType);
      return false;
   }

   MultiResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerFindSocketIdSequence::processFindSocketsResponse] failed to parse MultiResponseMessage");
      return false;
   }

   for (int i=0; i < response.payload_size(); ++i) {
      const ResponsePayload& payload = response.payload(i);
      auto payloadType = CelerAPI::GetMessageType(payload.classname());
      if (payloadType != CelerAPI::SocketConfigurationDownstreamEventType) {
         logger_->error("[CelerFindSocketIdSequence::processFindSocketsResponse] invalid payload type {}", payload.classname());
         return false;
      }

      SocketConfigurationDownstreamEvent socketConfig;
      if (socketConfig.ParseFromString(payload.contents())) {
         if (socketConfig.port() == port_) {
            socketId_ = socketConfig.id();
            logger_->debug("[CelerFindSocketIdSequence::processFindSocketsResponse] socket id for port {} : {}", port_, socketId_);
            return true;
         }
      } else {
         logger_->error("[CelerFindSocketIdSequence::processFindSocketsResponse] failed to parse SocketConfigurationDownstreamEvent");
         return false;
      }
   }

   logger_->error("[CelerFindSocketIdSequence::processFindSocketsResponse] socket id for port {} not found", port_);
   return false;
}

void CelerFindSocketIdSequence::SetCallback(const callback_func& callback)
{
   callback_ = callback;
}

bool CelerFindSocketIdSequence::FinishSequence()
{
   if (callback_) {
      callback_(socketId_);
   }

   return true;
}

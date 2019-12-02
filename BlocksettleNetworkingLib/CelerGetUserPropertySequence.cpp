/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerGetUserPropertySequence.h"

#include "UpstreamUserPropertyProto.pb.h"
#include "DownstreamUserPropertyProto.pb.h"
#include "NettyCommunication.pb.h"

#include <spdlog/logger.h>
#include <iostream>

using namespace com::celertech::staticdata::api::user::property;
using namespace com::celertech::baseserver::communication::protobuf;

CelerGetUserPropertySequence::CelerGetUserPropertySequence(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& username
   , const std::string& propertyName
   , const onGetProperty_func& cb)
 : CelerCommandSequence("CelerGetUserPropertySequence",
      {
         { false, nullptr, &CelerGetUserPropertySequence::sendFindPropertyRequest}
         , { true, &CelerGetUserPropertySequence::processGetPropertyResponse, nullptr}
      })
 , logger_(logger)
 , cb_(cb)
 , username_(username)
 , propertyName_(propertyName)
 , id_(-1)
{
}

bool CelerGetUserPropertySequence::FinishSequence()
{
   if (cb_) {
      cb_(value_, id_);
   }

   return true;
}

CelerMessage CelerGetUserPropertySequence::sendFindPropertyRequest()
{
   FindUserPropertyByUsernameAndKey request;
   request.set_username(username_);
   request.set_key(propertyName_);
   request.set_clientrequestid(GetSequenceId());

   CelerMessage message;
   message.messageType = CelerAPI::FindUserPropertyByUsernameAndKeyType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerGetUserPropertySequence::processGetPropertyResponse(const CelerMessage& message)
{
   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerGetUserPropertySequence::processGetPropertyResponse] get invalid message type {} instead of {}"
                     , message.messageType, CelerAPI::SingleResponseMessageType);
      return false;
   }

   SingleResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerGetUserPropertySequence::processGetPropertyResponse] failed to parse massage of type {}", message.messageType);
      return false;
   }

   if (!response.has_payload()) {
      logger_->debug("[CelerGetUserPropertySequence::processGetPropertyResponse] user {} do not have property {}"
         , username_, propertyName_);
      return true;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerGetUserPropertySequence::processGetPropertyResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent event;
   if (!event.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerGetUserPropertySequence::processGetPropertyResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   value_ = event.value();
   if (event.has_id()) {
      id_ = event.id();
   } else {
      id_ = -1;
   }

   return true;
}

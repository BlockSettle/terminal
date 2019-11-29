/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerGetUserIdSequence.h"

#include "UpstreamUserPropertyProto.pb.h"
#include "DownstreamUserPropertyProto.pb.h"
#include "NettyCommunication.pb.h"

#include "CelerPropertiesDefinitions.h"

#include <spdlog/logger.h>
#include <iostream>

using namespace com::celertech::staticdata::api::user::property;
using namespace com::celertech::baseserver::communication::protobuf;

CelerGetUserIdSequence::CelerGetUserIdSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const onGetId_func& cb)
 : CelerCommandSequence("CelerGetUserIdSequence",
      {
         { false, nullptr, &CelerGetUserIdSequence::sendGetUserIdRequest}
         , { true, &CelerGetUserIdSequence::processGetUserIdResponse, nullptr}
      })
 , logger_(logger)
 , cb_(cb)
 , username_(username)
{
}

bool CelerGetUserIdSequence::FinishSequence()
{
   if (cb_)
   {
      cb_(userId_);
   }

   return true;
}

CelerMessage CelerGetUserIdSequence::sendGetUserIdRequest()
{
   FindUserPropertyByUsernameAndKey request;
   request.set_username(username_);
   request.set_key(CelerUserProperties::UserIdPropertyName);
   request.set_clientrequestid(GetSequenceId());

   CelerMessage message;
   message.messageType = CelerAPI::FindUserPropertyByUsernameAndKeyType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerGetUserIdSequence::processGetUserIdResponse(const CelerMessage& message)
{
   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerGetUserIdSequence::processGetUserIdResponse] get invalid message type {} instead of {}"
                     , message.messageType, CelerAPI::SingleResponseMessageType);
      return false;
   }

   SingleResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerGetUserIdSequence::processGetUserIdResponse] failed to parse massage of type {}", message.messageType);
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerGetUserIdSequence::processGetUserIdResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent event;
   if (!event.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerGetUserIdSequence::processGetUserIdResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   userId_ = event.value();

   return true;
}

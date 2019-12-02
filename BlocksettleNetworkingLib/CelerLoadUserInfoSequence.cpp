/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerLoadUserInfoSequence.h"

#include "UpstreamUserPropertyProto.pb.h"
#include "DownstreamUserPropertyProto.pb.h"
#include "NettyCommunication.pb.h"

#include "CelerPropertiesDefinitions.h"

#include <spdlog/logger.h>
#include <iostream>

using namespace com::celertech::staticdata::api::user::property;
using namespace com::celertech::baseserver::communication::protobuf;

CelerLoadUserInfoSequence::CelerLoadUserInfoSequence(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& username
   , const onPropertiesRecvd_func& cb)
 : CelerCommandSequence("CelerLoadUserInfoSequence",
      {
           { false, nullptr, &CelerLoadUserInfoSequence::sendGetUserIdRequest}
         , { true, &CelerLoadUserInfoSequence::processGetPropertyResponse, nullptr}

         , { false, nullptr, &CelerLoadUserInfoSequence::sendGetSubmittedAuthAddressListRequest}
         , { true, &CelerLoadUserInfoSequence::processGetPropertyResponse, nullptr}

         , { false, nullptr, &CelerLoadUserInfoSequence::sendGetSubmittedCCAddressListRequest }
         , { true, &CelerLoadUserInfoSequence::processGetPropertyResponse, nullptr }

         ,{ false, nullptr, &CelerLoadUserInfoSequence::sendGetBitcoinParticipantRequest }
         ,{ true, &CelerLoadUserInfoSequence::processGetPropertyResponse, nullptr }

         ,{ false, nullptr, &CelerLoadUserInfoSequence::sendGetBitcoinDealerRequest }
         ,{ true, &CelerLoadUserInfoSequence::processGetPropertyResponse, nullptr }
       })
 , logger_(logger)
 , cb_(cb)
 , username_(username)
{
}

bool CelerLoadUserInfoSequence::FinishSequence()
{
   if (cb_) {
      cb_(properties_);
   }

   return true;
}

CelerMessage CelerLoadUserInfoSequence::sendGetUserIdRequest()
{
   return getPropertyRequest(CelerUserProperties::UserIdPropertyName);
}

CelerMessage CelerLoadUserInfoSequence::sendGetSubmittedAuthAddressListRequest()
{
   return getPropertyRequest(CelerUserProperties::SubmittedBtcAuthAddressListPropertyName);
}

CelerMessage CelerLoadUserInfoSequence::sendGetSubmittedCCAddressListRequest()
{
   return getPropertyRequest(CelerUserProperties::SubmittedCCAddressListPropertyName);
}

CelerMessage CelerLoadUserInfoSequence::sendGetBitcoinParticipantRequest()
{
   return getPropertyRequest(CelerUserProperties::BitcoinParticipantPropertyName);
}

CelerMessage CelerLoadUserInfoSequence::sendGetBitcoinDealerRequest()
{
   return getPropertyRequest(CelerUserProperties::BitcoinDealerPropertyName);
}

CelerMessage CelerLoadUserInfoSequence::getPropertyRequest(const std::string& name)
{
   FindUserPropertyByUsernameAndKey request;
   request.set_username(username_);
   request.set_key(name);
   request.set_clientrequestid(GetSequenceId());

   CelerMessage message;
   message.messageType = CelerAPI::FindUserPropertyByUsernameAndKeyType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerLoadUserInfoSequence::processGetPropertyResponse(const CelerMessage& message)
{
   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerLoadUserInfoSequence::processGetPropertyResponse] get invalid message type {} instead of {}"
                     , message.messageType, CelerAPI::SingleResponseMessageType);
      return false;
   }

   SingleResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerLoadUserInfoSequence::processGetPropertyResponse] failed to parse massage of type {}", message.messageType);
      return false;
   }

   if (!response.has_payload()) {
      return true;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerLoadUserInfoSequence::processGetPropertyResponse] unexpected type {} for class {}"
         , payloadType, response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent event;
   if (!event.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerLoadUserInfoSequence::processGetPropertyResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   CelerProperty property(event.key());

   property.value = event.value();
   if (event.has_id()) {
      property.id = event.id();
   } else {
      property.id = -1;
   }
   properties_.emplace(event.key(), property);

   return true;
}

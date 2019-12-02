/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerSetUserPropertySequence.h"

#include "DownstreamExceptionProto.pb.h"
#include "DownstreamUserPropertyProto.pb.h"
#include "UpstreamUserPropertyProto.pb.h"
#include "NettyCommunication.pb.h"

#include <spdlog/spdlog.h>

using namespace com::celertech::staticdata::api::user::property;
using namespace com::celertech::baseserver::communication::protobuf;

CelerSetUserPropertySequence::CelerSetUserPropertySequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& username
      , const CelerProperty& property)
 : CelerCommandSequence("CelerSetUserPropertySequence",
   {
      { false, nullptr, &CelerSetUserPropertySequence::sendSetPropertyRequest },
      { true, &CelerSetUserPropertySequence::processSetPropertyResponse, nullptr }
   })
 , logger_(logger)
 , userName_(username)
 , property_(property)
 , result_(false)
{}

CelerMessage CelerSetUserPropertySequence::sendSetPropertyRequest()
{
   CelerMessage message;
   // work around - for now we always "create" property, cause update is not working
   // don't like commented code as well :(

   // if (property_.id == -1) {
      CreateUserPropertyRequest request;

      request.set_username(userName_);
      request.set_clientrequestid(GetSequenceId());
      request.set_key(property_.name);
      request.set_value(property_.value);

      message.messageType = CelerAPI::CreateUserPropertyRequestType;
      message.messageData = request.SerializeAsString();

      logger_->debug("[CelerSetUserPropertySequence::sendSetPropertyRequest] {}"
         , request.DebugString());
   // } else {
   //    UpdateUserPropertyRequest request;

   //    request.set_clientrequestid(GetSequenceId());
   //    request.set_id(property_.id);
   //    request.set_username(userName_);
   //    request.set_key(property_.name);
   //    request.set_value(property_.value);

   //    message.messageType = CelerAPI::UpdateUserPropertyRequestType;
   //    message.messageData = request.SerializeAsString();

   //    logger_->debug("[CelerSetUserPropertySequence::sendSetPropertyRequest] {}"
   //       , request.DebugString());
   // }

   return message;
}

bool CelerSetUserPropertySequence::processSetPropertyResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] failed to parse massage of type {}"
         , message.messageType);
      return false;
   }

   const auto payloadMessageType = CelerAPI::GetMessageType(response.payload().classname());

   if (payloadMessageType != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] wrong response payload type: {}. UserPropertyDownstreamEventType Expected"
         , response.payload().classname());

      if (payloadMessageType == CelerAPI::PersistenceExceptionType) {
         com::celertech::baseserver::api::exception::PersistenceException exceptionMessage;
         if (!exceptionMessage.ParseFromString(response.payload().contents())) {
            logger_->error("[CelerClient::OnDataReceived] failed to parse PersistenceException");
         } else {
            logger_->error("[CelerClient::OnDataReceived] get PersistenceException: {}"
               , exceptionMessage.DebugString());
         }
      }

      return false;
   }

   UserPropertyDownstreamEvent setPropertyResponse;
   if (!setPropertyResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   if (setPropertyResponse.username() != userName_) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] response for different user.");
      return false;
   }

   if (setPropertyResponse.key() != property_.name) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] response for different property name.");
      return false;
   }

   if (setPropertyResponse.value() != property_.value) {
      logger_->error("[CelerSetUserPropertySequence::processSetPropertyResponse] value not set.");
      return false;
   }

   property_.id = setPropertyResponse.id();
   result_ = true;

   return true;
}

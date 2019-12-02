/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerLoginSequence.h"

#include "DownstreamLoginProto.pb.h"
#include "UpstreamLoginProto.pb.h"
#include "NettyCommunication.pb.h"

#include <cassert>

namespace spdlog
{
   class logger;
}

using namespace com::celertech::baseserver::communication::login;
using namespace com::celertech::baseserver::communication::protobuf;


CelerLoginSequence::CelerLoginSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const std::string& password)
   : CelerCommandSequence("CelerLoginSequence",
      {
         { false, nullptr, &CelerLoginSequence::sendLoginRequest},
         { true, &CelerLoginSequence::processLoginResponse, nullptr},
         { true, &CelerLoginSequence::processConnectedEvent, nullptr},
      })
   , logger_(logger)
   , username_(username)
   , password_(password)
{}

void CelerLoginSequence::SetCallbackFunctions(const onLoginSuccess_func& onSuccess, const onLoginFailed_func& onFailed)
{
   onLoginSuccess_ = onSuccess;
   onLoginFailed_ = onFailed;
}

CelerMessage CelerLoginSequence::sendLoginRequest()
{
   LoginRequest loginRequest;

   // This message will be replaced on BsProxy but we still need to do whole login sequence as usual
   loginRequest.set_username(username_);
   loginRequest.set_password(password_);

   CelerMessage message;
   message.messageType = CelerAPI::LoginRequestType;
   message.messageData = loginRequest.SerializeAsString();

   return message;
}

bool CelerLoginSequence::processLoginResponse(const CelerMessage& message)
{
   LoginResponse response;

   if (message.messageType != CelerAPI::LoginResponseType) {
      logger_->error("[CelerLoginSequence::processLoginResponse] get invalid message type {} instead of {}"
                     , message.messageType, CelerAPI::LoginResponseType);
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerLoginSequence::processLoginResponse] failed to parse LoginResponse");
      return false;
   }

   if (!response.loggedin()) {
      if (response.has_message()) {
         errorMessage_ = response.message();
      }

      return false;
   }

   sessionToken_ = response.sessiontoken();
   return true;
}

bool CelerLoginSequence::processConnectedEvent(const CelerMessage& message)
{
   ConnectedEvent response;

   if (message.messageType != CelerAPI::ConnectedEventType) {
      logger_->error("[CelerLoginSequence::processConnectedEvent] get invalid message type {} instead of {}"
                     , message.messageType, CelerAPI::ConnectedEventType);
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerLoginSequence::processConnectedEvent] failed to parse LoginResponse");
      return false;
   }

   heartbeatInterval_ = std::chrono::seconds(response.heartbeatintervalinsecs());

   return true;
}

bool CelerLoginSequence::FinishSequence()
{
   if (IsSequenceFailed()) {
      assert(onLoginFailed_);
      onLoginFailed_(errorMessage_);
   } else {
      assert(onLoginSuccess_);
      onLoginSuccess_(sessionToken_, heartbeatInterval_);
   }
   return true;
}

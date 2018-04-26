#include "CelerCreateUserSequence.h"

#include "DownstreamUserPropertyProto.pb.h"
#include "UpstreamAuthenticationUserProto.pb.h"
#include "UpstreamSessionProto.pb.h"
#include "UpstreamUserPropertyProto.pb.h"
#include "NettyCommunication.pb.h"
#include "DownstreamAuthenticationUserProto.pb.h"

#include "CelerPropertiesDefinitions.h"

#include <spdlog/spdlog.h>

using namespace com::celertech::baseserver::api::user;
using namespace com::celertech::baseserver::api::session;
using namespace com::celertech::staticdata::api::user::property;
using namespace com::celertech::baseserver::communication::protobuf;

static const std::string MarketSessionDefaultValue = "OMS_BITCOIN";
static const std::string SocketAccessDefaultValue = "MARKETMAKER";

CelerCreateUserSequence::CelerCreateUserSequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& username
      , const std::string& password
      , const std::string& userId
      , int64_t socketId)
 : CelerCommandSequence("CelerCreateUserSequence",
   {
      { false, nullptr, &CelerCreateUserSequence::sendFindUserRequest },
      { true, &CelerCreateUserSequence::processFindUserResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendCreateUserRequest },
      { true, &CelerCreateUserSequence::processCreateUserResponse, nullptr },
      // properties 
      { false, nullptr, &CelerCreateUserSequence::sendChangeUserSocketRequest },
      { true, &CelerCreateUserSequence::processChangeUserSocketResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendSetUserIdRequest },
      { true, &CelerCreateUserSequence::processSetUserIdResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendSetSocketAccessRequest },
      { true, &CelerCreateUserSequence::processSetSocketAccessResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendSetMarketSessionRequest },
      { true, &CelerCreateUserSequence::processSetMarketSessionResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendTradingDisabledRequest },
      { true, &CelerCreateUserSequence::processTradingDisabledResponse, nullptr },

      { false, nullptr, &CelerCreateUserSequence::sendResetPasswordRequest },
      { true, &CelerCreateUserSequence::processResetPasswordResponse, nullptr },
      { false, nullptr, &CelerCreateUserSequence::sendChangePasswordRequest},
      { true, &CelerCreateUserSequence::processChangePasswordResponse, nullptr }
   })
 , logger_(logger)
 , userName_(username)
 , password_(password)
 , userID_(userId)
 , socketId_(socketId)
 , createUserStatus_(SequenceNotCompleted)
{}

CelerMessage CelerCreateUserSequence::sendFindUserRequest()
{
   FindStandardUser request;

   request.set_clientrequestid(GetSequenceId());
   request.set_username(userName_);

   CelerMessage message;
   message.messageType = CelerAPI::FindStandardUserType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendCreateUserRequest()
{
   CreateStandardUser request;

   request.set_clientrequestid(GetSequenceId());

   request.set_username(userName_);
   request.set_email(userName_);
   request.set_passwordvaliddays(100);
   request.set_passwordexpirydateinmillis(10000000);
   request.set_userloginpolicytype(com::celertech::baseserver::api::enums::userloginpolicytype::DISCONNECT_PREVIOUS_SESSION);
   request.set_enabled(true);
   request.set_expired(false);
   request.set_locked(false);
   request.set_passwordexpired(false);

   CelerMessage message;
   message.messageType = CelerAPI::CreateStandardUserType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendChangeUserSocketRequest()
{
   CreateApiSessionRequest request;

   request.set_clientrequestid(GetSequenceId());
   request.set_sessionkey(userName_);
   request.set_socketconfigurationid(socketId_);
   request.set_enabled(true);

   CelerMessage message;
   message.messageType = CelerAPI::CreateApiSessionRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendSetUserIdRequest()
{
   CreateUserPropertyRequest request;

   request.set_username(userName_);
   request.set_clientrequestid(GetSequenceId());
   request.set_key(CelerUserProperties::UserIdPropertyName);
   request.set_value(userID_);

   CelerMessage message;
   message.messageType = CelerAPI::CreateUserPropertyRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendSetSocketAccessRequest()
{
   CreateUserPropertyRequest request;

   request.set_username(userName_);
   request.set_clientrequestid(GetSequenceId());
   request.set_key(CelerUserProperties::SocketAccessPropertyName);
   request.set_value(SocketAccessDefaultValue);

   CelerMessage message;
   message.messageType = CelerAPI::CreateUserPropertyRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendSetMarketSessionRequest()
{
   CreateUserPropertyRequest request;

   request.set_username(userName_);
   request.set_clientrequestid(GetSequenceId());
   request.set_key(CelerUserProperties::MarketSessionPropertyName);
   request.set_value(MarketSessionDefaultValue);

   CelerMessage message;
   message.messageType = CelerAPI::CreateUserPropertyRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendTradingDisabledRequest()
{
   CreateUserPropertyRequest request;

   request.set_username(userName_);
   request.set_clientrequestid(GetSequenceId());
   request.set_key(CelerUserProperties::BitcoinParticipantPropertyName);
   request.set_value(CelerUserProperties::DisabledPropertyValue);

   CelerMessage message;
   message.messageType = CelerAPI::CreateUserPropertyRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendResetPasswordRequest()
{
   GenerateResetUserPasswordTokenRequest request;

   request.set_clientrequestid(GetSequenceId());
   request.set_username(userName_);

   CelerMessage message;
   message.messageType = CelerAPI::GenerateResetUserPasswordTokenRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerCreateUserSequence::sendChangePasswordRequest()
{
   ChangeUserPasswordRequest request;

   request.set_clientrequestid(GetSequenceId());
   request.set_username(userName_);
   request.set_newpassword(password_);
   request.set_resetpasswordtoken(token_);

   CelerMessage message;
   message.messageType = CelerAPI::ChangeUserPasswordRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerCreateUserSequence::processFindUserResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   logger_->debug("[CelerCreateUserSequence::processFindUserResponse]");

   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerCreateUserSequence::processFindUserResponse] invalid response type {}", message.messageType);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processFindUserResponse] failed to parse response");
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (response.has_payload()) {
      logger_->error("[CelerCreateUserSequence::processFindUserResponse] user {} already exists", userName_);
      createUserStatus_ = UserExistsStatus;
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processCreateUserResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   logger_->debug("[CelerCreateUserSequence::processCreateUserResponse]");

   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerCreateUserSequence::processCreateUserResponse] invalid response type {}", message.messageType);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processCreateUserResponse] failed to parse response");
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!response.has_payload()) {
      logger_->error("[CelerCreateUserSequence::processCreateUserResponse] empty response for create user {}", userName_);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (response.payload().classname() != "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$StandardUserDownstreamEvent") {
      logger_->error("[CelerCreateUserSequence::processCreateUserResponse] unexpected payload class: {}", response.payload().classname());
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processChangeUserSocketResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (message.messageType != CelerAPI::SingleResponseMessageType) {
      logger_->error("[CelerCreateUserSequence::processChangeUserSocketResponse] invalid response type {}", message.messageType);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processChangeUserSocketResponse] failed to parse response");
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!response.has_payload()) {
      logger_->error("[CelerCreateUserSequence::processChangeUserSocketResponse] empty response for create user {}", userName_);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processSetUserIdResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] failed to parse massage of type {}"
         , message.messageType);
      return false;
   }

   if (CelerAPI::GetMessageType(response.payload().classname()) != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] wrong response payload type: {}. UserPropertyDownstreamEventType Expected"
         , response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent setPropertyResponse;
   if (!setPropertyResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   if (setPropertyResponse.username() != userName_) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] response for different user.");
      return false;
   }

   if (setPropertyResponse.key() != CelerUserProperties::UserIdPropertyName) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] response for different property name.");
      return false;
   }

   if (setPropertyResponse.value() != userID_) {
      logger_->error("[CelerCreateUserSequence::processSetUserIdResponse] value not set.");
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processSetSocketAccessResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] failed to parse massage of type {}"
         , message.messageType);
      return false;
   }

   if (CelerAPI::GetMessageType(response.payload().classname()) != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] wrong response payload type: {}. UserPropertyDownstreamEventType Expected"
         , response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent setPropertyResponse;
   if (!setPropertyResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   if (setPropertyResponse.username() != userName_) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] response for different user.");
      return false;
   }

   if (setPropertyResponse.key() != CelerUserProperties::SocketAccessPropertyName) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] response for different property name.");
      return false;
   }

   if (setPropertyResponse.value() != SocketAccessDefaultValue) {
      logger_->error("[CelerCreateUserSequence::processSetSocketAccessResponse] value not set.");
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processSetMarketSessionResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] failed to parse massage of type {}"
         , message.messageType);
      return false;
   }

   if (CelerAPI::GetMessageType(response.payload().classname()) != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] wrong response payload type: {}. UserPropertyDownstreamEventType Expected"
         , response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent setPropertyResponse;
   if (!setPropertyResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   if (setPropertyResponse.username() != userName_) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] response for different user.");
      return false;
   }

   if (setPropertyResponse.key() != CelerUserProperties::MarketSessionPropertyName) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] response for different property name.");
      return false;
   }

   if (setPropertyResponse.value() != MarketSessionDefaultValue) {
      logger_->error("[CelerCreateUserSequence::processSetMarketSessionResponse] value not set.");
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processTradingDisabledResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] failed to parse massage of type {}"
         , message.messageType);
      return false;
   }

   if (CelerAPI::GetMessageType(response.payload().classname()) != CelerAPI::UserPropertyDownstreamEventType) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] wrong response payload type: {}. UserPropertyDownstreamEventType Expected"
         , response.payload().classname());
      return false;
   }

   UserPropertyDownstreamEvent setPropertyResponse;
   if (!setPropertyResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] failed to parse UserPropertyDownstreamEvent");
      return false;
   }

   if (setPropertyResponse.username() != userName_) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] response for different user.");
      return false;
   }

   if (setPropertyResponse.key() != CelerUserProperties::BitcoinParticipantPropertyName) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] response for different property name.");
      return false;
   }

   if (setPropertyResponse.value() != CelerUserProperties::DisabledPropertyValue) {
      logger_->error("[CelerCreateUserSequence::processTradingDisabledResponse] value not set.");
      return false;
   }

   return true;
}

bool CelerCreateUserSequence::processResetPasswordResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processResetPasswordResponse] failed to parse massage of type {}", message.messageType);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::ResetUserPasswordTokenType) {
      logger_->error("[CelerCreateUserSequence::processResetPasswordResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   ResetUserPasswordToken resetPasswordResponse;
   if (!resetPasswordResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processResetPasswordResponse] failed to parse ResetUserPasswordToken");
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   token_ = resetPasswordResponse.resetuserpasswordtoken();
   return true;
}

bool CelerCreateUserSequence::processChangePasswordResponse(const CelerMessage &message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerCreateUserSequence::processChangePasswordResponse] failed to parse massage of type {}", message.messageType);
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::ChangeUserPasswordConfirmationType) {
      logger_->error("[CelerCreateUserSequence::processChangePasswordResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   ChangeUserPasswordConfirmation confirmation;
   if (!confirmation.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerCreateUserSequence::processChangePasswordResponse] failed to parse ChangeUserPasswordConfirmation");
      createUserStatus_ = ServerErrorStatus;
      return false;
   }

   if (!confirmation.succeeded()) {
      logger_->error("[CelerCreateUserSequence::processChangePasswordResponse] failed to change psssword for {}", userName_);
      for (int i=0; i < confirmation.message_size(); ++i) {
         logger_->error("[CelerCreateUserSequence::processChangePasswordResponse] confirmation message[{}] : {}", i, confirmation.message(i));
      }

      createUserStatus_ = ServerErrorStatus;

      return false;
   }

   logger_->debug("[CelerCreateUserSequence::processChangePasswordResponse] user {} created and password updated", userName_);

   createUserStatus_ = UserCreatedStatus;
   return true;
}

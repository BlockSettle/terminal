#include "CelerChangePasswordSequence.h"

#include "UpstreamAuthenticationUserProto.pb.h"
#include "DownstreamAuthenticationUserProto.pb.h"
#include "NettyCommunication.pb.h"

#include <spdlog/spdlog.h>

using namespace com::celertech::baseserver::api::user;
using namespace com::celertech::baseserver::communication::protobuf;

CelerChangePasswordSequence::CelerChangePasswordSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const std::string& password)
   : CelerCommandSequence("CelerChangePasswordSequence",
      {
         { false, nullptr, &CelerChangePasswordSequence::sendResetPasswordRequest }
       , { true, &CelerChangePasswordSequence::processResetPasswordResponse, nullptr }
       , { false, nullptr, &CelerChangePasswordSequence::sendChangePasswordRequest}
       , { true, &CelerChangePasswordSequence::processChangePasswordResponse, nullptr }
      })
   , logger_(logger)
   , username_(username)
   , password_(password)
   , changePasswordResult_(false)
{}

bool CelerChangePasswordSequence::FinishSequence()
{
   assert(callback_);

   callback_(changePasswordResult_);

   return true;
}

CelerMessage CelerChangePasswordSequence::sendResetPasswordRequest()
{
   GenerateResetUserPasswordTokenRequest request;

   request.set_clientrequestid(GetSequenceId());
   request.set_username(username_);

   CelerMessage message;
   message.messageType = CelerAPI::GenerateResetUserPasswordTokenRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerChangePasswordSequence::processResetPasswordResponse( const CelerMessage& message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerChangePasswordSequence::processResetPasswordResponse] failed to parse massage of type {}", message.messageType);
      changePasswordResult_ = false;
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::ResetUserPasswordTokenType) {
      logger_->error("[CelerChangePasswordSequence::processResetPasswordResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      changePasswordResult_ = false;
      return false;
   }

   ResetUserPasswordToken resetPasswordResponse;
   if (!resetPasswordResponse.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerChangePasswordSequence::processResetPasswordResponse] failed to parse ResetUserPasswordToken");
      changePasswordResult_ = false;
      return false;
   }

   token_ = resetPasswordResponse.resetuserpasswordtoken();
   return true;
}

CelerMessage CelerChangePasswordSequence::sendChangePasswordRequest()
{
   ChangeUserPasswordRequest request;

   request.set_clientrequestid(GetSequenceId());
   request.set_username(username_);
   request.set_newpassword(password_);
   request.set_resetpasswordtoken(token_);

   CelerMessage message;
   message.messageType = CelerAPI::ChangeUserPasswordRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerChangePasswordSequence::processChangePasswordResponse(const CelerMessage &message)
{
   SingleResponseMessage response;

   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerChangePasswordSequence::processChangePasswordResponse] failed to parse massage of type {}", message.messageType);
      changePasswordResult_ = false;
      return false;
   }

   auto payloadType = CelerAPI::GetMessageType(response.payload().classname());
   if (payloadType != CelerAPI::ChangeUserPasswordConfirmationType) {
      logger_->error("[CelerChangePasswordSequence::processChangePasswordResponse] unexpected type {} for class {}", payloadType, response.payload().classname());
      changePasswordResult_ = false;
      return false;
   }

   ChangeUserPasswordConfirmation confirmation;
   if (!confirmation.ParseFromString(response.payload().contents())) {
      logger_->error("[CelerChangePasswordSequence::processChangePasswordResponse] failed to parse ChangeUserPasswordConfirmation");
      changePasswordResult_ = false;
      return false;
   }

   if (!confirmation.succeeded()) {
      logger_->error("[CelerChangePasswordSequence::processChangePasswordResponse] failed to change psssword for {}", username_);
      for (int i=0; i < confirmation.message_size(); ++i) {
         logger_->error("[CelerChangePasswordSequence::processChangePasswordResponse] confirmation message[{}] : {}", i, confirmation.message(i));
      }

      changePasswordResult_ = false;
      return false;
   }

   logger_->debug("[CelerChangePasswordSequence::processChangePasswordResponse] password changed for user: {}", username_);

   changePasswordResult_ = true;
   return true;
}

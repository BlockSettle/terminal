#ifndef __CELER_CREATE_USER_SEQUENCE_H__
#define __CELER_CREATE_USER_SEQUENCE_H__

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

#include "CelerCommandSequence.h"

class CelerCreateUserSequence : public CelerCommandSequence<CelerCreateUserSequence>
{
public:
   enum CreateUserStatus {
      SequenceNotCompleted,
      UserCreatedStatus,
      UserExistsStatus,
      ServerErrorStatus
   };

   using callback_func = std::function<void (CreateUserStatus status)>;

public:
   CelerCreateUserSequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& username
      , const std::string &password, const std::string& userId
      , int64_t socketId);
   ~CelerCreateUserSequence() noexcept = default;

   CelerCreateUserSequence(const CelerCreateUserSequence&) = delete;
   CelerCreateUserSequence& operator = (const CelerCreateUserSequence&) = delete;

   CelerCreateUserSequence(CelerCreateUserSequence&&) = delete;
   CelerCreateUserSequence& operator = (CelerCreateUserSequence&&) = delete;

   void SetCallback(const callback_func& callback) {
      callback_ = callback;
   }

   bool FinishSequence() override {
      if (callback_) {
         callback_(createUserStatus_);
      }
      return true;
   }
private:
   CelerMessage sendFindUserRequest();
   CelerMessage sendCreateUserRequest();

   CelerMessage sendChangeUserSocketRequest();
   CelerMessage sendSetUserIdRequest();
   CelerMessage sendSetSocketAccessRequest();
   CelerMessage sendSetMarketSessionRequest();
   CelerMessage sendTradingDisabledRequest();

   CelerMessage sendResetPasswordRequest();
   CelerMessage sendChangePasswordRequest();

   bool processFindUserResponse( const CelerMessage& message);
   bool processCreateUserResponse( const CelerMessage& message);

   bool processChangeUserSocketResponse( const CelerMessage& message);
   bool processSetUserIdResponse( const CelerMessage& message);
   bool processSetSocketAccessResponse( const CelerMessage& message);
   bool processSetMarketSessionResponse( const CelerMessage& message);
   bool processTradingDisabledResponse( const CelerMessage& message);

   bool processResetPasswordResponse( const CelerMessage& message);
   bool processChangePasswordResponse( const CelerMessage& message);
private:
   std::shared_ptr<spdlog::logger> logger_;

   std::string userName_;
   std::string password_;
   std::string userID_;
   std::string token_;
   int64_t     socketId_;

   CreateUserStatus  createUserStatus_;
   callback_func     callback_;
};

#endif // __CELER_CREATE_USER_SEQUENCE_H__

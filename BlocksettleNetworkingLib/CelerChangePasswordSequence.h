#ifndef __CELER_CHANGE_PASSWORD_SEQUENCE_H__
#define __CELER_CHANGE_PASSWORD_SEQUENCE_H__

#include "CelerCommandSequence.h"

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

class CelerChangePasswordSequence : public CelerCommandSequence<CelerChangePasswordSequence>
{
public:
   using callback_function = std::function< void (bool result)>;

public:
   CelerChangePasswordSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const std::string& password);
   ~CelerChangePasswordSequence() noexcept override = default;

   CelerChangePasswordSequence(const CelerChangePasswordSequence&) = delete;
   CelerChangePasswordSequence& operator = (const CelerChangePasswordSequence&) = delete;

   CelerChangePasswordSequence(CelerChangePasswordSequence&&) = delete;
   CelerChangePasswordSequence& operator = (CelerChangePasswordSequence&&) = delete;

   bool FinishSequence() override;

   void SetCallback(const callback_function& callback) { callback_ = callback; }

private:
   CelerMessage sendResetPasswordRequest();
   CelerMessage sendChangePasswordRequest();

   bool processResetPasswordResponse( const CelerMessage& message);
   bool processChangePasswordResponse( const CelerMessage& message);

   std::shared_ptr<spdlog::logger> logger_;
   std::string username_;
   std::string password_;
   std::string token_;

   bool changePasswordResult_;

   callback_function callback_;
};

#endif // __CELER_CHANGE_PASSWORD_SEQUENCE_H__

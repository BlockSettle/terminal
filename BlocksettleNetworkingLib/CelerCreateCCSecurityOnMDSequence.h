#ifndef __CELER_CREATE_CC_SECURITY_ON_MD_SERVER_H__
#define __CELER_CREATE_CC_SECURITY_ON_MD_SERVER_H__

#include "CelerCommandSequence.h"

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

class CelerCreateCCSecurityOnMDSequence : public CelerCommandSequence<CelerCreateCCSecurityOnMDSequence>
{
public:
   using callback_function = std::function< void (bool result, const std::string& securityId)>;

public:
   CelerCreateCCSecurityOnMDSequence(const std::string& securityId
      , const std::string& exchangeId
      , const callback_function& callback
      , const std::shared_ptr<spdlog::logger>& logger);
   ~CelerCreateCCSecurityOnMDSequence() noexcept override = default;

   CelerCreateCCSecurityOnMDSequence(const CelerCreateCCSecurityOnMDSequence&) = delete;
   CelerCreateCCSecurityOnMDSequence& operator = (const CelerCreateCCSecurityOnMDSequence&) = delete;

   CelerCreateCCSecurityOnMDSequence(CelerCreateCCSecurityOnMDSequence&&) = delete;
   CelerCreateCCSecurityOnMDSequence& operator = (CelerCreateCCSecurityOnMDSequence&&) = delete;

   bool FinishSequence() override;
private:
   CelerMessage sendRequest();

   bool processResponse(const CelerMessage& message);

private:
   std::string GetExchangeId() const;

private:
   const std::string                securityId_;
   const std::string                exchangeId_;
   callback_function                callback_;
   std::shared_ptr<spdlog::logger>  logger_;

   bool        result_ = false;
};

#endif // __CELER_CREATE_CC_SECURITY_ON_MD_SERVER_H__

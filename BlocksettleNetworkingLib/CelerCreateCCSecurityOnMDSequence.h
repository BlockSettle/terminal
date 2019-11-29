/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   CelerCreateCCSecurityOnMDSequence(const std::string& securityId
      , const std::string& exchangeId
      , const std::shared_ptr<spdlog::logger>& logger);
   ~CelerCreateCCSecurityOnMDSequence() noexcept override = default;

   CelerCreateCCSecurityOnMDSequence(const CelerCreateCCSecurityOnMDSequence&) = delete;
   CelerCreateCCSecurityOnMDSequence& operator = (const CelerCreateCCSecurityOnMDSequence&) = delete;

   CelerCreateCCSecurityOnMDSequence(CelerCreateCCSecurityOnMDSequence&&) = delete;
   CelerCreateCCSecurityOnMDSequence& operator = (CelerCreateCCSecurityOnMDSequence&&) = delete;

   bool FinishSequence() override;
private:
   CelerMessage sendRequest();

private:
   std::string GetExchangeId() const;

private:
   const std::string                securityId_;
   const std::string                exchangeId_;
   std::shared_ptr<spdlog::logger>  logger_;
};

#endif // __CELER_CREATE_CC_SECURITY_ON_MD_SERVER_H__

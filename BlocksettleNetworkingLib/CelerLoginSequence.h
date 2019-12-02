/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_LOGIN_SEQUENCE_H__
#define __CELER_LOGIN_SEQUENCE_H__

#include "CelerCommandSequence.h"

#include <chrono>
#include <functional>
#include <string>

#include <spdlog/spdlog.h>

class CelerLoginSequence : public CelerCommandSequence<CelerLoginSequence>
{
public:
   using onLoginSuccess_func = std::function< void (const std::string& sessionToken, std::chrono::seconds heartbeatInterval)>;
   using onLoginFailed_func = std::function< void (const std::string& errorMessage)>;

public:
   CelerLoginSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const std::string& password);
   ~CelerLoginSequence() noexcept override = default;

   CelerLoginSequence(const CelerLoginSequence&) = delete;
   CelerLoginSequence& operator = (const CelerLoginSequence&) = delete;

   CelerLoginSequence(CelerLoginSequence&&) = delete;
   CelerLoginSequence& operator = (CelerLoginSequence&&) = delete;

   bool FinishSequence() override;

   void SetCallbackFunctions(const onLoginSuccess_func &onSuccess, const onLoginFailed_func &onFailed);

private:
   CelerMessage sendLoginRequest();
   bool         processLoginResponse(const CelerMessage& );
   bool         processConnectedEvent(const CelerMessage& );

   std::shared_ptr<spdlog::logger> logger_;
   std::string username_;
   std::string password_;

   std::string errorMessage_;
   std::chrono::seconds heartbeatInterval_;
   std::string sessionToken_;

   onLoginFailed_func   onLoginFailed_;
   onLoginSuccess_func  onLoginSuccess_;
};

#endif // __CELER_LOGIN_SEQUENCE_H__

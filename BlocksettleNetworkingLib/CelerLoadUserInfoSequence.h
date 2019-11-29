/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_LOAD_USER_INFO_SEQUENCE_H__
#define __CELER_LOAD_USER_INFO_SEQUENCE_H__

#include "CelerCommandSequence.h"
#include "CelerProperty.h"

#include <memory>
#include <functional>

namespace spdlog
{
   class logger;
}

class CelerLoadUserInfoSequence : public CelerCommandSequence<CelerLoadUserInfoSequence>
{
public:
   using onPropertiesRecvd_func = std::function< void (CelerProperties properties)>;

   CelerLoadUserInfoSequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& username
      , const onPropertiesRecvd_func& cb);
   ~CelerLoadUserInfoSequence() = default;

   bool FinishSequence() override;

private:
   CelerMessage sendGetUserIdRequest();
   CelerMessage sendGetSubmittedAuthAddressListRequest();
   CelerMessage sendGetSubmittedCCAddressListRequest();
   CelerMessage sendGetBitcoinParticipantRequest();
   CelerMessage sendGetBitcoinDealerRequest();

   CelerMessage   getPropertyRequest(const std::string& name);
   bool           processGetPropertyResponse(const CelerMessage& message);

private:
   std::shared_ptr<spdlog::logger> logger_;
   onPropertiesRecvd_func  cb_;
   const std::string username_;
   CelerProperties   properties_;
};

#endif // __CELER_LOAD_USER_INFO_SEQUENCE_H__

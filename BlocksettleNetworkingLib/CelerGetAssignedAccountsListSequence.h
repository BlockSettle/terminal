/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_GET_ASSIGNED_ACCOUNTS_H__
#define __CELER_GET_ASSIGNED_ACCOUNTS_H__

#include "CelerCommandSequence.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spdlog
{
   class logger;
}

class CelerGetAssignedAccountsListSequence : public CelerCommandSequence<CelerGetAssignedAccountsListSequence>
{
public:
   using onGetAccountListFunc = std::function< void (const std::vector<std::string>& assignedAccounts)>;

   CelerGetAssignedAccountsListSequence(const std::shared_ptr<spdlog::logger>& logger
      , const onGetAccountListFunc& cb);
   ~CelerGetAssignedAccountsListSequence() = default;

   bool FinishSequence() override;

   CelerMessage sendFindAccountRequest();
   bool         processFindAccountResponse(const CelerMessage& );

private:
   std::shared_ptr<spdlog::logger> logger_;
   onGetAccountListFunc cb_;

   std::vector<std::string> assignedAccounts_;
};

#endif // __CELER_GET_ASSIGNED_ACCOUNTS_H__
/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CELERGETUSERIDSEQUENCE_H
#define CELERGETUSERIDSEQUENCE_H

#include "CelerCommandSequence.h"
#include <memory>
#include <functional>

namespace spdlog
{
   class logger;
}

class CelerGetUserIdSequence : public CelerCommandSequence<CelerGetUserIdSequence>
{
public:
   using onGetId_func = std::function< void (const std::string& userId)>;

   CelerGetUserIdSequence(const std::shared_ptr<spdlog::logger>& logger, const std::string& username, const onGetId_func& cb);
   ~CelerGetUserIdSequence() = default;

   bool FinishSequence() override;
   CelerMessage sendGetUserIdRequest();
   bool         processGetUserIdResponse(const CelerMessage& );

private:
   std::shared_ptr<spdlog::logger> logger_;
   onGetId_func cb_;
   std::string username_;
   std::string userId_;
};

#endif // CELERGETUSERIDSEQUENCE_H

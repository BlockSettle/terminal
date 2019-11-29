/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_LOAD_DEFINITIONS_FROM_MD_SERVER_H__
#define __CELER_LOAD_DEFINITIONS_FROM_MD_SERVER_H__

#include "CelerCommandSequence.h"

#include <string>
#include <memory>

namespace spdlog
{
   class logger;
}

class CelerLoadMDDefinitionsSequence : public CelerCommandSequence<CelerLoadMDDefinitionsSequence>
{

public:
   CelerLoadMDDefinitionsSequence(const std::shared_ptr<spdlog::logger>& logger);
   ~CelerLoadMDDefinitionsSequence() noexcept override = default;

   CelerLoadMDDefinitionsSequence(const CelerLoadMDDefinitionsSequence&) = delete;
   CelerLoadMDDefinitionsSequence& operator = (const CelerLoadMDDefinitionsSequence&) = delete;

   CelerLoadMDDefinitionsSequence(CelerLoadMDDefinitionsSequence&&) = delete;
   CelerLoadMDDefinitionsSequence& operator = (CelerLoadMDDefinitionsSequence&&) = delete;

   bool FinishSequence() override;
private:
   CelerMessage sendRequest();

   bool processResponse(const CelerMessage& message);

private:
   std::shared_ptr<spdlog::logger>  logger_;
};

#endif // __CELER_LOAD_DEFINITIONS_FROM_MD_SERVER_H__
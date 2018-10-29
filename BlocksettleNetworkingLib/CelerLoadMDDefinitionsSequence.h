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
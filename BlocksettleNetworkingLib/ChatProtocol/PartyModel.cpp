#include "PartyModel.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

#include "ChatProtocol/PrivateDirectMessageParty.h"

namespace Chat
{

   PartyModel::PartyModel(const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : QObject(parent), loggerPtr_(loggerPtr)
   {
      connect(this, &PartyModel::error, this, &PartyModel::handleLocalErrors);
   }

   void PartyModel::insertParty(const PartyPtr& partyPtr)
   {
      if (partyMap_.find(partyPtr->id()) != partyMap_.end())
      {
         PartyPtr oldPartyPtr = partyMap_[partyPtr->id()];
         partyMap_.erase(partyPtr->id());

         emit partyRemoved(oldPartyPtr);
         emit error(PartyModelError::InsertExistingParty, partyPtr->id());
      }

      partyMap_[partyPtr->id()] = partyPtr;

      emit partyInserted(partyPtr);
   }

   void PartyModel::removeParty(const PartyPtr& partyPtr)
   {
      if (partyMap_.find(partyPtr->id()) != partyMap_.end())
      {
         PartyPtr oldPartyPtr = partyMap_[partyPtr->id()];
         partyMap_.erase(partyPtr->id());

         emit partyRemoved(oldPartyPtr);
         return;
      }

      emit error(PartyModelError::RemovingNonexistingParty, partyPtr->id());
   }

   PartyPtr PartyModel::getPartyById(const std::string& id)
   {
      const auto it = partyMap_.find(id);

      if (it != partyMap_.end())
      {
         return it->second;
      }

      emit error(PartyModelError::CouldNotFindParty, id);

      return nullptr;
   }

   void PartyModel::handleLocalErrors(const Chat::PartyModelError& errorCode, const std::string& id)
   {
      loggerPtr_->debug("[PartyModel::handleLocalErrors] Error: {}, what: {}", (int)errorCode, id);
   }

   void PartyModel::clearModel()
   {
      for (const auto& element : partyMap_)
      {
         emit partyRemoved(element.second);
      }

      partyMap_.clear();
   }
}
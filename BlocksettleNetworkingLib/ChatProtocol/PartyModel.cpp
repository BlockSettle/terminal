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

   PrivateDirectMessagePartyPtr PartyModel::getPrivatePartyById(const std::string& id)
   {
      PartyPtr partyPtr = getPartyById(id);

      if (!partyPtr)
      {
         emit error(PartyModelError::CouldNotFindParty, id);
         return nullptr;
      }

      PrivateDirectMessagePartyPtr privateDMPartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);

      if (nullptr == privateDMPartyPtr)
      {
         // this should not happen
         emit error(PartyModelError::PrivatePartyCasting, id);
         return nullptr;
      }

      return privateDMPartyPtr;
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

   void PartyModel::insertOrUpdateParty(const PartyPtr& partyPtr)
   {
      // private party
      if (PartyType::PRIVATE_DIRECT_MESSAGE == partyPtr->partyType() && PartySubType::STANDARD == partyPtr->partySubType())
      {
         PrivateDirectMessagePartyPtr privatePartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);
         PrivateDirectMessagePartyPtr existingPartyPtr = getPrivatePartyById(partyPtr->id());

         // party not exist, insert
         if (nullptr == existingPartyPtr)
         {
            insertParty(privatePartyPtr);
            return;
         }

         // party exist, update
         for (const auto recipientPtr : privatePartyPtr->recipients())
         {
            existingPartyPtr->insertOrUpdateRecipient(recipientPtr);
         }
         return;
      }

      // other party types
      PartyPtr existingPartyPtr = getPrivatePartyById(partyPtr->id());

      // if not exist, insert new, otherwise do nothing
      if (nullptr == existingPartyPtr)
      {
         insertParty(partyPtr);
         return;
      }
   }
}
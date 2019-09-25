#include "PartyModel.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

#include "ChatProtocol/PrivateDirectMessageParty.h"

using namespace Chat;

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

      emit partyRemoved(oldPartyPtr);

      partyMap_.erase(partyPtr->id());

      emit partyModelChanged();
      emit error(PartyModelError::InsertExistingParty, partyPtr->id());
   }

   partyMap_[partyPtr->id()] = partyPtr;

   emit partyInserted(partyPtr);
   emit partyModelChanged();
}

void PartyModel::removeParty(const PartyPtr& partyPtr)
{
   if (partyMap_.find(partyPtr->id()) != partyMap_.end())
   {
      PartyPtr oldPartyPtr = partyMap_[partyPtr->id()];

      emit partyRemoved(oldPartyPtr);

      partyMap_.erase(partyPtr->id());

      emit partyModelChanged();
      return;
   }

   emit error(PartyModelError::RemovingNonexistingParty, partyPtr->id(), true);
}

PartyPtr PartyModel::getPartyById(const std::string& party_id)
{
   const auto it = partyMap_.find(party_id);

   if (it != partyMap_.end())
   {
      return it->second;
   }

   emit error(PartyModelError::CouldNotFindParty, party_id);

   return nullptr;
}

PrivateDirectMessagePartyPtr PartyModel::getPrivatePartyById(const std::string& party_id)
{
   PartyPtr partyPtr = getPartyById(party_id);

   if (!partyPtr)
   {
      emit error(PartyModelError::CouldNotFindParty, party_id);
      return nullptr;
   }

   PrivateDirectMessagePartyPtr privateDMPartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);

   if (nullptr == privateDMPartyPtr)
   {
      // this should not happen
      emit error(PartyModelError::PrivatePartyCasting, party_id);
      return nullptr;
   }

   return privateDMPartyPtr;
}

void PartyModel::handleLocalErrors(const Chat::PartyModelError& errorCode, const std::string& what, bool displayAsWarning)
{
   const std::string displayAs = displayAsWarning ? WarningDescription : ErrorDescription;

   loggerPtr_->debug("[PartyModel::handleLocalErrors] {}: {}, what: {}", displayAs, (int)errorCode, what);
}

void PartyModel::clearModel()
{
   for (const auto& element : partyMap_)
   {
      emit partyRemoved(element.second);
   }

   partyMap_.clear();
   emit partyModelChanged();
}

void PartyModel::insertOrUpdateParty(const PartyPtr& partyPtr)
{
   // private party
   if (partyPtr->isPrivateStandard())
   {
      PrivateDirectMessagePartyPtr privatePartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);

      if (nullptr == privatePartyPtr)
      {
         emit error(PartyModelError::DynamicPointerCast, partyPtr->id());
         return;
      }

      PrivateDirectMessagePartyPtr existingPartyPtr = getPrivatePartyById(partyPtr->id());

      // party not exist, insert
      if (nullptr == existingPartyPtr)
      {
         insertParty(privatePartyPtr);
         return;
      }

      // party exist, update
      for (const auto &recipientPtr : privatePartyPtr->recipients())
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


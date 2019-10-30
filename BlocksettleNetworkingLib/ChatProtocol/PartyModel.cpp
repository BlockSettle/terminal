#include "PartyModel.h"
#include "FastLock.h"

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
   PartyPtr oldPartyPtr{};

   {
      FastLock locker(partyMapLockerFlag_);
      const auto it = partyMap_.find(partyPtr->id());
      if (it != partyMap_.end())
      {
         oldPartyPtr = partyMap_[partyPtr->id()];
         partyMap_.erase(partyPtr->id());
         emit error(PartyModelError::InsertExistingParty, partyPtr->id(), true);
      }

      partyMap_[partyPtr->id()] = partyPtr;
   }

   if (oldPartyPtr)
   {
      emit partyRemoved(oldPartyPtr);
   }

   emit partyInserted(partyPtr);
   emit partyModelChanged();
}

void PartyModel::removeParty(const PartyPtr& partyPtr)
{
   if (!partyPtr)
   {
      return;
   }

   PartyPtr oldPartyPtr{};
   auto it = partyMap_.end();
   auto isErased = false;

   {
      FastLock locker(partyMapLockerFlag_);
      it = partyMap_.find(partyPtr->id());
      if (it != partyMap_.end())
      {
         oldPartyPtr = partyMap_[partyPtr->id()];
         partyMap_.erase(partyPtr->id());
         isErased = true;
      }
   }

   if (oldPartyPtr)
   {
      emit partyRemoved(oldPartyPtr);
      emit partyModelChanged();
   }

   if (!isErased)
   {
      emit error(PartyModelError::RemovingNonexistingParty, partyPtr->id(), true);
   }
}

PartyPtr PartyModel::getPartyById(const std::string& party_id)
{
   {
      FastLock locker(partyMapLockerFlag_);

      const auto it = partyMap_.find(party_id);

      if (it != partyMap_.end())
      {
         return it->second;
      }      
   }

   emit error(PartyModelError::CouldNotFindParty, party_id, true);

   return nullptr;
}

PrivateDirectMessagePartyPtr PartyModel::getPrivatePartyById(const std::string& party_id)
{
   PartyPtr partyPtr = getPartyById(party_id);

   if (!partyPtr)
   {
      emit error(PartyModelError::CouldNotFindParty, party_id, true);
      return nullptr;
   }

   PrivateDirectMessagePartyPtr privateDMPartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);

   if (nullptr == privateDMPartyPtr)
   {
      // this should not happen
      emit error(PartyModelError::PrivatePartyCasting, party_id, true);
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
   // copy party id's to new list
   std::vector<std::string> partyIds;
   partyIds.reserve(partyMap_.size());
   std::transform(partyMap_.begin(), partyMap_.end(), std::back_inserter(partyIds),
      [](decltype(partyMap_)::value_type const& pair)
   {
      return pair.first;
   });

   // then remove one by one
   for (const auto& element : partyIds)
   {
      const auto& partyPtr = getPartyById(element);
      removeParty(partyPtr);
   }
}

void PartyModel::insertOrUpdateParty(const PartyPtr& partyPtr)
{
   // private party
   if (partyPtr->isPrivate())
   {
      PrivateDirectMessagePartyPtr privatePartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);

      if (nullptr == privatePartyPtr)
      {
         emit error(PartyModelError::DynamicPointerCast, partyPtr->id(), true);
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
   PartyPtr existingPartyPtr = getPartyById(partyPtr->id());

   // if not exist, insert new, otherwise do nothing
   if (nullptr == existingPartyPtr)
   {
      insertParty(partyPtr);
   }
}


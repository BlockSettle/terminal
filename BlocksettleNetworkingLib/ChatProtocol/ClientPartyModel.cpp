#include "ChatProtocol/ClientPartyModel.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

using namespace Chat;

ClientPartyModel::ClientPartyModel(const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
   : PartyModel(loggerPtr, parent)
{
   qRegisterMetaType<Chat::PrivatePartyState>();
   qRegisterMetaType<Chat::ClientPartyModelError>();

   connect(this, &ClientPartyModel::error, this, &ClientPartyModel::handleLocalErrors);
   connect(this, &ClientPartyModel::partyInserted, this, &ClientPartyModel::handlePartyInserted);
   connect(this, &ClientPartyModel::partyRemoved, this, &ClientPartyModel::handlePartyRemoved);
}

IdPartyList ClientPartyModel::getIdPartyList() const
{
   IdPartyList idPartyList;
   for (const auto& element : partyMap_)
   {
      idPartyList.push_back(element.first);
   }

   return idPartyList;
}

IdPartyList ClientPartyModel::getIdPrivatePartyList()
{
   IdPartyList idPartyList;
   for (const auto& element : partyMap_)
   {
      if (element.second->isPrivate())
      {
         idPartyList.push_back(element.first);
      }
   }

   return idPartyList;
}

IdPartyList ClientPartyModel::getIdPrivatePartyListBySubType(const PartySubType& partySubType)
{
   IdPartyList idPartyList;
   for (const auto& element : partyMap_)
   {
      if (!element.second->isPrivate())
      {
         continue;
      }

      if (element.second->partySubType() == partySubType)
      {
         idPartyList.push_back(element.first);
      }
   }

   return idPartyList;
}

ClientPartyPtrList ClientPartyModel::getClientPartyListForRecipient(const IdPartyList& idPartyList, const std::string& recipientUserHash)
{
   ClientPartyPtrList clientPartyPtrList;

   for (const auto& partyId : idPartyList)
   {
      const ClientPartyPtr clientPartyPtr = getClientPartyById(partyId);

      if (nullptr == clientPartyPtr)
      {
         continue;
      }

      if (clientPartyPtr->isUserBelongsToParty(recipientUserHash))
      {
         clientPartyPtrList.push_back(clientPartyPtr);
      }
   }

   if (clientPartyPtrList.empty())
   {
      emit error(ClientPartyModelError::UserNameNotFound, recipientUserHash, true);
   }

   return clientPartyPtrList;
}

ClientPartyPtrList ClientPartyModel::getStandardPrivatePartyListForRecipient(const std::string& recipientUserHash)
{
   const IdPartyList idPartyList = getIdPrivatePartyListBySubType();
   
   return getClientPartyListForRecipient(idPartyList, recipientUserHash);
}

ClientPartyPtrList ClientPartyModel::getOtcPrivatePartyListForRecipient(const std::string& recipientUserHash)
{
   const IdPartyList idPartyList = getIdPrivatePartyListBySubType(PartySubType::OTC);

   return getClientPartyListForRecipient(idPartyList, recipientUserHash);
}

ClientPartyPtr ClientPartyModel::getClientPartyForRecipients(const ClientPartyPtrList& clientPartyPtrList, const std::string& firstUserHash, const std::string& secondUserHash)
{
   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      if (clientPartyPtr->isUserInPartyWith(firstUserHash, secondUserHash))
      {
         return clientPartyPtr;
      }
   }

   return nullptr;
}

ClientPartyPtr ClientPartyModel::getStandardPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash)
{
   Chat::ClientPartyPtrList clientPartyPtrList = getStandardPrivatePartyListForRecipient(secondUserHash);

   return getClientPartyForRecipients(clientPartyPtrList, firstUserHash, secondUserHash);
}

ClientPartyPtr ClientPartyModel::getOtcPartyForUsers(const std::string& firstUserHash, const std::string& secondUserHash)
{
   Chat::ClientPartyPtrList clientPartyPtrList = getOtcPrivatePartyListForRecipient(secondUserHash);

   return getClientPartyForRecipients(clientPartyPtrList, firstUserHash, secondUserHash);
}


/*
ClientPartyPtr ClientPartyModel::getClientPartyByUserName(const std::string& userName)
{
   const IdPartyList idPartyList = getIdPartyList();

   for (const auto& partyId : idPartyList)
   {
      const ClientPartyPtr clientPartyPtr = getClientPartyById(partyId);

      if (nullptr == clientPartyPtr)
      {
         continue;
      }

      if (clientPartyPtr->isUserBelongsToParty(userName))
      {
         return clientPartyPtr;
      }
   }

   emit error(ClientPartyModelError::UserNameNotFound, userName);

   return nullptr;
}
*/

void ClientPartyModel::handleLocalErrors(const Chat::ClientPartyModelError& errorCode, const std::string& what, bool displayAsWarning)
{
   const std::string displayAs = displayAsWarning ? WarningDescription : ErrorDescription;

   loggerPtr_->debug("[ClientPartyModel::handleLocalErrors] {}: {}, what: {}", displayAs, (int)errorCode, what);
}

void ClientPartyModel::handlePartyInserted(const PartyPtr& partyPtr)
{
   const ClientPartyPtr clientPartyPtr = getClientPartyById(partyPtr->id());

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   connect(clientPartyPtr.get(), &ClientParty::clientStatusChanged, this, &ClientPartyModel::handlePartyStatusChanged);
   connect(clientPartyPtr.get(), &ClientParty::partyStateChanged, this, &ClientPartyModel::handlePartyStateChanged);
   connect(clientPartyPtr.get(), &ClientParty::displayNameChanged, this, &ClientPartyModel::handleDisplayNameChanged);
}

void ClientPartyModel::handlePartyRemoved(const PartyPtr& partyPtr)
{
   const ClientPartyPtr clientPartyPtr = getClientPartyById(partyPtr->id());

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   disconnect(clientPartyPtr.get(), &ClientParty::displayNameChanged, this, &ClientPartyModel::handleDisplayNameChanged);
   disconnect(clientPartyPtr.get(), &ClientParty::partyStateChanged, this, &ClientPartyModel::handlePartyStateChanged);
   disconnect(clientPartyPtr.get(), &ClientParty::clientStatusChanged, this, &ClientPartyModel::handlePartyStatusChanged);
}

void ClientPartyModel::handlePartyStatusChanged(const ClientStatus&)
{
   ClientParty* clientParty = qobject_cast<ClientParty*>(sender());

   if (!clientParty)
   {
      emit error(ClientPartyModelError::QObjectCast);
      return;
   }

   const ClientPartyPtr clientPartyPtr = getClientPartyById(clientParty->id());

   if (!clientPartyPtr)
   {
      return;
   }

   emit clientPartyStatusChanged(clientPartyPtr);
}

ClientPartyPtr ClientPartyModel::castToClientPartyPtr(const PartyPtr& partyPtr)
{
   const ClientPartyPtr clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

   if (!clientPartyPtr)
   {
      emit error(ClientPartyModelError::DynamicPointerCast, partyPtr->id(), true);
      return nullptr;
   }

   return clientPartyPtr;
}

ClientPartyPtr ClientPartyModel::getClientPartyById(const std::string& party_id)
{
   const PartyPtr partyPtr = getPartyById(party_id);

   if (nullptr == partyPtr)
   {
      emit error(ClientPartyModelError::PartyNotFound, party_id, true);
      return nullptr;
   }

   const ClientPartyPtr clientPartyPtr = castToClientPartyPtr(partyPtr);

   return clientPartyPtr;
}

void ClientPartyModel::handlePartyStateChanged(const std::string& partyId)
{
   emit partyStateChanged(partyId);
   emit partyModelChanged();
}

PrivatePartyState ClientPartyModel::deducePrivatePartyStateForUser(const std::string& userName)
{
   const ClientPartyPtrList clientPartyPtrList = getStandardPrivatePartyListForRecipient(userName);

   if (clientPartyPtrList.empty())
   {
      return PrivatePartyState::Unknown;
   }

   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      if (!clientPartyPtr->isUserBelongsToParty(userName))
      {
         continue;
      }

      const PartyState partyState = clientPartyPtr->partyState();

      if (PartyState::UNINITIALIZED == partyState)
      {
         return PrivatePartyState::Uninitialized;
      }

      if (PartyState::REQUESTED == partyState)
      {
         if (userName == ownUserName_)
         {
            return PrivatePartyState::RequestedOutgoing;
         }
         else
         {
            return PrivatePartyState::RequestedIncoming;
         }
      }

      if (PartyState::REJECTED == partyState || ownUserName_ == userName)
      {
         return PrivatePartyState::Rejected;
      }

      return PrivatePartyState::Initialized;
   }

   return PrivatePartyState::Unknown;
}

void ClientPartyModel::handleDisplayNameChanged()
{
   ClientParty* clientParty = qobject_cast<ClientParty*>(sender());

   if (!clientParty)
   {
      emit error(ClientPartyModelError::QObjectCast);
      return;
   }

   emit clientPartyDisplayNameChanged(clientParty->id());
}

ClientPartyPtr ClientPartyModel::getClientPartyByCreatorHash(const std::string& creatorHash)
{
   const std::function<bool(const ClientPartyPtr&)> compareCb = [creatorHash](const ClientPartyPtr& cp) {
      return cp->partyCreatorHash() == creatorHash; 
   };

   const ClientPartyPtr clientPartyPtr = getClientPartyByHash(compareCb);

   if (nullptr == clientPartyPtr)
   {
      emit error(ClientPartyModelError::PartyCreatorHashNotFound, creatorHash);
   }

   return clientPartyPtr;
}

ClientPartyPtr ClientPartyModel::getClientPartyByUserHash(const std::string& userHash, const bool isPrivateOTC)
{
   const std::function<bool(const ClientPartyPtr&)> compareCb = [userHash, isPrivateOTC](const ClientPartyPtr& cp) {
      if (isPrivateOTC)
      {
         if (cp->isPrivateOTC() && cp->userHash() == userHash)
         {
            return true;
         }
         
         return false;
      }

      // don't return private otc in standard search
      if (cp->isPrivateOTC())
      {
         return false;
      }

      return cp->userHash() == userHash; 
   };

   const ClientPartyPtr clientPartyPtr = getClientPartyByHash(compareCb);

   if (nullptr == clientPartyPtr)
   {
      emit error(ClientPartyModelError::UserHashNotFound, userHash);
   }

   return clientPartyPtr;
}

ClientPartyPtr ClientPartyModel::getClientPartyByHash(const std::function<bool(const ClientPartyPtr&)>& compareCb)
{
   const IdPartyList idPartyList = getIdPartyList();

   for (const auto& partyId : idPartyList)
   {
      const ClientPartyPtr clientPartyPtr = getClientPartyById(partyId);

      if (clientPartyPtr && compareCb(clientPartyPtr))
      {
         return clientPartyPtr;
      }
   }

   return nullptr;
}

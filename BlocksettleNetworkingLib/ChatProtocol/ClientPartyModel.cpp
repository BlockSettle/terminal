#include "ChatProtocol/ClientPartyModel.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   ClientPartyModel::ClientPartyModel(const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : PartyModel(loggerPtr, parent)
   {
      connect(this, &ClientPartyModel::error, this, &ClientPartyModel::handleLocalErrors);
      connect(this, &ClientPartyModel::partyInserted, this, &ClientPartyModel::handlePartyInserted);
      connect(this, &ClientPartyModel::partyRemoved, this, &ClientPartyModel::handlePartyRemoved);
   }

   IdPartyList ClientPartyModel::getIdPartyList() const
   {
      IdPartyList idPartyList;
      for (const std::pair<std::string, PartyPtr>& element : partyMap_)
      {
         idPartyList.push_back(element.first);
      }

      return idPartyList;
   }

   ClientPartyPtr ClientPartyModel::getPartyByUserName(const std::string& userName)
   {
      IdPartyList idPartyList = getIdPartyList();

      for (const auto& partyId : idPartyList)
      {
         ClientPartyPtr clientPartyPtr = getClientPartyById(partyId);

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

   void ClientPartyModel::handleLocalErrors(const ClientPartyModelError& errorCode, const std::string& what)
   {
      loggerPtr_->debug("[ClientPartyModel::handleLocalErrors] Error: {}, what: {}", (int)errorCode, what);
   }

   void ClientPartyModel::handlePartyInserted(const PartyPtr& partyPtr)
   {
      ClientPartyPtr clientPartyPtr = getClientPartyById(partyPtr->id());

      if (nullptr == clientPartyPtr)
      {
         return;
      }

      connect(clientPartyPtr.get(), &ClientParty::clientStatusChanged, this, &ClientPartyModel::handlePartyStatusChanged);
      connect(clientPartyPtr.get(), &ClientParty::partyStateChanged, this, &ClientPartyModel::handlePartyStateChanged);
   }

   void ClientPartyModel::handlePartyRemoved(const PartyPtr& partyPtr)
   {
      ClientPartyPtr clientPartyPtr = getClientPartyById(partyPtr->id());

      if (nullptr == clientPartyPtr)
      {
         return;
      }

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

      ClientPartyPtr clientPartyPtr = getClientPartyById(clientParty->id());

      if (!clientPartyPtr)
      {
         return;
      }

      emit clientPartyStatusChanged(clientPartyPtr);
   }

   ClientPartyPtr ClientPartyModel::castToClientPartyPtr(const PartyPtr& partyPtr)
   {
      ClientPartyPtr clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

      if (!clientPartyPtr)
      {
         emit error(ClientPartyModelError::DynamicPointerCast, partyPtr->id());
         return nullptr;
      }

      return clientPartyPtr;
   }

   ClientPartyPtr ClientPartyModel::getClientPartyById(const std::string& party_id)
   {
      PartyPtr partyPtr = getPartyById(party_id);

      if (nullptr == partyPtr)
      {
         emit error(ClientPartyModelError::PartyNotFound, party_id);
         return nullptr;
      }

      ClientPartyPtr clientPartyPtr = castToClientPartyPtr(partyPtr);

      return clientPartyPtr;
   }

   void ClientPartyModel::handlePartyStateChanged(const std::string& partyId)
   {
      emit partyStateChanged(partyId);
      emit partyModelChanged();
   }

}
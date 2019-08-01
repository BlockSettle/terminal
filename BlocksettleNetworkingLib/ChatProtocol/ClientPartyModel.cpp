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
         PartyPtr partyPtr = getPartyById(partyId);

         if (partyPtr->partyType() == PartyType::PRIVATE_DIRECT_MESSAGE)
         {
            ClientPartyPtr clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

            if (!clientPartyPtr)
            {
               emit error(ClientPartyModelError::DynamicPointerCast, userName);
               continue;
            }

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

}
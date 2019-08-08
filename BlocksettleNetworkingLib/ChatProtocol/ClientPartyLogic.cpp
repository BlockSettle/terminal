#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientParty.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   ClientPartyLogic::ClientPartyLogic(const LoggerPtr& loggerPtr, const ClientDBServicePtr& clientDBServicePtr, QObject* parent) : QObject(parent)
   {
      qRegisterMetaType<Chat::ClientPartyLogicError>();

      clientDBServicePtr_ = clientDBServicePtr;
      clientPartyModelPtr_ = std::make_shared<ClientPartyModel>(loggerPtr, this);
      connect(this, &ClientPartyLogic::error, this, &ClientPartyLogic::handleLocalErrors);
   }

   void ClientPartyLogic::handlePartiesFromWelcomePacket(const google::protobuf::Message& msg)
   {
      clientPartyModelPtr_->clearModel();

      WelcomeResponse welcomeResponse;
      welcomeResponse.CopyFrom(msg);

      for (int i = 0; i < welcomeResponse.party_size(); i++)
      {
         const PartyPacket& partyPacket = welcomeResponse.party(i);

         if (partyPacket.party_type() == PartyType::GLOBAL)
         {
            ClientPartyPtr clientPartyPtr = std::make_shared<ClientParty>(
               partyPacket.id(), partyPacket.party_type(), partyPacket.party_subtype(), partyPacket.party_state());

            clientPartyPtr->setDisplayName(partyPacket.display_name());

            clientPartyModelPtr_->insertParty(clientPartyPtr);
         }
      }

      emit partyModelChanged();
   }

   void ClientPartyLogic::onUserStatusChanged(const std::string& userName, const ClientStatus& clientStatus)
   {
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr_->getPartyByUserName(userName);

      if (clientPartyPtr == nullptr)
      {
         emit error(ClientPartyLogicError::NonexistentClientStatusChanged, userName);
         return;
      }

      clientPartyPtr->setClientStatus(clientStatus);
   }

   void ClientPartyLogic::handleLocalErrors(const ClientPartyLogicError& errorCode, const std::string& what)
   {
      loggerPtr_->debug("[ClientPartyLogic::handleLocalErrors] Error: {}, what: {}", (int)errorCode, what);
   }

}
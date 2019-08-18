#include <QUuid>

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
      connect(clientDBServicePtr.get(), &ClientDBService::messageInserted, clientPartyModelPtr_.get(), &ClientPartyModel::messageInserted);
      connect(clientDBServicePtr.get(), &ClientDBService::messageStateChanged, clientPartyModelPtr_.get(), &ClientPartyModel::messageStateChanged);

      connect(clientPartyModelPtr_.get(), &ClientPartyModel::partyInserted, this, &ClientPartyLogic::handlePartyInserted);
   }

   void ClientPartyLogic::handlePartiesFromWelcomePacket(const google::protobuf::Message& msg)
   {
      clientPartyModelPtr_->clearModel();

      WelcomeResponse welcomeResponse;
      welcomeResponse.CopyFrom(msg);

      for (int i = 0; i < welcomeResponse.party_size(); i++)
      {
         const PartyPacket& partyPacket = welcomeResponse.party(i);

         ClientPartyPtr clientPartyPtr = std::make_shared<ClientParty>(
            partyPacket.party_id(), partyPacket.party_type(), partyPacket.party_subtype(), partyPacket.party_state());
         clientPartyPtr->setDisplayName(partyPacket.display_name());

         if (PartyType::PRIVATE_DIRECT_MESSAGE == partyPacket.party_type() && PartySubType::STANDARD == partyPacket.party_subtype())
         {
            PartyRecipientsPtrList recipients;
            for (int i = 0; i < partyPacket.recipient_size(); i++)
            {
               PartyRecipientPacket recipient = partyPacket.recipient(i);
               PartyRecipientPtr recipientPtr =
                  std::make_shared<PartyRecipient>(recipient.user_name(), recipient.public_key());
               recipients.push_back(recipientPtr);
            }

            clientPartyPtr->setRecipients(recipients);
         }

         clientPartyModelPtr_->insertParty(clientPartyPtr);
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

      // don't change status for other than private parties
      if (PartyType::PRIVATE_DIRECT_MESSAGE != clientPartyPtr->partyType())
      {
         return;
      }

      clientPartyPtr->setClientStatus(clientStatus);
   }

   void ClientPartyLogic::handleLocalErrors(const ClientPartyLogicError& errorCode, const std::string& what)
   {
      loggerPtr_->debug("[ClientPartyLogic::handleLocalErrors] Error: {}, what: {}", (int)errorCode, what);
   }

   void ClientPartyLogic::handlePartyInserted(const Chat::PartyPtr& partyPtr)
   {
      clientDBServicePtr_->createNewParty(partyPtr->id());
   }

   void ClientPartyLogic::createPrivateParty(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName)
   {
      // check if private party exist
      IdPartyList idPartyList = clientPartyModelPtr_->getIdPartyList();

      for (const auto& partyId : idPartyList)
      {
         ClientPartyPtr clientPartyPtr = clientPartyModelPtr_->getClientPartyById(partyId);
         if (!clientPartyPtr)
         {
            continue;
         }

         if (PartyType::PRIVATE_DIRECT_MESSAGE != clientPartyPtr->partyType() || PartySubType::STANDARD != clientPartyPtr->partySubType())
         {
            continue;
         }

         PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr->displayName());
         for (const auto recipient : recipients)
         {
            if (recipient->userName() == remoteUserName)
            {
               // party already existed
               emit privatePartyAlreadyExist(clientPartyPtr->id());
               return;
            }
         }
      }

      // party not exist, create new one
      ClientPartyPtr newClientPrivatePartyPtr =
         std::make_shared<ClientParty>(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(), PartyType::PRIVATE_DIRECT_MESSAGE, PartySubType::STANDARD);

      newClientPrivatePartyPtr->setDisplayName(remoteUserName);
      // setup recipients for new private party
      PartyRecipientsPtrList recipients;
      recipients.push_back(std::make_shared<PartyRecipient>(currentUserPtr->displayName()));
      recipients.push_back(std::make_shared<PartyRecipient>(remoteUserName));
      newClientPrivatePartyPtr->setRecipients(recipients);

      // update model
      clientPartyModelPtr_->insertParty(newClientPrivatePartyPtr);
      emit partyModelChanged();

      // save party in db
      clientDBServicePtr_->createNewParty(newClientPrivatePartyPtr->id());

      emit privatePartyCreated(newClientPrivatePartyPtr->id());
   }

   void ClientPartyLogic::createPrivatePartyFromPrivatePartyRequest(const ChatUserPtr& currentUserPtr, const google::protobuf::Message& msg)
   {
      PrivatePartyRequest privatePartyRequest;
      privatePartyRequest.CopyFrom(msg);

      PartyPacket partyPacket = privatePartyRequest.party_packet();

      ClientPartyPtr newClientPrivatePartyPtr =
         std::make_shared<ClientParty>(
            partyPacket.party_id(),
            partyPacket.party_type(),
            partyPacket.party_subtype()
         );

      PartyRecipientsPtrList recipients;
      for (int i = 0; i < partyPacket.recipient_size(); i++)
      {
         PartyRecipientPtr recipient = std::make_shared<PartyRecipient>(
            partyPacket.recipient(i).user_name(), partyPacket.recipient(i).public_key()
         );

         recipients.push_back(recipient);
      }

      // update model
      clientPartyModelPtr_->insertParty(newClientPrivatePartyPtr);
      emit partyModelChanged();

      // save party in db
      clientDBServicePtr_->createNewParty(newClientPrivatePartyPtr->id());

      // ! Do NOT emit here privatePartyCreated, it's connected with party request
   }

}
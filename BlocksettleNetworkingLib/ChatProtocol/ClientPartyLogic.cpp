/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QUuid>

#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientParty.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

#include "chat.pb.h"

using namespace Chat;

ClientPartyLogic::ClientPartyLogic(const LoggerPtr& loggerPtr, const ClientDBServicePtr& clientDBServicePtr, QObject* parent)
   : QObject(parent), loggerPtr_(loggerPtr)
{
   qRegisterMetaType<Chat::ClientPartyLogicError>();

   clientDBServicePtr_ = clientDBServicePtr;
   clientPartyModelPtr_ = std::make_shared<ClientPartyModel>(loggerPtr, this);
   connect(clientPartyModelPtr_.get(), &ClientPartyModel::clientPartyDisplayNameChanged, this, &ClientPartyLogic::clientPartyDisplayNameChanged);
   connect(clientPartyModelPtr_.get(), &ClientPartyModel::partyModelChanged, this, &ClientPartyLogic::partyModelChanged);

   connect(clientDBServicePtr.get(), &ClientDBService::partyDisplayNameLoaded, this, &ClientPartyLogic::partyDisplayNameLoaded);

   connect(this, &ClientPartyLogic::error, this, &ClientPartyLogic::handleLocalErrors);
   connect(clientDBServicePtr.get(), &ClientDBService::messageArrived, this, &ClientPartyLogic::handleMessageArrived);
   connect(this, &ClientPartyLogic::messageArrived, clientPartyModelPtr_.get(), &ClientPartyModel::messageArrived);
   connect(clientDBServicePtr.get(), &ClientDBService::messageStateChanged, clientPartyModelPtr_.get(), &ClientPartyModel::messageStateChanged);
   connect(clientDBServicePtr_.get(), &ClientDBService::recipientKeysHasChanged, this, &ClientPartyLogic::onRecipientKeysHasChanged);
   connect(clientDBServicePtr_.get(), &ClientDBService::recipientKeysUnchanged, this, &ClientPartyLogic::onRecipientKeysUnchanged);

   connect(clientPartyModelPtr_.get(), &ClientPartyModel::partyInserted, this, &ClientPartyLogic::handlePartyInserted);
   connect(this, &ClientPartyLogic::userPublicKeyChanged, clientPartyModelPtr_.get(), &ClientPartyModel::userPublicKeyChanged, Qt::QueuedConnection);
}

void ClientPartyLogic::handlePartiesFromWelcomePacket(const ChatUserPtr& currentUserPtr, const WelcomeResponse& welcomeResponse) const
{
   clientPartyModelPtr_->clearModel();
   clientDBServicePtr_->cleanUnusedParties();

   // all unique recipients
   UniqieRecipientMap uniqueRecipients;

   for (auto i = 0; i < welcomeResponse.party_size(); i++)
   {
      const auto& partyPacket = welcomeResponse.party(i);

      auto clientPartyPtr = std::make_shared<ClientParty>(
         partyPacket.party_id(), partyPacket.party_type(), partyPacket.party_subtype(), partyPacket.party_state());
      clientPartyPtr->setDisplayName(partyPacket.display_name());
      clientPartyPtr->setUserHash(partyPacket.display_name());
      clientPartyPtr->setPartyCreatorHash(partyPacket.party_creator_hash());

      if (PRIVATE_DIRECT_MESSAGE == partyPacket.party_type())
      {
         PartyRecipientsPtrList recipients;
         for (auto j = 0; j < partyPacket.recipient_size(); j++)
         {
            const auto& recipient = partyPacket.recipient(j);
            auto recipientPtr =
               std::make_shared<PartyRecipient>(recipient.user_hash(), recipient.public_key(), QDateTime::fromMSecsSinceEpoch(recipient.timestamp_ms()));
            recipients.push_back(recipientPtr);

            // choose all recipients except me
            if (currentUserPtr->userHash() != recipientPtr->userHash())
            {
               uniqueRecipients[recipientPtr->userHash()] = recipientPtr;
            }
         }

         clientPartyPtr->setRecipients(recipients);
      }

      clientPartyModelPtr_->insertParty(clientPartyPtr);

      // refresh party to user table
      clientDBServicePtr_->savePartyRecipients(clientPartyPtr);
   }

   // check if any of recipients has changed public key
   clientDBServicePtr_->checkRecipientPublicKey(uniqueRecipients);
}

void ClientPartyLogic::onUserStatusChanged(const ChatUserPtr&, const StatusChanged& statusChanged)
{
   // status changed only for private parties
   auto clientPartyPtrList = 
      clientPartyModelPtr_->getClientPartyListForRecipient(clientPartyModelPtr_->getIdPrivatePartyList(), statusChanged.user_hash());

   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      auto recipientPtr = clientPartyPtr->getRecipient(statusChanged.user_hash());
      if (recipientPtr)
      {
         recipientPtr->setCelerType(static_cast<CelerClient::CelerUserType>(statusChanged.celer_type()));
      }

      const auto oldClientStatus = clientPartyPtr->clientStatus();

      clientPartyPtr->setClientStatus(statusChanged.client_status());

      if (ONLINE != clientPartyPtr->clientStatus())
      {
         return;
      }

      // check if public key changed
      if (statusChanged.has_public_key())
      {
         if (recipientPtr)
         {
            const BinaryData public_key(statusChanged.public_key().value());
            const auto dt = QDateTime::fromMSecsSinceEpoch(statusChanged.timestamp_ms().value());
            const auto userPkPtr = std::make_shared<UserPublicKeyInfo>();

            userPkPtr->setUser_hash(QString::fromStdString(recipientPtr->userHash()));
            userPkPtr->setOldPublicKeyHex(recipientPtr->publicKey());
            userPkPtr->setOldPublicKeyTime(recipientPtr->publicKeyTime());
            userPkPtr->setNewPublicKeyHex(public_key);
            userPkPtr->setNewPublicKeyTime(dt);
            UserPublicKeyInfoList userPkList;
            userPkList.push_back(userPkPtr);

            emit userPublicKeyChanged(userPkList);
         }

         return;
      }

      // if client status is online and different than previous status
      // check if we have any unsent messages for this user
      if (oldClientStatus == statusChanged.client_status())
      {
         continue;
      }

      clientDBServicePtr_->checkUnsentMessages(clientPartyPtr->id());
   }
}

void ClientPartyLogic::handleLocalErrors(const ClientPartyLogicError& errorCode, const std::string& what) const
{
   loggerPtr_->debug("[ClientPartyLogic::handleLocalErrors] Error: {}, what: {}", int(errorCode), what);
}

void ClientPartyLogic::handlePartyInserted(const Chat::PartyPtr& partyPtr) const
{
   clientDBServicePtr_->createNewParty(partyPtr);
}

void ClientPartyLogic::createPrivateParty(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName, const Chat::PartySubType& partySubType, const std::string& initialMessage)
{
   // check if private party exist
   if (isPrivatePartyForUserExist(currentUserPtr, remoteUserName, partySubType))
   {
      if (OTC == partySubType)
      {
         // delete old otc private party and create new one
         const auto clientPartyPtr = clientPartyModelPtr_->getOtcPartyForUsers(currentUserPtr->userHash(), remoteUserName);
         emit deletePrivateParty(clientPartyPtr->id());
      }
      else
      {
         // standard private parties can be reused
         return;
      }
   }

   // party not exist, create new one
   auto newClientPrivatePartyPtr =
      std::make_shared<ClientParty>(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(), PRIVATE_DIRECT_MESSAGE, partySubType);

   newClientPrivatePartyPtr->setDisplayName(remoteUserName);
   newClientPrivatePartyPtr->setUserHash(remoteUserName);
   newClientPrivatePartyPtr->setPartyCreatorHash(currentUserPtr->userHash());
   // setup recipients for new private party
   PartyRecipientsPtrList recipients;
   recipients.push_back(std::make_shared<PartyRecipient>(currentUserPtr->userHash()));
   recipients.push_back(std::make_shared<PartyRecipient>(remoteUserName));
   newClientPrivatePartyPtr->setRecipients(recipients);
   newClientPrivatePartyPtr->setInitialMessage(initialMessage);

   // update model
   clientPartyModelPtr_->insertParty(newClientPrivatePartyPtr);
   emit partyModelChanged();

   // save party in db
   clientDBServicePtr_->createNewParty(newClientPrivatePartyPtr);

   emit privatePartyCreated(newClientPrivatePartyPtr->id());
}

bool ClientPartyLogic::isPrivatePartyForUserExist(const ChatUserPtr& currentUserPtr, const std::string& remoteUserName, const Chat::PartySubType& partySubType)
{
   auto idPartyList = clientPartyModelPtr_->getIdPrivatePartyListBySubType(partySubType);

   for (const auto& partyId : idPartyList)
   {
      auto clientPartyPtr = clientPartyModelPtr_->getClientPartyById(partyId);
      if (!clientPartyPtr)
      {
         continue;
      }

      auto recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr->userHash());
      for (const auto& recipient : recipients)
      {
         if (recipient->userHash() == remoteUserName)
         {
            // party already existed
            emit privatePartyAlreadyExist(clientPartyPtr->id());
            return true;
         }
      }
   }

   return false;
}

void ClientPartyLogic::createPrivatePartyFromPrivatePartyRequest(const ChatUserPtr& currentUserPtr, const PrivatePartyRequest& privatePartyRequest)
{
   const auto& partyPacket = privatePartyRequest.party_packet();

   auto newClientPrivatePartyPtr =
      std::make_shared<ClientParty>(
         partyPacket.party_id(),
         partyPacket.party_type(),
         partyPacket.party_subtype()
         );

   newClientPrivatePartyPtr->setPartyState(partyPacket.party_state());
   newClientPrivatePartyPtr->setPartyCreatorHash(partyPacket.party_creator_hash());
   newClientPrivatePartyPtr->setDisplayName(partyPacket.display_name());
   newClientPrivatePartyPtr->setUserHash(partyPacket.party_creator_hash());

   PartyRecipientsPtrList recipients;
   for (auto i = 0; i < partyPacket.recipient_size(); i++)
   {
      auto recipient = std::make_shared<PartyRecipient>(
         partyPacket.recipient(i).user_hash(), partyPacket.recipient(i).public_key()
         );

      recipients.push_back(recipient);
   }

   newClientPrivatePartyPtr->setRecipients(recipients);

   // check if already exist party in rejected state with this same user
   const auto clientPartyPtrList = clientPartyModelPtr_->getStandardPrivatePartyListForRecipient(newClientPrivatePartyPtr->partyCreatorHash());
   for (const auto& oldClientPartyPtr : clientPartyPtrList)
   {
      // exist one, checking party state
      if (REJECTED == oldClientPartyPtr->partyState())
      {
         emit deletePrivateParty(oldClientPartyPtr->id());

         // delete old recipients keys
         auto oldRecipients = oldClientPartyPtr->getRecipientsExceptMe(currentUserPtr->userHash());
         clientDBServicePtr_->deleteRecipientsKeys(oldRecipients);
      }
   }

   // update model
   clientPartyModelPtr_->insertParty(newClientPrivatePartyPtr);

   // update recipients keys
   const auto remoteRecipients = newClientPrivatePartyPtr->getRecipientsExceptMe(currentUserPtr->userHash());
   clientDBServicePtr_->saveRecipientsKeys(remoteRecipients);

   emit partyModelChanged();

   // save party in db
   clientDBServicePtr_->createNewParty(newClientPrivatePartyPtr);

   // auto accept private otc party
   if (newClientPrivatePartyPtr->isPrivateOTC())
   {
       emit acceptOTCPrivateParty(newClientPrivatePartyPtr->id());
   }

   // ! Do NOT emit here privatePartyCreated, it's connected with party request
}

void ClientPartyLogic::clientPartyDisplayNameChanged(const std::string& partyId) const
{
   const auto clientPartyPtr = clientPartyModelPtr_->getClientPartyById(partyId);

   if (!clientPartyPtr)
   {
      return;
   }

   clientDBServicePtr_->updateDisplayNameForParty(partyId, clientPartyPtr->displayName());
}

void ClientPartyLogic::partyDisplayNameLoaded(const std::string& partyId, const std::string& displayName)
{
   auto clientPartyPtr = clientPartyModelPtr_->getClientPartyById(partyId);

   if (clientPartyPtr == nullptr)
   {
      emit error(ClientPartyLogicError::PartyNotExist, partyId);
      return;
   }

   clientPartyPtr->setDisplayName(displayName);

   emit partyModelChanged();
}

// if logged out set offline for all private parties
void ClientPartyLogic::loggedOutFromServer() const
{
   auto idPartyList = clientPartyModelPtr_->getIdPrivatePartyList();
   for (const auto& partyId : idPartyList)
   {
      auto clientPartyPtr = clientPartyModelPtr_->getClientPartyById(partyId);

      if (!clientPartyPtr)
      {
         continue;
      }

      clientPartyPtr->setClientStatus(OFFLINE);
   }
}

void ClientPartyLogic::onRecipientKeysHasChanged(const Chat::UserPublicKeyInfoList& userPkList)
{
   emit userPublicKeyChanged(userPkList);
}

void ClientPartyLogic::onRecipientKeysUnchanged()
{
   updateModelAndRefreshPartyDisplayNames();
}

void ClientPartyLogic::updateModelAndRefreshPartyDisplayNames()
{
   emit partyModelChanged();

   // parties loaded, check is party display name should be updated
   auto idPartyList = clientPartyModelPtr_->getIdPartyList();
   for (const auto& partyId : idPartyList)
   {
      clientDBServicePtr_->loadPartyDisplayName(partyId);
   }

}

void ClientPartyLogic::handleMessageArrived(const Chat::MessagePtrList& messagePtrList)
{
   for (const auto& messagePtr : messagePtrList)
   {
      ClientPartyPtrList clientPartyPtrList = clientPartyModelPtr()->getClientPartyListFromIdPartyList(
         clientPartyModelPtr()->getIdPrivatePartyListBySubType());

      for (const auto& clientPartyPtr : clientPartyPtrList)
      {
         if (!clientPartyPtr->isUserBelongsToParty(messagePtr->senderHash()))
         {
            continue;
         }

         if (clientPartyPtr->userHash() == clientPartyPtr->displayName())
         {
            // display name isn't changed
            messagePtr->setDisplayName(messagePtr->senderHash());
            continue;
         }

         // custom party/message display name for user
         messagePtr->setDisplayName(clientPartyPtr->displayName());
      }
   }

   emit messageArrived(messagePtrList);
}

/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QThread>
#include <QUuid>
#include <QMetaType>
#include <utility>

#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ClientConnectionLogic.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientPartyModel.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include "chat.pb.h"
#include <enable_warnings.h>

using namespace Chat;

ClientConnectionLogic::ClientConnectionLogic(ClientPartyLogicPtr clientPartyLogicPtr,
                                             ClientDBServicePtr clientDBServicePtr, LoggerPtr loggerPtr,
                                             Chat::CryptManagerPtr cryptManagerPtr,
                                             SessionKeyHolderPtr sessionKeyHolderPtr, QObject* parent /* = nullptr */)
   : QObject(parent), loggerPtr_(std::move(loggerPtr)), clientPartyLogicPtr_(
        std::move(clientPartyLogicPtr)), 
   clientDBServicePtr_(std::move(clientDBServicePtr)), sessionKeyHolderPtr_(std::move(sessionKeyHolderPtr)), cryptManagerPtr_(
      std::move(cryptManagerPtr))
{
   qRegisterMetaType<Chat::SearchUserReplyList>();

   connect(this, &ClientConnectionLogic::userStatusChanged, clientPartyLogicPtr_.get(), &ClientPartyLogic::onUserStatusChanged);
   connect(this, &ClientConnectionLogic::error, this, &ClientConnectionLogic::handleLocalErrors);

   connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::requestSessionKeyExchange, this, &ClientConnectionLogic::requestSessionKeyExchange);
   connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::replySessionKeyExchange, this, &ClientConnectionLogic::replySessionKeyExchange);
   connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::sessionKeysForUser, this, &ClientConnectionLogic::sessionKeysForUser);
   connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::sessionKeysForUserFailed, this, &ClientConnectionLogic::sessionKeysForUserFailed);

   connect(clientDBServicePtr_.get(), &ClientDBService::messageLoaded, this, &ClientConnectionLogic::messageLoaded);
   connect(clientDBServicePtr_.get(), &ClientDBService::unsentMessagesFound, this, &ClientConnectionLogic::unsentMessagesFound);
}

void ClientConnectionLogic::onDataReceived(const std::string& data)
{
   google::protobuf::Any any;
   if (!any.ParseFromString(data))
   {
      emit error(ClientConnectionLogicError::ParsingPacketData, data);
      return;
   }

   loggerPtr_->debug("[ClientConnectionLogic::onDataReceived] Data: {}", ProtobufUtils::toJsonCompact(any));

   WelcomeResponse welcomeResponse;
   if (ProtobufUtils::pbAnyToMessage<WelcomeResponse>(any, &welcomeResponse))
   {
      handleWelcomeResponse(welcomeResponse);
      return;
   }

   LogoutResponse logoutResponse;
   if (ProtobufUtils::pbAnyToMessage<LogoutResponse>(any, &logoutResponse))
   {
      handleLogoutResponse(logoutResponse);
      return;
   }

   StatusChanged statusChanged;
   if (ProtobufUtils::pbAnyToMessage<StatusChanged>(any, &statusChanged))
   {
      handleStatusChanged(statusChanged);
      return;
   }

   PartyMessageStateUpdate partyMessageStateUpdate;
   if (ProtobufUtils::pbAnyToMessage<PartyMessageStateUpdate>(any, &partyMessageStateUpdate))
   {
      handlePartyMessageStateUpdate(partyMessageStateUpdate);
      return;
   }

   PartyMessagePacket partyMessagePacket;
   if (ProtobufUtils::pbAnyToMessage<PartyMessagePacket>(any, &partyMessagePacket))
   {
      handlePartyMessagePacket(partyMessagePacket);
      return;
   }

   PrivatePartyRequest privatePartyRequest;
   if (ProtobufUtils::pbAnyToMessage<PrivatePartyRequest>(any, &privatePartyRequest))
   {
      handlePrivatePartyRequest(privatePartyRequest);
      return;
   }

   RequestSessionKeyExchange requestSessionKey;
   if (ProtobufUtils::pbAnyToMessage<RequestSessionKeyExchange>(any, &requestSessionKey))
   {
      handleRequestSessionKeyExchange(requestSessionKey);
      return;
   }

   ReplySessionKeyExchange replyKeyExchange;
   if (ProtobufUtils::pbAnyToMessage<ReplySessionKeyExchange>(any, &replyKeyExchange))
   {
      handleReplySessionKeyExchange(replyKeyExchange);
      return;
   }

   PrivatePartyStateChanged privatePartyStateChanged;
   if (ProtobufUtils::pbAnyToMessage<PrivatePartyStateChanged>(any, &privatePartyStateChanged))
   {
      handlePrivatePartyStateChanged(privatePartyStateChanged);
      return;
   }

   ReplySearchUser replySearchUser;
   if (ProtobufUtils::pbAnyToMessage<ReplySearchUser>(any, &replySearchUser))
   {
      handleReplySearchUser(replySearchUser);
      return;
   }

   PartyMessageOfflineRequest partyMessageOfflineRequest;
   if (ProtobufUtils::pbAnyToMessage<PartyMessageOfflineRequest>(any, &partyMessageOfflineRequest))
   {
      handlePartyMessageOfflineRequest(partyMessageOfflineRequest);
      return;
   }

   auto what = QString::fromLatin1("data: %1").arg(QString::fromStdString(data));
   emit error(ClientConnectionLogicError::UnhandledPacket, what.toStdString());
}

void ClientConnectionLogic::onConnected()
{
   WelcomeRequest welcomeRequest;
   welcomeRequest.set_user_hash(currentUserPtr()->userHash());
   welcomeRequest.set_client_public_key(currentUserPtr()->publicKey().toBinStr());
   welcomeRequest.set_celer_type(static_cast<int>(currentUserPtr()->celerUserType()));
   welcomeRequest.set_chat_token_data(token_.toBinStr());
   welcomeRequest.set_chat_token_sign(tokenSign_.toBinStr());

   emit sendPacket(welcomeRequest);
}

void ClientConnectionLogic::onDisconnected()
{

}

void ClientConnectionLogic::onError(DataConnectionListener::DataConnectionError)
{

}

void ClientConnectionLogic::handleWelcomeResponse(const WelcomeResponse& welcomeResponse)
{
   if (!welcomeResponse.success())
   {
      emit closeConnection();
      return;
   }

   clientPartyLogicPtr_->handlePartiesFromWelcomePacket(currentUserPtr(), welcomeResponse);

   emit properlyConnected();

   // update party to user table and check history messages
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtrList = clientPartyModelPtr->getStandardPrivatePartyListForRecipient(currentUserPtr()->userHash());
   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      // Read and provide last 10 history messages only for standard private parties
      clientDBServicePtr_->readHistoryMessages(clientPartyPtr->id(), clientPartyPtr->userHash(), 10);
   }

   // request offline messages for me
   const PartyMessageOfflineRequest partyMessageOfflineRequest;
   emit sendPacket(partyMessageOfflineRequest);
}

void ClientConnectionLogic::handleLogoutResponse(const LogoutResponse&)
{
   emit closeConnection();
}

void ClientConnectionLogic::handleStatusChanged(const StatusChanged& statusChanged)
{
   // clear session keys for user
   sessionKeyHolderPtr_->clearSessionForUser(statusChanged.user_hash());

   emit userStatusChanged(currentUserPtr(), statusChanged);
}

void ClientConnectionLogic::handlePartyMessageStateUpdate(const PartyMessageStateUpdate& partyMessageStateUpdate) const
{
   clientDBServicePtr_->updateMessageState(partyMessageStateUpdate.message_id(), partyMessageStateUpdate.party_message_state());
}

void ClientConnectionLogic::prepareAndSendMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
{
   if (clientPartyPtr->isGlobal()) {
      prepareAndSendPublicMessage(clientPartyPtr, data);
      return;
   }

   if (clientPartyPtr->isPrivate()) {
      prepareAndSendPrivateMessage(clientPartyPtr, data);
      return;
   }

   emit error(ClientConnectionLogicError::SendingDataToUnhandledParty, clientPartyPtr->id());
}

void ClientConnectionLogic::prepareAndSendPublicMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
{
   const auto& partyId = clientPartyPtr->id();
   const auto& messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
   const auto& timestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
   const auto& message = data;
   const auto& encryptionType = UNENCRYPTED;
   const auto& partyMessageState = UNSENT;

   PartyMessagePacket partyMessagePacket;
   partyMessagePacket.set_party_id(partyId);
   partyMessagePacket.set_message_id(messageId);
   partyMessagePacket.set_timestamp_ms(timestamp);
   partyMessagePacket.set_message(message);
   partyMessagePacket.set_encryption(encryptionType);
   partyMessagePacket.set_nonce("");
   partyMessagePacket.set_party_message_state(partyMessageState);
   partyMessagePacket.set_sender_hash(currentUserPtr()->userHash());

   clientDBServicePtr_->saveMessage(clientPartyPtr, ProtobufUtils::pbMessageToString(partyMessagePacket));

   emit sendPacket(partyMessagePacket);
}

void ClientConnectionLogic::handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what, bool displayAsWarning) const
{
   const auto displayAs = displayAsWarning ? ErrorType::WarningDescription : ErrorType::ErrorDescription;

   loggerPtr_->debug("[ClientConnectionLogic::handleLocalErrors] {}: {}, what: {}", displayAs, static_cast<int>(errorCode), what);
}

void ClientConnectionLogic::handlePartyMessagePacket(PartyMessagePacket& partyMessagePacket)
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   const auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());
   if (!clientPartyPtr) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "can't find party with id: {}", partyMessagePacket.party_id());
      return;
   }

   // TODO: think about better way to display messages with correct timestamp
   // if remote pc have different time then we have timestamp mess in view
   // for now we're updating timestamp to local one
   partyMessagePacket.set_timestamp_ms(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

   // TODO: handle here state changes of the rest of message types
   if (clientPartyPtr->isPrivate())
   {
      incomingPrivatePartyMessage(partyMessagePacket);
      return;
   }

   // Allow processing OTC messages
   if (clientPartyPtr->isGlobal())
   {
      incomingGlobalPartyMessage(partyMessagePacket);
   }
}

void ClientConnectionLogic::incomingGlobalPartyMessage(PartyMessagePacket& partyMessagePacket)
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

   saveIncomingPartyMessageAndUpdateState(partyMessagePacket, RECEIVED);
}

void ClientConnectionLogic::incomingPrivatePartyMessage(PartyMessagePacket& partyMessagePacket)
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

   const auto recipientPtr = clientPartyPtr->getSecondRecipient(currentUserPtr()->userHash());

   if (nullptr == recipientPtr)
   {
      emit error(ClientConnectionLogicError::WrongPartyRecipient, partyMessagePacket.party_id());
      return;
   }

   if (partyMessagePacket.encryption() == AEAD)
   {
      const auto sessionKeyDataPtr = sessionKeyHolderPtr_->sessionKeyDataForUser(recipientPtr->userHash());

      const BinaryData nonce = partyMessagePacket.nonce();
      const auto associatedData = cryptManagerPtr_->jsonAssociatedData(clientPartyPtr->id(), nonce);

      const auto future = cryptManagerPtr_->decryptMessageAEAD(partyMessagePacket.message(), associatedData,
         sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());
      const auto decryptedMessage = future.result();

      partyMessagePacket.set_message(decryptedMessage);

      saveIncomingPartyMessageAndUpdateState(partyMessagePacket, RECEIVED);
   }

   if (partyMessagePacket.encryption() == IES)
   {
      const auto future = cryptManagerPtr_->decryptMessageIES(partyMessagePacket.message(), currentUserPtr()->privateKey());
      const auto decryptedMessage = future.result();

      partyMessagePacket.set_message(decryptedMessage);

      saveIncomingPartyMessageAndUpdateState(partyMessagePacket, RECEIVED);
   }
}

void ClientConnectionLogic::saveIncomingPartyMessageAndUpdateState(PartyMessagePacket& partyMessagePacket, const PartyMessageState& partyMessageState)
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   const auto partyPtr = clientPartyModelPtr->getPartyById(partyMessagePacket.party_id());

   if (nullptr == partyPtr)
   {
      emit error(ClientConnectionLogicError::CouldNotFindParty, partyMessagePacket.party_id(), true);
      return;
   }

   //save message
   clientDBServicePtr_->saveMessage(partyPtr, ProtobufUtils::pbMessageToString(partyMessagePacket));

   if (partyPtr->isGlobalStandard())
   {
      // for global we only updating state in local db
      clientDBServicePtr_->updateMessageState(partyMessagePacket.message_id(), partyMessageState);
      return;
   }

   if (partyPtr->isPrivate())
   {
      // for private we need to reply message state as RECEIVED
      // and then save in local db
      const auto privateDMPartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);
      if (nullptr == privateDMPartyPtr)
      {
         emit error(ClientConnectionLogicError::DynamicPointerCast, partyMessagePacket.party_id());
         return;
      }

      // set message state as RECEIVED
      partyMessagePacket.set_party_message_state(partyMessageState);

      // reply new message state
      PartyMessageStateUpdate partyMessageStateUpdate;
      partyMessageStateUpdate.set_party_id(partyMessagePacket.party_id());
      partyMessageStateUpdate.set_message_id(partyMessagePacket.message_id());
      partyMessageStateUpdate.set_party_message_state(partyMessagePacket.party_message_state());

      emit sendPacket(partyMessageStateUpdate);

      // update message state in db
      clientDBServicePtr_->updateMessageState(partyMessagePacket.message_id(), partyMessagePacket.party_message_state());
   }
}

void ClientConnectionLogic::setMessageSeen(const ClientPartyPtr& clientPartyPtr, const std::string& messageId)
{
   if (!clientPartyPtr->isPrivate())
   {
      return;
   }

   const auto partyMessageState = SEEN;

   // private chat, reply that message was received
   PartyMessageStateUpdate partyMessageStateUpdate;
   partyMessageStateUpdate.set_party_id(clientPartyPtr->id());
   partyMessageStateUpdate.set_message_id(messageId);
   partyMessageStateUpdate.set_party_message_state(partyMessageState);

   emit sendPacket(partyMessageStateUpdate);

   clientDBServicePtr_->updateMessageState(messageId, partyMessageState);
}

void ClientConnectionLogic::messagePacketSent(const std::string& messageId) const
{
   const auto partyMessageState = SENT;
   clientDBServicePtr_->updateMessageState(messageId, partyMessageState);
}

void ClientConnectionLogic::prepareRequestPrivateParty(const std::string& partyId)
{
   const PartyPtr partyPtr = clientPartyLogicPtr_->clientPartyModelPtr()->getClientPartyById(partyId);

   if (nullptr == partyPtr)
   {
      return;
   }

   auto clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   // update party state
   clientPartyPtr->setPartyState(REQUESTED);

   const auto secondRecipientPtr = clientPartyPtr->getSecondRecipient(currentUserPtr()->userHash());
   // wrong recipient, delete party and show error
   if (nullptr == secondRecipientPtr)
   {
      clientPartyLogicPtr_->clientPartyModelPtr()->removeParty(clientPartyPtr);
      emit error(ClientConnectionLogicError::WrongPartyRecipient, partyId);
      return;
   }

   PrivatePartyRequest privatePartyRequest;
   auto partyPacket = privatePartyRequest.mutable_party_packet();
   partyPacket->set_party_id(partyId);
   partyPacket->set_display_name(secondRecipientPtr->userHash());
   partyPacket->set_party_type(clientPartyPtr->partyType());
   partyPacket->set_party_subtype(clientPartyPtr->partySubType());
   partyPacket->set_party_state(clientPartyPtr->partyState());
   partyPacket->set_party_creator_hash(currentUserPtr()->userHash());

   privatePartyRequest.set_initial_message(clientPartyPtr->initialMessage());

   for (const auto& recipient : clientPartyPtr->recipients())
   {
      auto partyRecipientPacket = partyPacket->add_recipient();
      partyRecipientPacket->set_user_hash(recipient->userHash());
      partyRecipientPacket->set_public_key(recipient->publicKey().toBinStr());
      partyRecipientPacket->set_timestamp_ms(recipient->publicKeyTime().toMSecsSinceEpoch());
   }

   emit sendPacket(privatePartyRequest);

   if (!clientPartyPtr->initialMessage().empty())
   {
      // send initial message
      prepareAndSendPrivateMessage(clientPartyPtr, clientPartyPtr->initialMessage());
      // clean
      clientPartyPtr->setInitialMessage("");
   }
}

void ClientConnectionLogic::handlePrivatePartyRequest(const PrivatePartyRequest& privatePartyRequest)
{
   // 1. check if model have this same party id
   // 2. if have and local party state is initialized then reply initialized state
   // 3. if not create new private party
   // 4. save updated recipients keys in db
   // 5. save party id in db

   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   const PartyPtr partyPtr = clientPartyModelPtr->getClientPartyById(privatePartyRequest.party_packet().party_id());

   // local party exist
   if (partyPtr)
   {
      // if party request was created by me
      // update recipient data
      const PartyPacket& partyPacket = privatePartyRequest.party_packet();
      if (currentUserPtr()->userHash() == partyPacket.party_creator_hash())
      {
         auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyPacket.party_id());

         if (nullptr == clientPartyPtr)
         {
            return;
         }

         PartyRecipientsPtrList updatedRecipients;
         for (auto i = 0; i < partyPacket.recipient_size(); i++)
         {
            const auto& recipient = partyPacket.recipient(i);
            auto newRecipient = std::make_shared<PartyRecipient>(recipient.user_hash(), recipient.public_key(), QDateTime::fromMSecsSinceEpoch(recipient.timestamp_ms()));
            updatedRecipients.push_back(newRecipient);
         }

         clientPartyPtr->setRecipients(updatedRecipients);

         // Save recipient keys in db
         saveRecipientsKeys(clientPartyPtr);

         return;
      }

      // party is in initialized or rejected state (already accepted)
      // send this state to requester

      if (INITIALIZED == partyPtr->partyState())
      {
         acceptPrivateParty(partyPtr->id());
         return;
      }

      if (REJECTED == partyPtr->partyState())
      {
         rejectPrivateParty(partyPtr->id());
         return;
      }

      return;
   }

   // local party not exist, create new one
   clientPartyLogicPtr_->createPrivatePartyFromPrivatePartyRequest(currentUserPtr(), privatePartyRequest);
}

void ClientConnectionLogic::requestSessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey)
{
   RequestSessionKeyExchange requestSessionKey;
   requestSessionKey.set_sender_user_hash(currentUserPtr()->userHash());
   requestSessionKey.set_encoded_public_key(encodedLocalSessionPublicKey.toBinStr());
   requestSessionKey.set_receiver_user_hash(receieverUserName);

   sendPacket(requestSessionKey);
}

void ClientConnectionLogic::replySessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey)
{
   ReplySessionKeyExchange replyKeyExchange;
   replyKeyExchange.set_sender_user_hash(currentUserPtr()->userHash());
   replyKeyExchange.set_encoded_public_key(encodedLocalSessionPublicKey.toBinStr());
   replyKeyExchange.set_receiver_user_hash(receieverUserName);

   sendPacket(replyKeyExchange);
}

void ClientConnectionLogic::handleRequestSessionKeyExchange(const RequestSessionKeyExchange& requestKeyExchange) const
{
   sessionKeyHolderPtr_->onIncomingRequestSessionKeyExchange(requestKeyExchange.sender_user_hash(), requestKeyExchange.encoded_public_key(), currentUserPtr()->privateKey());
}

void ClientConnectionLogic::handleReplySessionKeyExchange(const ReplySessionKeyExchange& replyKeyExchange) const
{
   sessionKeyHolderPtr_->onIncomingReplySessionKeyExchange(replyKeyExchange.sender_user_hash(), replyKeyExchange.encoded_public_key());
}

void ClientConnectionLogic::prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data) const
{
   // prepare
   const auto partyId = clientPartyPtr->id();
   const auto messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
   const auto timestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
   const auto& message = data;
   const auto encryptionType = UNENCRYPTED;
   const auto partyMessageState = UNSENT;

   PartyMessagePacket partyMessagePacket;
   partyMessagePacket.set_party_id(partyId);
   partyMessagePacket.set_message_id(messageId);
   partyMessagePacket.set_timestamp_ms(timestamp);
   partyMessagePacket.set_message(message);
   partyMessagePacket.set_encryption(encryptionType);
   partyMessagePacket.set_nonce("");
   partyMessagePacket.set_party_message_state(partyMessageState);
   partyMessagePacket.set_sender_hash(currentUserPtr()->userHash());

   // save in db
   clientDBServicePtr_->saveMessage(clientPartyPtr, ProtobufUtils::pbMessageToString(partyMessagePacket));

   // call session key handler
   auto recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userHash());
   for (const auto& recipient : recipients)
   {
      sessionKeyHolderPtr_->requestSessionKeysForUser(recipient->userHash(), recipient->publicKey());
   }
}

void ClientConnectionLogic::sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr) const
{
   // read msg from db
   const auto receiverUserHash = sessionKeyDataPtr->userHash();
   const auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();

   const auto idPartyList = clientPartyModelPtr->getIdPrivatePartyList();
   const auto cppList = clientPartyModelPtr->getClientPartyListFromIdPartyList(idPartyList);
   const auto clientPartyPtrList = clientPartyModelPtr->getClientPartyForRecipients(cppList, currentUserPtr()->userHash(), receiverUserHash);

   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      clientDBServicePtr_->readUnsentMessages(clientPartyPtr->id());
   }
}

void ClientConnectionLogic::sessionKeysForUserFailed(const std::string&)
{
   // ! not implemented
   // this function is called after send message
}

void ClientConnectionLogic::messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
   const std::string& message, const int, const std::string&, const int partyMessageState)
{
   // 1. encrypt by aead
   // 2. send msg
   // 3. update message state in db

   // we need only unsent messages
   if (UNSENT != partyMessageState)
   {
      return;
   }

   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);

   auto recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userHash());
   for (const auto& recipient : recipients)
   {
      if (OFFLINE == clientPartyPtr->clientStatus())
      {
         // use IES encryption for offline clients
         QFuture<std::string> future = cryptManagerPtr_->encryptMessageIES(message, recipient->publicKey());
         auto encryptedMessage = future.result();

         PartyMessagePacket partyMessagePacket;
         partyMessagePacket.set_party_id(clientPartyPtr->id());
         partyMessagePacket.set_message_id(messageId);
         partyMessagePacket.set_timestamp_ms(timestamp);
         partyMessagePacket.set_encryption(IES);
         partyMessagePacket.set_message(encryptedMessage);
         partyMessagePacket.set_party_message_state(SENT);

         sendPacket(partyMessagePacket);
      }
      else
      {
         // we need to be sure here that sessionKeyDataPtr is properly initialized
         const auto sessionKeyDataPtr = sessionKeyHolderPtr_->sessionKeyDataForUser(recipient->userHash());
         if (!sessionKeyDataPtr->isInitialized())
         {
            /*
             * 13.11.2019
             * This could happen only in rare occasions. 
             * We need to break here and no error message is needed. 
             * Process of sending unsent messages will be repeated when sender will send next new message 
             * or sender will change his status to online again
             * or server can request unsent messages if detect that we're sending aead message to offline user 
             * or when we're trying temporary key exchange with offline user.
            */
            continue;
         }
         
         // use AEAD encryption for online clients
         auto nonce = sessionKeyDataPtr->nonce();
         auto associatedData = cryptManagerPtr_->jsonAssociatedData(partyId, nonce);

         auto future = cryptManagerPtr_->encryptMessageAEAD(
            message, associatedData, sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());
         auto encryptedMessage = future.result();

         PartyMessagePacket partyMessagePacket;
         partyMessagePacket.set_party_id(clientPartyPtr->id());
         partyMessagePacket.set_message_id(messageId);
         partyMessagePacket.set_timestamp_ms(timestamp);
         partyMessagePacket.set_encryption(AEAD);
         partyMessagePacket.set_message(encryptedMessage);
         partyMessagePacket.set_nonce(nonce.toBinStr());
         partyMessagePacket.set_party_message_state(SENT);

         sendPacket(partyMessagePacket);
      }
   }
}

void ClientConnectionLogic::unsentMessagesFound(const std::string& partyId) const
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);

   if (!clientPartyPtr)
   {
      return;
   }

   PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userHash());
   for (const auto& recipient : recipients)
   {
      sessionKeyHolderPtr_->requestSessionKeysForUser(recipient->userHash(), recipient->publicKey());
   }
}

void ClientConnectionLogic::rejectPrivateParty(const std::string& partyId)
{
   RequestPrivatePartyStateChange requestPrivatePartyStateChange;

   requestPrivatePartyStateChange.set_party_id(partyId);
   requestPrivatePartyStateChange.set_party_state(REJECTED);

   sendPacket(requestPrivatePartyStateChange);
}

void ClientConnectionLogic::acceptPrivateParty(const std::string& partyId)
{
   RequestPrivatePartyStateChange requestPrivatePartyStateChange;

   requestPrivatePartyStateChange.set_party_id(partyId);
   requestPrivatePartyStateChange.set_party_state(INITIALIZED);

   sendPacket(requestPrivatePartyStateChange);
}

void ClientConnectionLogic::handlePrivatePartyStateChanged(const PrivatePartyStateChanged& privatePartyStateChanged)
{
   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtr = clientPartyModelPtr->getClientPartyById(privatePartyStateChanged.party_id());

   if (nullptr == clientPartyPtr)
   {
      emit error(ClientConnectionLogicError::CouldNotFindParty, privatePartyStateChanged.party_id(), true);
      return;
   }

   clientPartyPtr->setPartyState(privatePartyStateChanged.party_state());

   if (INITIALIZED == privatePartyStateChanged.party_state())
   {
      // if it's otc party, notify that is ready
      if (clientPartyPtr->isPrivateOTC())
      {
         emit clientPartyModelPtr->otcPrivatePartyReady(clientPartyPtr);
      }

      // for private standard parties read history messages
      if (clientPartyPtr->isPrivateStandard())
      {
         clientDBServicePtr_->readHistoryMessages(clientPartyPtr->id(), clientPartyPtr->userHash(), 10);
      }
   }

   // if it's otc party with rejected state, then delete party
   if (REJECTED == clientPartyPtr->partyState() && clientPartyPtr->isPrivateOTC())
   {
      emit deletePrivateParty(clientPartyPtr->id());
   }
}

void ClientConnectionLogic::handleReplySearchUser(const ReplySearchUser& replySearchUser)
{
   SearchUserReplyList searchUserReplyList;

   for (const auto& searchUser : replySearchUser.user_hash())
   {
      searchUserReplyList.push_back(searchUser);
   }

   emit searchUserReply(searchUserReplyList, replySearchUser.search_id());
}

void ClientConnectionLogic::handlePartyMessageOfflineRequest(
   const PartyMessageOfflineRequest& partyMessageOfflineRequest) const
{
   const auto& receiverHash = partyMessageOfflineRequest.receiver_hash();

   auto clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   auto clientPartyPtrList = clientPartyModelPtr->getStandardPrivatePartyListForRecipient(receiverHash);

   for (const auto& clientPartyPtr : clientPartyPtrList)
   {
      clientDBServicePtr_->readUnsentMessages(clientPartyPtr->id());
   }
}

void ClientConnectionLogic::searchUser(const std::string& userHash, const std::string& searchId)
{
   RequestSearchUser requestSearchUser;
   requestSearchUser.set_search_id(searchId);
   requestSearchUser.set_search_text(userHash);

   emit sendPacket(requestSearchUser);
}

void ClientConnectionLogic::setToken(const BinaryData &token, const BinaryData &tokenSign)
{
   token_ = token;
   tokenSign_ = tokenSign;
}

void ClientConnectionLogic::saveRecipientsKeys(const ClientPartyPtr& clientPartyPtr) const
{
   const auto recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userHash());

   clientDBServicePtr_->saveRecipientsKeys(recipients);
}

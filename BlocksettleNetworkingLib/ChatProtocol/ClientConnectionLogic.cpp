#include <QThread>
#include <QUuid>
#include <QDateTime>
#include <QMetaType>

#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ClientConnectionLogic.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientPartyModel.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

using namespace Chat;

ClientConnectionLogic::ClientConnectionLogic(const ClientPartyLogicPtr& clientPartyLogicPtr, const ApplicationSettingsPtr& appSettings,
   const ClientDBServicePtr& clientDBServicePtr, const LoggerPtr& loggerPtr, const Chat::CryptManagerPtr& cryptManagerPtr, QObject* parent /* = nullptr */)
   : QObject(parent), cryptManagerPtr_(cryptManagerPtr), loggerPtr_(loggerPtr), clientDBServicePtr_(clientDBServicePtr), appSettings_(appSettings), clientPartyLogicPtr_(clientPartyLogicPtr)
{
   qRegisterMetaType<Chat::SearchUserReplyList>();

   connect(this, &ClientConnectionLogic::userStatusChanged, clientPartyLogicPtr_.get(), &ClientPartyLogic::onUserStatusChanged);
   connect(this, &ClientConnectionLogic::error, this, &ClientConnectionLogic::handleLocalErrors);

   sessionKeyHolderPtr_ = std::make_shared<SessionKeyHolder>(loggerPtr_, this);
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

   loggerPtr_->debug("[ClientConnectionLogic::onDataReceived] Data: {}", ProtobufUtils::toJsonReadable(any));

   WelcomeResponse welcomeResponse;
   if (ProtobufUtils::pbAnyToMessage<WelcomeResponse>(any, &welcomeResponse))
   {
      handleWelcomeResponse(welcomeResponse);
      emit properlyConnected();
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

   QString what = QString::fromLatin1("data: %1").arg(QString::fromStdString(data));
   emit error(ClientConnectionLogicError::UnhandledPacket, what.toStdString());
}

void ClientConnectionLogic::onConnected(void)
{
   Chat::WelcomeRequest welcomeRequest;
   welcomeRequest.set_user_name(currentUserPtr()->userName());
   welcomeRequest.set_client_public_key(currentUserPtr()->publicKey().toBinStr());

   emit sendPacket(welcomeRequest);
}

void ClientConnectionLogic::onDisconnected(void)
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

   clientPartyLogicPtr_->handlePartiesFromWelcomePacket(welcomeResponse);
}

void ClientConnectionLogic::handleLogoutResponse(const LogoutResponse&)
{
   emit closeConnection();
}

void ClientConnectionLogic::handleStatusChanged(const StatusChanged& statusChanged)
{
   // clear session keys for user
   sessionKeyHolderPtr_->clearSessionForUser(statusChanged.user_name());

   emit userStatusChanged(statusChanged.user_name(), statusChanged.client_status());
}

void ClientConnectionLogic::handlePartyMessageStateUpdate(const PartyMessageStateUpdate& partyMessageStateUpdate)
{
   clientDBServicePtr_->updateMessageState(partyMessageStateUpdate.message_id(), partyMessageStateUpdate.party_message_state());
}

void ClientConnectionLogic::prepareAndSendMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
{
   if (clientPartyPtr->isGlobalStandard())
   {
      prepareAndSendPublicMessage(clientPartyPtr, data);
      return;
   }

   if (clientPartyPtr->isPrivateStandard())
   {
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
   const auto& encryptionType = Chat::EncryptionType::UNENCRYPTED;
   const auto& partyMessageState = Chat::PartyMessageState::UNSENT;

   PartyMessagePacket partyMessagePacket;
   partyMessagePacket.set_party_id(partyId);
   partyMessagePacket.set_message_id(messageId);
   partyMessagePacket.set_timestamp_ms(timestamp);
   partyMessagePacket.set_message(message);
   partyMessagePacket.set_encryption(encryptionType);
   partyMessagePacket.set_nonce("");
   partyMessagePacket.set_party_message_state(partyMessageState);
   partyMessagePacket.set_sender_hash(currentUserPtr()->userName());

   clientDBServicePtr_->saveMessage(ProtobufUtils::pbMessageToString(partyMessagePacket));

   emit sendPacket(partyMessagePacket);
}

void ClientConnectionLogic::handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what)
{
   loggerPtr_->debug("[ClientConnectionLogic::handleLocalErrors] Error: {}, what: {}", static_cast<int>(errorCode), what);
}

void ClientConnectionLogic::handlePartyMessagePacket(PartyMessagePacket& partyMessagePacket)
{
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());
   if (!clientPartyPtr) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "can't find party with id: {}", partyMessagePacket.party_id());
      return;
   }

   // TODO: handle here state changes of the rest of message types
   if (clientPartyPtr->isPrivateStandard())
   {
      incomingPrivatePartyMessage(partyMessagePacket);
      return;
   }

   if (clientPartyPtr->isGlobalStandard())
   {
      incomingGlobalPartyMessage(partyMessagePacket);
      return;
   }
}

void ClientConnectionLogic::incomingGlobalPartyMessage(PartyMessagePacket& partyMessagePacket)
{
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

   saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
}

void ClientConnectionLogic::incomingPrivatePartyMessage(PartyMessagePacket& partyMessagePacket)
{
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

   PartyRecipientPtr recipientPtr = clientPartyPtr->getSecondRecipient(currentUserPtr()->userName());

   if (nullptr == recipientPtr)
   {
      emit error(ClientConnectionLogicError::WrongPartyRecipient, partyMessagePacket.party_id());
      return;
   }

   if (partyMessagePacket.encryption() == EncryptionType::AEAD)
   {
      SessionKeyDataPtr sessionKeyDataPtr = sessionKeyHolderPtr_->sessionKeyDataForUser(recipientPtr->userName());

      BinaryData nonce = partyMessagePacket.nonce();
      std::string associatedData = cryptManagerPtr_->jsonAssociatedData(clientPartyPtr->id(), nonce);

      QFuture<std::string> future = cryptManagerPtr_->decryptMessageAEAD(partyMessagePacket.message(), associatedData,
         sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());
      std::string decryptedMessage = future.result();

      partyMessagePacket.set_message(decryptedMessage);

      saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
   }

   if (partyMessagePacket.encryption() == EncryptionType::IES)
   {
      QFuture<std::string> future = cryptManagerPtr_->decryptMessageIES(partyMessagePacket.message(), currentUserPtr()->privateKey());
      std::string decryptedMessage = future.result();

      partyMessagePacket.set_message(decryptedMessage);

      saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
   }
}

void ClientConnectionLogic::saveIncomingPartyMessageAndUpdateState(PartyMessagePacket& partyMessagePacket, const PartyMessageState& partyMessageState)
{
   //save message
   clientDBServicePtr_->saveMessage(ProtobufUtils::pbMessageToString(partyMessagePacket));

   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   PartyPtr partyPtr = clientPartyModelPtr->getPartyById(partyMessagePacket.party_id());

   if (nullptr == partyPtr)
   {
      emit error(ClientConnectionLogicError::CouldNotFindParty, partyMessagePacket.party_id());
      return;
   }

   if (partyPtr->isGlobalStandard())
   {
      // for global we only updating state in local db
      clientDBServicePtr_->updateMessageState(partyMessagePacket.message_id(), partyMessageState);
      return;
   }

   if (partyPtr->isPrivateStandard())
   {
      // for private we need to reply message state as RECEIVED
      // and then save in local db
      PrivateDirectMessagePartyPtr privateDMPartyPtr = std::dynamic_pointer_cast<PrivateDirectMessageParty>(partyPtr);
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
   if (!(clientPartyPtr->isPrivateStandard()))
   {
      return;
   }

   auto partyMessageState = Chat::PartyMessageState::SEEN;

   // private chat, reply that message was received
   PartyMessageStateUpdate partyMessageStateUpdate;
   partyMessageStateUpdate.set_party_id(clientPartyPtr->id());
   partyMessageStateUpdate.set_message_id(messageId);
   partyMessageStateUpdate.set_party_message_state(partyMessageState);

   emit sendPacket(partyMessageStateUpdate);

   clientDBServicePtr_->updateMessageState(messageId, partyMessageState);
}

void ClientConnectionLogic::messagePacketSent(const std::string& messageId)
{
   auto partyMessageState = Chat::PartyMessageState::SENT;
   clientDBServicePtr_->updateMessageState(messageId, partyMessageState);
}

void ClientConnectionLogic::prepareRequestPrivateParty(const std::string& partyId)
{
   PartyPtr partyPtr = clientPartyLogicPtr_->clientPartyModelPtr()->getClientPartyById(partyId);

   if (nullptr == partyPtr)
   {
      return;
   }

   ClientPartyPtr clientPartyPtr = std::dynamic_pointer_cast<ClientParty>(partyPtr);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   // update party state
   clientPartyPtr->setPartyState(PartyState::REQUESTED);

   PartyRecipientPtr secondRecipientPtr = clientPartyPtr->getSecondRecipient(currentUserPtr()->userName());
   // wrong recipient, delete party and show error
   if (nullptr == secondRecipientPtr)
   {
      clientPartyLogicPtr_->clientPartyModelPtr()->removeParty(clientPartyPtr);
      emit error(ClientConnectionLogicError::WrongPartyRecipient, partyId);
      return;
   }

   PrivatePartyRequest privatePartyRequest;
   PartyPacket* partyPacket = privatePartyRequest.mutable_party_packet();
   partyPacket->set_party_id(partyId);
   partyPacket->set_display_name(secondRecipientPtr->userName());
   partyPacket->set_party_type(clientPartyPtr->partyType());
   partyPacket->set_party_subtype(clientPartyPtr->partySubType());
   partyPacket->set_party_state(clientPartyPtr->partyState());
   partyPacket->set_party_creator_hash(currentUserPtr()->userName());

   for (const PartyRecipientPtr& recipient : clientPartyPtr->recipients())
   {
      PartyRecipientPacket* partyRecipientPacket = partyPacket->add_recipient();
      partyRecipientPacket->set_user_name(recipient->userName());
      partyRecipientPacket->set_public_key(recipient->publicKey().toBinStr());
      partyRecipientPacket->set_timestamp_ms(recipient->publicKeyTime().toMSecsSinceEpoch());
   }

   emit sendPacket(privatePartyRequest);
}

void ClientConnectionLogic::handlePrivatePartyRequest(const PrivatePartyRequest& privatePartyRequest)
{
   // 1. check if model have this same party id
   // 2. if have and local party state is initialized then reply initialized state
   // 3. if not create new private party
   // 4. save party id in db

   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   PartyPtr partyPtr = clientPartyModelPtr->getClientPartyById(privatePartyRequest.party_packet().party_id());

   // local party exist
   if (partyPtr)
   {
      // if party request was created by me
      // update recipient data
      PartyPacket partyPacket = privatePartyRequest.party_packet();
      if (currentUserPtr()->userName() == partyPacket.party_creator_hash())
      {
         ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyPacket.party_id());

         if (nullptr == clientPartyPtr)
         {
            return;
         }

         PartyRecipientsPtrList updatedRecipients;
         for (int i = 0; i < partyPacket.recipient_size(); i++)
         {
            PartyRecipientPacket recipient = partyPacket.recipient(i);
            PartyRecipientPtr newRecipient = std::make_shared<PartyRecipient>(recipient.user_name(), recipient.public_key(), QDateTime::fromMSecsSinceEpoch(recipient.timestamp_ms()));
            updatedRecipients.push_back(newRecipient);
         }

         clientPartyPtr->setRecipients(updatedRecipients);

         return;
      }

      // party is in initialized or rejected state (already accepted)
      // send this state to requester

      if (PartyState::INITIALIZED == partyPtr->partyState())
      {
         acceptPrivateParty(partyPtr->id());
         return;
      }

      if (PartyState::REJECTED == partyPtr->partyState())
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
   requestSessionKey.set_sender_user_name(currentUserPtr()->userName());
   requestSessionKey.set_encoded_public_key(encodedLocalSessionPublicKey.toBinStr());
   requestSessionKey.set_receiver_user_name(receieverUserName);

   sendPacket(requestSessionKey);
}

void ClientConnectionLogic::replySessionKeyExchange(const std::string& receieverUserName, const BinaryData& encodedLocalSessionPublicKey)
{
   ReplySessionKeyExchange replyKeyExchange;
   replyKeyExchange.set_sender_user_name(currentUserPtr()->userName());
   replyKeyExchange.set_encoded_public_key(encodedLocalSessionPublicKey.toBinStr());
   replyKeyExchange.set_receiver_user_name(receieverUserName);

   sendPacket(replyKeyExchange);
}

void ClientConnectionLogic::handleRequestSessionKeyExchange(const RequestSessionKeyExchange& requestKeyExchange)
{
   sessionKeyHolderPtr_->onIncomingRequestSessionKeyExchange(requestKeyExchange.sender_user_name(), requestKeyExchange.encoded_public_key(), currentUserPtr()->privateKey());
}

void ClientConnectionLogic::handleReplySessionKeyExchange(const ReplySessionKeyExchange& replyKeyExchange)
{
   sessionKeyHolderPtr_->onIncomingReplySessionKeyExchange(replyKeyExchange.sender_user_name(), replyKeyExchange.encoded_public_key());
}

void ClientConnectionLogic::prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
{
   // prepare
   auto partyId = clientPartyPtr->id();
   auto messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
   auto timestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
   auto message = data;
   auto encryptionType = Chat::EncryptionType::UNENCRYPTED;
   auto partyMessageState = Chat::PartyMessageState::UNSENT;

   PartyMessagePacket partyMessagePacket;
   partyMessagePacket.set_party_id(partyId);
   partyMessagePacket.set_message_id(messageId);
   partyMessagePacket.set_timestamp_ms(timestamp);
   partyMessagePacket.set_message(message);
   partyMessagePacket.set_encryption(encryptionType);
   partyMessagePacket.set_nonce("");
   partyMessagePacket.set_party_message_state(partyMessageState);
   partyMessagePacket.set_sender_hash(currentUserPtr()->userName());

   // save in db
   clientDBServicePtr_->saveMessage(ProtobufUtils::pbMessageToString(partyMessagePacket));

   // call session key handler
   PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userName());
   for (const auto recipient : recipients)
   {
      sessionKeyHolderPtr_->requestSessionKeysForUser(recipient->userName(), recipient->publicKey());
   }
}

void ClientConnectionLogic::sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr)
{
   // read msg from db
   std::string receiverUserName = sessionKeyDataPtr->userName();
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   PartyPtr partyPtr = clientPartyModelPtr->getPartyByUserName(receiverUserName);

   clientDBServicePtr_->readUnsentMessages(partyPtr->id());
}

void ClientConnectionLogic::sessionKeysForUserFailed(const std::string& userName)
{
   // ! not implemented
   // this function is called after sendmessage
}

void ClientConnectionLogic::messageLoaded(const std::string& partyId, const std::string& messageId, const qint64 timestamp,
   const std::string& message, const int encryptionType, const std::string& nonce, const int partyMessageState)
{
   Q_UNUSED(encryptionType);
   Q_UNUSED(partyMessageState);

   // 1. encrypt by aead
   // 2. send msg
   // 3. update message state in db

   // we need only unsent messages
   if (PartyMessageState::UNSENT != partyMessageState)
   {
      return;
   }

   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);

   PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userName());
   for (const PartyRecipientPtr& recipient : recipients)
   {
      // we need to be sure here that sessionKeyDataPtr is properly initialized
      SessionKeyDataPtr sessionKeyDataPtr = sessionKeyHolderPtr_->sessionKeyDataForUser(recipient->userName());
      if (!sessionKeyDataPtr->isInitialized())
      {
         // sorry, not today
         continue;
      }

      // use AEAD encryption for online clients
      if (clientPartyPtr->clientStatus() == ClientStatus::ONLINE)
      {
         BinaryData nonce = sessionKeyDataPtr->nonce();
         std::string associatedData = cryptManagerPtr_->jsonAssociatedData(partyId, nonce);

         QFuture<std::string> future = cryptManagerPtr_->encryptMessageAEAD(
            message, associatedData, sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());
         std::string encryptedMessage = future.result();

         PartyMessagePacket partyMessagePacket;
         partyMessagePacket.set_party_id(clientPartyPtr->id());
         partyMessagePacket.set_message_id(messageId);
         partyMessagePacket.set_timestamp_ms(timestamp);
         partyMessagePacket.set_encryption(EncryptionType::AEAD);
         partyMessagePacket.set_message(encryptedMessage);
         partyMessagePacket.set_nonce(nonce.toBinStr());
         partyMessagePacket.set_party_message_state(PartyMessageState::SENT);

         sendPacket(partyMessagePacket);

         clientDBServicePtr_->updateMessageState(messageId, PartyMessageState::SENT);
         continue;
      }

      // in other case use IES encryption
      QFuture<std::string> future = cryptManagerPtr_->encryptMessageIES(message, recipient->publicKey());
      std::string encryptedMessage = future.result();

      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.set_party_id(clientPartyPtr->id());
      partyMessagePacket.set_message_id(messageId);
      partyMessagePacket.set_timestamp_ms(timestamp);
      partyMessagePacket.set_encryption(EncryptionType::IES);
      partyMessagePacket.set_party_message_state(PartyMessageState::SENT);

      sendPacket(partyMessagePacket);

      clientDBServicePtr_->updateMessageState(messageId, PartyMessageState::SENT);
   }
}

void ClientConnectionLogic::unsentMessagesFound(const std::string& partyId)
{
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);

   if (!clientPartyPtr)
   {
      return;
   }

   PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(currentUserPtr()->userName());
   for (const auto recipient : recipients)
   {
      sessionKeyHolderPtr_->requestSessionKeysForUser(recipient->userName(), recipient->publicKey());
   }
}

void ClientConnectionLogic::rejectPrivateParty(const std::string& partyId)
{
   RequestPrivatePartyStateChange requestPrivatePartyStateChange;

   requestPrivatePartyStateChange.set_party_id(partyId);
   requestPrivatePartyStateChange.set_party_state(PartyState::REJECTED);

   sendPacket(requestPrivatePartyStateChange);
}

void ClientConnectionLogic::acceptPrivateParty(const std::string& partyId)
{
   RequestPrivatePartyStateChange requestPrivatePartyStateChange;

   requestPrivatePartyStateChange.set_party_id(partyId);
   requestPrivatePartyStateChange.set_party_state(PartyState::INITIALIZED);

   sendPacket(requestPrivatePartyStateChange);
}

void ClientConnectionLogic::handlePrivatePartyStateChanged(const PrivatePartyStateChanged& privatePartyStateChanged)
{
   ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
   ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(privatePartyStateChanged.party_id());

   if (nullptr == clientPartyPtr)
   {
      emit error(ClientConnectionLogicError::CouldNotFindParty, privatePartyStateChanged.party_id());
      return;
   }

   clientPartyPtr->setPartyState(privatePartyStateChanged.party_state());
}

void ClientConnectionLogic::handleReplySearchUser(const ReplySearchUser& replySearchUser)
{
   SearchUserReplyList searchUserReplyList;

   for (const auto& searchUser : replySearchUser.user_name())
   {
      searchUserReplyList.push_back(searchUser);
   }

   emit searchUserReply(searchUserReplyList, replySearchUser.search_id());
}

void ClientConnectionLogic::searchUser(const std::string& userHash, const std::string& searchId)
{
   RequestSearchUser requestSearchUser;
   requestSearchUser.set_search_id(searchId);
   requestSearchUser.set_search_text(userHash);

   emit sendPacket(requestSearchUser);
}

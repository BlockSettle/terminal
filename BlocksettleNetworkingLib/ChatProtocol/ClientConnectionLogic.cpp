#include <QtDebug>
#include <QThread>
#include <QUuid>
#include <QDateTime>
#include <QFutureWatcher>

#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ClientConnectionLogic.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientPartyModel.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   ClientConnectionLogic::ClientConnectionLogic(const ClientPartyLogicPtr& clientPartyLogicPtr, const ApplicationSettingsPtr& appSettings, 
      const ClientDBServicePtr& clientDBServicePtr, const LoggerPtr& loggerPtr, const Chat::CryptManagerPtr& cryptManagerPtr, QObject* parent /* = nullptr */)
      : QObject(parent), cryptManagerPtr_(cryptManagerPtr), loggerPtr_(loggerPtr), clientDBServicePtr_(clientDBServicePtr), appSettings_(appSettings), clientPartyLogicPtr_(clientPartyLogicPtr)
   {
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
      any.ParseFromString(data);

      loggerPtr_->debug("[ClientConnectionLogic::onDataReceived] Data: {}", ProtobufUtils::toJsonReadable(any));

      WelcomeResponse welcomeResponse;
      if (ProtobufUtils::pbStringToMessage<WelcomeResponse>(data, &welcomeResponse))
      {
         handleWelcomeResponse(welcomeResponse);
         emit properlyConnected();
         return;
      }

      LogoutResponse logoutResponse;
      if (ProtobufUtils::pbStringToMessage<LogoutResponse>(data, &logoutResponse))
      {
         handleLogoutResponse(logoutResponse);
         return;
      }

      StatusChanged statusChanged;
      if (ProtobufUtils::pbStringToMessage<StatusChanged>(data, &statusChanged))
      {
         handleStatusChanged(statusChanged);
         return;
      }

      PartyMessageStateUpdate partyMessageStateUpdate;
      if (ProtobufUtils::pbStringToMessage<PartyMessageStateUpdate>(data, &partyMessageStateUpdate))
      {
         handlePartyMessageStateUpdate(partyMessageStateUpdate);
         return;
      }

      PartyMessagePacket partyMessagePacket;
      if (ProtobufUtils::pbStringToMessage<PartyMessagePacket>(data, &partyMessagePacket))
      {
         handlePartyMessagePacket(partyMessagePacket);
         return;
      }

      PrivatePartyRequest privatePartyRequest;
      if (ProtobufUtils::pbStringToMessage<PrivatePartyRequest>(data, &privatePartyRequest))
      {
         handlePrivatePartyRequest(privatePartyRequest);
         return;
      }

      RequestSessionKeyExchange requestSessionKey;
      if (ProtobufUtils::pbStringToMessage<RequestSessionKeyExchange>(data, &requestSessionKey))
      {
         handleRequestSessionKeyExchange(requestSessionKey);
         return;
      }

      ReplySessionKeyExchange replyKeyExchange;
      if (ProtobufUtils::pbStringToMessage<ReplySessionKeyExchange>(data, &replyKeyExchange))
      {
         handleReplySessionKeyExchange(replyKeyExchange);
         return;
      }

      QString what = QString::fromLatin1("data: %1").arg(QString::fromStdString(data));
      emit error(ClientConnectionLogicError::UnhandledPacket, what.toStdString());
   }

   void ClientConnectionLogic::onConnected(void)
   {
      qDebug() << "ClientConnectionLogic::onConnected Thread ID:" << this->thread()->currentThreadId();

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

   void ClientConnectionLogic::handleWelcomeResponse(const google::protobuf::Message& msg)
   {
      WelcomeResponse welcomeResponse;
      welcomeResponse.CopyFrom(msg);

      if (!welcomeResponse.success())
      {
         emit closeConnection();
         return;
      }

      clientPartyLogicPtr_->handlePartiesFromWelcomePacket(msg);
   }

   void ClientConnectionLogic::handleLogoutResponse(const google::protobuf::Message& msg)
   {
      emit closeConnection();
   }

   void ClientConnectionLogic::handleStatusChanged(const google::protobuf::Message& msg)
   {
      StatusChanged statusChanged;
      statusChanged.CopyFrom(msg);

      emit userStatusChanged(statusChanged.user_name(), statusChanged.client_status());
   }

   void ClientConnectionLogic::handlePartyMessageStateUpdate(const google::protobuf::Message& msg)
   {
      PartyMessageStateUpdate partyMessageStateUpdate;
      partyMessageStateUpdate.CopyFrom(msg);

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
      partyMessagePacket.set_sender(currentUserPtr()->userName());

      clientDBServicePtr_->saveMessage(ProtobufUtils::pbMessageToString(partyMessagePacket));

      emit sendPacket(partyMessagePacket);
   }

   void ClientConnectionLogic::handleLocalErrors(const Chat::ClientConnectionLogicError& errorCode, const std::string& what)
   {
      loggerPtr_->debug("[ClientConnectionLogic::handleLocalErrors] Error: {}, what: {}", static_cast<int>(errorCode), what);
   }

   void ClientConnectionLogic::handlePartyMessagePacket(const google::protobuf::Message& msg)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(msg);

      ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

      // TODO: handle here state changes of the rest of message types
      if (clientPartyPtr->isPrivateStandard())
      {
         incomingPrivatePartyMessage(msg);
         return;
      }

      if (clientPartyPtr->isGlobalStandard())
      {
         incomingGlobalPartyMessage(msg);
         return;
      }
   }

   void ClientConnectionLogic::incomingGlobalPartyMessage(const google::protobuf::Message& msg)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(msg);

      ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

      saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
   }

   void ClientConnectionLogic::incomingPrivatePartyMessage(const google::protobuf::Message& msg)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(msg);

      ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

      PartyRecipientPtr recipientPtr = clientPartyPtr->getSecondRecipient(currentUserPtr()->userName());

      if (partyMessagePacket.encryption() == EncryptionType::AEAD)
      {
         SessionKeyDataPtr sessionKeyDataPtr = sessionKeyHolderPtr_->sessionKeyDataForUser(recipientPtr->userName());

         BinaryData nonce = partyMessagePacket.nonce();
         std::string associatedData = cryptManagerPtr_->jsonAssociatedData(clientPartyPtr->id(), nonce);

         QFutureWatcher<std::string>* watcher = new QFutureWatcher<std::string>(this);
         connect(watcher, &QFutureWatcher<std::string>::finished,
            [this, watcher, partyMessagePacket, nonce]() mutable
            {
               std::string decryptedMessage = watcher->result();
               watcher->deleteLater();

               partyMessagePacket.set_message(decryptedMessage);

               saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
            });

         QFuture<std::string> future = cryptManagerPtr_->decryptMessageAEAD(partyMessagePacket.message(), associatedData,
            sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());

         watcher->setFuture(future);
      }

      if (partyMessagePacket.encryption() == EncryptionType::IES)
      {
         QFutureWatcher<std::string>* watcher = new QFutureWatcher<std::string>(this);
         connect(watcher, &QFutureWatcher<std::string>::finished,
            [this, watcher, partyMessagePacket]() mutable
            {
               std::string decryptedMessage = watcher->result();
               watcher->deleteLater();

               partyMessagePacket.set_message(decryptedMessage);

               saveIncomingPartyMessageAndUpdateState(partyMessagePacket, PartyMessageState::RECEIVED);
            });

         QFuture<std::string> future = cryptManagerPtr_->decryptMessageIES(partyMessagePacket.message(), currentUserPtr()->privateKey());
         watcher->setFuture(future);
      }
   }

   void ClientConnectionLogic::saveIncomingPartyMessageAndUpdateState(const google::protobuf::Message& msg, const PartyMessageState& partyMessageState)
   {
      PartyMessagePacket partyMessagePacket;
      partyMessagePacket.CopyFrom(msg);

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
         emit error(ClientConnectionLogicError::MessageSeenForWrongTypeOfParty, clientPartyPtr->id());
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

      PrivatePartyRequest privatePartyRequest;
      PartyPacket *partyPacket = privatePartyRequest.mutable_party_packet();
      partyPacket->set_party_id(partyId);
      partyPacket->set_display_name(clientPartyPtr->getSecondRecipient(currentUserPtr()->userName())->userName());
      partyPacket->set_party_type(clientPartyPtr->partyType());
      partyPacket->set_party_subtype(clientPartyPtr->partySubType());
      partyPacket->set_party_state(clientPartyPtr->partyState());

      for (const PartyRecipientPtr& recipient : clientPartyPtr->recipients())
      {
         PartyRecipientPacket* partyRecipientPacket = partyPacket->add_recipient();
         partyRecipientPacket->set_user_name(recipient->userName());
         partyRecipientPacket->set_public_key(recipient->publicKey().toBinStr());
         partyRecipientPacket->set_timestamp_ms(recipient->publicKeyTime().toMSecsSinceEpoch());
      }

      emit sendPacket(privatePartyRequest);
   }

   void ClientConnectionLogic::handlePrivatePartyRequest(const google::protobuf::Message& msg)
   {
      PrivatePartyRequest privatePartyRequest;
      privatePartyRequest.CopyFrom(msg);

      // 1. check if model have this same party id
      // 2. if have and local party state is initialized then reply initialized state
      // 3. if not create new private party
      // 4. save party id in db

      ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
      PartyPtr partyPtr = clientPartyModelPtr->getClientPartyById(privatePartyRequest.party_packet().party_id());

      // local party exist
      if (partyPtr)
      {
         if (PartyState::INITIALIZED == partyPtr->partyState() || PartyState::REJECTED == partyPtr->partyState())
         {
            // party is in initialized or rejected state (already accepted)
            // send this state to requester
            sendPrivatePartyState(partyPtr->id(), partyPtr->partyState());
            return;
         }

         return;
      }

      // local party not exist, create new one
      clientPartyLogicPtr_->createPrivatePartyFromPrivatePartyRequest(currentUserPtr(), privatePartyRequest);
   }

   void ClientConnectionLogic::sendPrivatePartyState(const std::string& partyId, const Chat::PartyState& partyState)
   {
      PrivatePartyRequest privatePartyRequest;
      PartyPacket* partyPacket = privatePartyRequest.mutable_party_packet();
      partyPacket->set_party_id(partyId);
      partyPacket->set_party_state(partyState);

      sendPacket(privatePartyRequest);
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

   void ClientConnectionLogic::handleRequestSessionKeyExchange(const google::protobuf::Message& msg)
   {
      RequestSessionKeyExchange requestKeyExchange;
      requestKeyExchange.CopyFrom(msg);

      sessionKeyHolderPtr_->onIncomingRequestSessionKeyExchange(requestKeyExchange.sender_user_name(), requestKeyExchange.encoded_public_key(), currentUserPtr()->privateKey());
   }

   void ClientConnectionLogic::handleReplySessionKeyExchange(const google::protobuf::Message& msg)
   {
      ReplySessionKeyExchange replyKeyExchange;
      replyKeyExchange.CopyFrom(msg);

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
      partyMessagePacket.set_sender(currentUserPtr()->userName());

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

            QFutureWatcher<std::string>* watcher = new QFutureWatcher<std::string>(this);
            connect(watcher, &QFutureWatcher<std::string>::finished,
               [this, watcher, clientPartyPtr, messageId, timestamp, sessionKeyDataPtr, nonce]()
               {
                  std::string encryptedMessage = watcher->result();
                  watcher->deleteLater();

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
               });

            QFuture<std::string> future = cryptManagerPtr_->encryptMessageAEAD(
               message, associatedData, sessionKeyDataPtr->localSessionPrivateKey(), nonce, sessionKeyDataPtr->remoteSessionPublicKey());

            watcher->setFuture(future);

            continue;
         }

         // in other case use IES encryption
         QFutureWatcher<std::string>* watcher = new QFutureWatcher<std::string>(this);
         connect(watcher, &QFutureWatcher<std::string>::finished,
            [this, watcher, clientPartyPtr, messageId, timestamp, sessionKeyDataPtr, nonce]()
            {
               std::string encryptedMessage = watcher->result();
               watcher->deleteLater();

               PartyMessagePacket partyMessagePacket;
               partyMessagePacket.set_party_id(clientPartyPtr->id());
               partyMessagePacket.set_message_id(messageId);
               partyMessagePacket.set_timestamp_ms(timestamp);
               partyMessagePacket.set_encryption(EncryptionType::IES);
               partyMessagePacket.set_party_message_state(PartyMessageState::SENT);

               sendPacket(partyMessagePacket);

               clientDBServicePtr_->updateMessageState(messageId, PartyMessageState::SENT);
            });

         QFuture<std::string> future = cryptManagerPtr_->encryptMessageIES(message, recipient->publicKey());

         watcher->setFuture(future);
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

}

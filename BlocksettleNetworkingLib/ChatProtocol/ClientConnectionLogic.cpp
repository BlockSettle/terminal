#include <QtDebug>
#include <QThread>
#include <QUuid>
#include <QDateTime>

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
      const ClientDBServicePtr& clientDBServicePtr, const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : QObject(parent), loggerPtr_(loggerPtr), clientDBServicePtr_(clientDBServicePtr), appSettings_(appSettings), clientPartyLogicPtr_(clientPartyLogicPtr)
   {
      connect(this, &ClientConnectionLogic::userStatusChanged, clientPartyLogicPtr_.get(), &ClientPartyLogic::onUserStatusChanged);
      connect(this, &ClientConnectionLogic::error, this, &ClientConnectionLogic::handleLocalErrors);

      sessionKeyHolderPtr_ = std::make_shared<SessionKeyHolder>(loggerPtr_, this);
      connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::requestSessionKeyExchange, this, &ClientConnectionLogic::requestSessionKeyExchange);
      connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::replySessionKeyExchange, this, &ClientConnectionLogic::replySessionKeyExchange);
      connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::sessionKeysForUser, this, &ClientConnectionLogic::sessionKeysForUser);
      connect(sessionKeyHolderPtr_.get(), &SessionKeyHolder::sessionKeysForUserFailed, this, &ClientConnectionLogic::sessionKeysForUserFailed);
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
         emit testProperlyConnected();
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
      if (PartyType::GLOBAL == clientPartyPtr->partyType() && PartySubType::STANDARD == clientPartyPtr->partySubType())
      {
         prepareAndSendGlobalMessage(clientPartyPtr, data);
         return;
      }

      if (PartyType::PRIVATE_DIRECT_MESSAGE == clientPartyPtr->partyType() && PartySubType::STANDARD == clientPartyPtr->partySubType())
      {
         prepareAndSendPrivateMessage(clientPartyPtr, data);
         return;
      }

      emit error(ClientConnectionLogicError::SendingDataToUnhandledParty, clientPartyPtr->id());
   }

   void ClientConnectionLogic::prepareAndSendGlobalMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
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

      // save message as it is
      clientDBServicePtr_->saveMessage(ProtobufUtils::pbMessageToString(partyMessagePacket));

      ClientPartyModelPtr clientPartyModelPtr = clientPartyLogicPtr_->clientPartyModelPtr();
      ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyMessagePacket.party_id());

      // TODO: handle here state changes of the rest of message types
      if (Chat::PartyType::PRIVATE_DIRECT_MESSAGE == clientPartyPtr->partyState() && Chat::PartySubType::STANDARD == clientPartyPtr->partySubType())
      {
         // private chat, reply that message was received
         PartyMessageStateUpdate partyMessageStateUpdate;
         partyMessageStateUpdate.set_party_id(partyMessagePacket.party_id());
         partyMessageStateUpdate.set_message_id(partyMessagePacket.message_id());
         partyMessageStateUpdate.set_party_message_state(Chat::PartyMessageState::RECEIVED);

         emit sendPacket(partyMessageStateUpdate);
      }

      // save received message state in db
      auto partyMessageState = Chat::PartyMessageState::RECEIVED;
      clientDBServicePtr_->updateMessageState(partyMessagePacket.message_id(), partyMessageState);
   }

   void ClientConnectionLogic::setMessageSeen(const ClientPartyPtr& clientPartyPtr, const std::string& messageId)
   {
      if (!(Chat::PartyType::PRIVATE_DIRECT_MESSAGE == clientPartyPtr->partyState() && Chat::PartySubType::STANDARD == clientPartyPtr->partySubType()))
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
      // encrypt by aead
      // send msg
      // delete msg in db
   }

   void ClientConnectionLogic::sessionKeysForUserFailed(const std::string& userName)
   {

   }
}

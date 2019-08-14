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

      QString what = QString::fromLatin1("data: %1").arg(QString::fromStdString(data));
      emit error(ClientConnectionLogicError::UnhandledPacket, what.toStdString());
   }

   void ClientConnectionLogic::onConnected(void)
   {
      qDebug() << "ClientConnectionLogic::onConnected Thread ID:" << this->thread()->currentThreadId();

      Chat::WelcomeRequest welcomeRequest;
      welcomeRequest.set_user_name(currentUserPtr()->displayName());
      welcomeRequest.set_client_public_key(currentUserPtr()->publicKey().toBinStr());

      emit sendPacket(welcomeRequest);
   }

   void ClientConnectionLogic::onDisconnected(void)
   {

   }

   void ClientConnectionLogic::onError(DataConnectionListener::DataConnectionError)
   {

   }
/*
   template<typename T>
   bool ClientConnectionLogic::pbStringToMessage(const std::string& packetString, google::protobuf::Message* msg)
   {
      google::protobuf::Any any;
      any.ParseFromString(packetString);

      if (any.Is<T>())
      {
         if (!any.UnpackTo(msg))
         {
            loggerPtr_->debug("[ServerConnectionLogic::pbStringToMessage] Can't unpack to {}", typeid(T).name());
            return false;
         }

         return true;
      }

      return false;
   }
*/
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

   void ClientConnectionLogic::prepareAndSendPrivateMessage(const ClientPartyPtr& clientPartyPtr, const std::string& data)
   {
      // TODO
   }

   void ClientConnectionLogic::prepareRequestPrivateParty(const std::string& userName)
   {

   }

}

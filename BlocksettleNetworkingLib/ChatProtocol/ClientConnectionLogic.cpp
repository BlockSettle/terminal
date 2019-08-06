#include <QtDebug>
#include <QThread>

#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ClientConnectionLogic.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   ClientConnectionLogic::ClientConnectionLogic(const ClientPartyLogicPtr& clientPartyLogicPtr, const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : QObject(parent), loggerPtr_(loggerPtr), appSettings_(appSettings), clientPartyLogicPtr_(clientPartyLogicPtr)
   {

   }

   void ClientConnectionLogic::onDataReceived(const std::string& data)
   {
      google::protobuf::Any any;
      any.ParseFromString(data);

      loggerPtr_->debug("[ClientConnectionLogic::onDataReceived] Data: {}", ProtobufUtils::toJsonReadable(any));

      WelcomeResponse welcomeResponse;
      if (pbStringToMessage<WelcomeResponse>(data, &welcomeResponse))
      {
         handleWelcomeResponse(welcomeResponse);
         return;
      }

      LogoutResponse logoutResponse;
      if (pbStringToMessage<LogoutResponse>(data, &logoutResponse))
      {
         handleLogoutResponse(logoutResponse);
         return;
      }

      StatusChanged statusChanged;
      if (pbStringToMessage<StatusChanged>(data, &statusChanged))
      {
         handleStatusChanged(statusChanged);
         return;
      }
   }

   void ClientConnectionLogic::onConnected(void)
   {
      qDebug() << "ClientConnectionLogic::onConnected Thread ID:" << this->thread()->currentThreadId();

      Chat::WelcomeRequest welcomeRequest;
      welcomeRequest.set_user_name(currentUserPtr()->displayName());
      welcomeRequest.set_client_public_key(currentUserPtr()->publicKey().toBinStr());

      emit sendRequestPacket(welcomeRequest);
   }

   void ClientConnectionLogic::onDisconnected(void)
   {

   }

   void ClientConnectionLogic::onError(DataConnectionListener::DataConnectionError)
   {

   }

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

}

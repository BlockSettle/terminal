#include <QtDebug>
#include <QThread>

#include <google/protobuf/any.pb.h>

#include "ChatProtocol/ClientConnectionLogic.h"
#include "ProtobufUtils.h"
#include "chat.pb.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   ClientConnectionLogic::ClientConnectionLogic(const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : appSettings_(appSettings), loggerPtr_(loggerPtr), QObject(parent)
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

      return nullptr;
   }

   void ClientConnectionLogic::handleWelcomeResponse(const google::protobuf::Message& msg)
   {
      WelcomeResponse welcomeResponse;
      welcomeResponse.CheckTypeAndMergeFrom(msg);

      if (!welcomeResponse.success())
      {
         emit closeConnection();
         return;
      }
   }

   void ClientConnectionLogic::handleLogoutResponse(const google::protobuf::Message& msg)
   {
      LogoutResponse logoutResponse;
      logoutResponse.CheckTypeAndMergeFrom(msg);

      emit closeConnection();
   }

}
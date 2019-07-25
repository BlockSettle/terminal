#include <QtDebug>
#include <QThread>

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

   void ClientConnectionLogic::onDataReceived(const std::string&)
   {

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

}
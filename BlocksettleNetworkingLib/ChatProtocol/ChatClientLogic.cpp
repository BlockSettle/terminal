#include <QtDebug>

#include "ChatProtocol/ChatClientLogic.h"

#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "UserHasher.h"
#include "ProtobufUtils.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

#include "chat.pb.h"

namespace Chat
{

   ChatClientLogic::ChatClientLogic()
   {
      qRegisterMetaType<DataConnectionListener::DataConnectionError>();
   }

   ChatClientLogic::~ChatClientLogic()
   {

   }

   void ChatClientLogic::Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettingsPtr, const LoggerPtr& loggerPtr)
   {
      qDebug() << "ChatClientLogic::Init Thread ID:" << this->thread()->currentThreadId();

      if (connectionManagerPtr_) {
         // already initialized
         emit chatClientError(ChatClientLogicError::AlreadyInitialized);
         return;
      }

      connectionManagerPtr_ = connectionManagerPtr;
      applicationSettingsPtr_ = appSettingsPtr;
      loggerPtr_ = loggerPtr;
      
      currentUserPtr_ = std::make_shared<ChatUser>();
      const auto publicKey = applicationSettingsPtr_->GetAuthKeys().second;
      currentUserPtr_->setPublicKey(BinaryData(publicKey.data(), publicKey.size()));
      connect(currentUserPtr_.get(), &ChatUser::displayNameChanged, this, &ChatClientLogic::chatUserDisplayNameChanged);

      userHasherPtr_ = std::make_shared<UserHasher>();

      connectionLogicPtr_ = std::make_shared<ClientConnectionLogic>(appSettingsPtr, loggerPtr);
      connectionLogicPtr_->setCurrentUserPtr(currentUserPtr_);
      connect(this, &ChatClientLogic::dataReceived, connectionLogicPtr_.get(), &ClientConnectionLogic::onDataReceived);
      connect(this, &ChatClientLogic::connected, connectionLogicPtr_.get(), &ClientConnectionLogic::onConnected);
      connect(this, &ChatClientLogic::disconnected, connectionLogicPtr_.get(), &ClientConnectionLogic::onDisconnected);
      connect(this, qOverload<DataConnectionListener::DataConnectionError>(&ChatClientLogic::error), 
         connectionLogicPtr_.get(), qOverload<DataConnectionListener::DataConnectionError>(&ClientConnectionLogic::onError));

      connect(connectionLogicPtr_.get(), &ClientConnectionLogic::sendRequestPacket, this, &ChatClientLogic::sendRequestPacket);
      connect(connectionLogicPtr_.get(), &ClientConnectionLogic::closeConnection, this, &ChatClientLogic::onCloseConnection);
   }

   void ChatClientLogic::LoginToServer(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb& cb)
   {
      if (connectionPtr_) {
         loggerPtr_->error("[ChatClientLogic::{}] connecting with not purged connection", __func__);

         emit chatClientError(ChatClientLogicError::ConnectionAlreadyUsed);

         return;
      }

      connectionPtr_ = connectionManagerPtr_->CreateZMQBIP15XDataConnection();
      connectionPtr_->setCBs(cb);

      if (!connectionPtr_->openConnection(this->getChatServerHost(), this->getChatServerPort(), this))
      {
         loggerPtr_->error("[ChatClientLogic::{}] failed to open ZMQ data connection", __func__);
         connectionPtr_.reset();

         emit chatClientError(ChatClientLogicError::ZmqDataConnectionFailed);
         emit clientLoggedOutFromServer();
         return;
      }

      currentUserPtr_->setDisplayName(userHasherPtr_->deriveKey(email));

//    currentJwt_ = jwt;
   }

   std::string ChatClientLogic::getChatServerHost() const
   {
      return applicationSettingsPtr_->get<std::string>(ApplicationSettings::chatServerHost);
   }

   std::string ChatClientLogic::getChatServerPort() const
   {
      return applicationSettingsPtr_->get<std::string>(ApplicationSettings::chatServerPort);
   }

   void ChatClientLogic::OnDataReceived(const std::string& data)
   {
      emit dataReceived(data);
   }

   void ChatClientLogic::OnConnected(void)
   {
      emit connected();
   }

   void ChatClientLogic::OnDisconnected(void)
   {
      emit disconnected();
   }

   void ChatClientLogic::OnError(DataConnectionListener::DataConnectionError dataConnectionError)
   {
      emit error(dataConnectionError);
   }

   void ChatClientLogic::sendRequestPacket(const google::protobuf::Message& message)
   {
      qDebug() << "ChatClientLogic::sendRequestPacket Thread ID:" << this->thread()->currentThreadId();

      loggerPtr_->debug("send: {}", ProtobufUtils::toJsonCompact(message));

      if (!connectionPtr_->isActive())
      {
         loggerPtr_->error("[ChatClientLogic::{}] Connection is not alive!", __func__);
         return;
      }

      auto packetString = ProtobufUtils::pbMessageToString(message);

      if (!connectionPtr_->send(packetString))
      {
         loggerPtr_->error("[ChatClientLogic::{}] Failed to send packet!", __func__);
      }
   }

   void ChatClientLogic::LogoutFromServer()
   {
      if (!connectionPtr_)
      {
         emit clientLoggedOutFromServer();
         return;
      }

      LogoutRequest logoutRequest;
      sendRequestPacket(logoutRequest);
   }

   void ChatClientLogic::onCloseConnection()
   {
      connectionPtr_.reset();
      emit clientLoggedOutFromServer();
   }

}

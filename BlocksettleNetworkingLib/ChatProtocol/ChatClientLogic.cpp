#include <QtDebug>

#include "ChatProtocol/ChatClientLogic.h"

#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "UserHasher.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

namespace Chat
{

   ChatClientLogic::ChatClientLogic()
   {
   }

   ChatClientLogic::~ChatClientLogic()
   {

   }

   void ChatClientLogic::Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettingsPtr, const LoggerPtr& loggerPtr)
   {
      if (connectionManagerPtr_) {
         // already initialized
         emit chatClientError(ChatClientLogicError::AlreadyInitialized);
         return;
      }

      connectionManagerPtr_ = connectionManagerPtr;
      applicationSettingsPtr_ = appSettingsPtr;
      loggerPtr_ = loggerPtr;

      currentUserPtr_ = std::make_shared<ChatUser>();
      connect(currentUserPtr_.get(), &ChatUser::displayNameChanged, this, &ChatClientLogic::chatUserDisplayNameChanged);

      userHasherPtr_ = std::make_shared<UserHasher>();

      connectionLogicPtr_ = std::make_shared<ConnectionLogic>(loggerPtr);
      connect(this, &ChatClientLogic::dataReceived, connectionLogicPtr_.get(), &ConnectionLogic::onDataReceived);
      connect(this, &ChatClientLogic::connected, connectionLogicPtr_.get(), &ConnectionLogic::onConnected);
      connect(this, &ChatClientLogic::disconnected, connectionLogicPtr_.get(), &ConnectionLogic::onDisconnected);
      connect(this, qOverload<DataConnectionListener::DataConnectionError>(&ChatClientLogic::error), 
         connectionLogicPtr_.get(), qOverload<DataConnectionListener::DataConnectionError>(&ConnectionLogic::onError));

   }

   void ChatClientLogic::LoginToServer(const std::string& email, const std::string& jwt, const ZmqBIP15XDataConnection::cbNewKey& cb)
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

}

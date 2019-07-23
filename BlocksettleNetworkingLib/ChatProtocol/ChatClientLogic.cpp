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
         emit error(ChatClientLogicError::AlreadyInitialized);
         return;
      }

      connectionManagerPtr_ = connectionManagerPtr;
      applicationSettingsPtr_ = appSettingsPtr;
      loggerPtr_ = loggerPtr;

      currentUserPtr_ = std::make_shared<ChatUser>();
      connect(currentUserPtr_.get(), &ChatUser::displayNameChanged, this, &ChatClientLogic::chatUserDisplayNameChanged);

      userHasherPtr_ = std::make_shared<UserHasher>();
   }

   void ChatClientLogic::LoginToServer(const std::string& email, const std::string& jwt, const ZmqBIP15XDataConnection::cbNewKey& cb)
   {
      if (connectionPtr_) {
         loggerPtr_->error("[ChatClientLogic::{}] connecting with not purged connection", __func__);

         emit error(ChatClientLogicError::ConnectionAlreadyUsed);

         return;
      }

      connectionPtr_ = connectionManagerPtr_->CreateZMQBIP15XDataConnection();
      connectionPtr_->setCBs(cb);

      if (!connectionPtr_->openConnection(this->getChatServerHost(), this->getChatServerPort(), this))
      {
         loggerPtr_->error("[ChatClientLogic::{}] failed to open ZMQ data connection", __func__);
         connectionPtr_.reset();

         emit error(ChatClientLogicError::ZmqDataConnectionFailed);
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

   void ChatClientLogic::OnDataReceived(const std::string&)
   {

   }

   void ChatClientLogic::OnConnected(void)
   {

   }

   void ChatClientLogic::OnDisconnected(void)
   {

   }

   void ChatClientLogic::OnError(DataConnectionListener::DataConnectionError)
   {

   }

}

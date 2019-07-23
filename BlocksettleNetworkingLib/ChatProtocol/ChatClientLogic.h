#ifndef ChatClientLogic_h__
#define ChatClientLogic_h__

#include <QThread>

#include "ChatProtocol/ChatUser.h"

#include "DataConnectionListener.h"

#include <disable_warnings.h>
#include "ZMQ_BIP15X_DataConnection.h"
#include <enable_warnings.h>

namespace spdlog
{
   class logger;
}

class ConnectionManager;
class ApplicationSettings;
class UserHasher;

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using ConnectionManagerPtr = std::shared_ptr<ConnectionManager>;
   using ApplicationSettingsPtr = std::shared_ptr<ApplicationSettings>;
   using UserHasherPtr = std::shared_ptr<UserHasher>;

   enum class ChatClientLogicError
   {
      NoError,
      AlreadyInitialized,
      ConnectionAlreadyUsed,
      ZmqDataConnectionFailed
   };

   class ChatClientLogic : public QObject, public DataConnectionListener
   {
      Q_OBJECT

   public:
      ChatClientLogic();
      ~ChatClientLogic();

      void OnDataReceived(const std::string&) override;
      void OnConnected(void) override;
      void OnDisconnected(void) override;
      void OnError(DataConnectionListener::DataConnectionError) override;

   public slots:
      void Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr);
      void LoginToServer(const std::string& email, const std::string& jwt, const ZmqBIP15XDataConnection::cbNewKey& cb);

   signals:
      void finished();
      void error(const ChatClientLogicError& errorCode);

      void chatUserDisplayNameChanged(const std::string& chatUserDisplayName);

   private:
      std::string getChatServerHost() const;
      std::string getChatServerPort() const;

      ConnectionManagerPtr       connectionManagerPtr_;
      ZmqBIP15XDataConnectionPtr connectionPtr_;
      LoggerPtr                  loggerPtr_;
      ApplicationSettingsPtr     applicationSettingsPtr_;
      UserHasherPtr              userHasherPtr_;
      ChatUserPtr                currentUserPtr_;
   };

}

#endif // ChatClientLogic_h__
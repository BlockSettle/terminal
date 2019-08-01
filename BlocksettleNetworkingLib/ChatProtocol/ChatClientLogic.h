#ifndef ChatClientLogic_h__
#define ChatClientLogic_h__

#include <QThread>
#include <google/protobuf/message.h>

#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/ClientConnectionLogic.h"
#include "ChatProtocol/ClientPartyLogic.h"

#include "DataConnectionListener.h"

#include <disable_warnings.h>
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_Helpers.h"
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

   Q_DECLARE_METATYPE(DataConnectionListener::DataConnectionError)
   Q_DECLARE_METATYPE(ChatClientLogicError)
   Q_DECLARE_METATYPE(ClientPartyLogicPtr)

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

      ClientPartyLogicPtr clientPartyLogicPtr() const { return clientPartyLogicPtr_; }

   public slots:
      void Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr);
      void LoginToServer(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb& cb);
      void LogoutFromServer();

   signals:
      void dataReceived(const std::string&);
      void connected(void);
      void disconnected(void);
      void error(DataConnectionListener::DataConnectionError);

      void finished();
      void chatClientError(const ChatClientLogicError& errorCode);

      void chatUserDisplayNameChanged(const std::string& chatUserDisplayName);
      void clientLoggedOutFromServer();

   private slots:
      void sendRequestPacket(const google::protobuf::Message& message);
      void onCloseConnection();

   private:
      void setClientPartyLogicPtr(ClientPartyLogicPtr val) { clientPartyLogicPtr_ = val; }
      std::string getChatServerHost() const;
      std::string getChatServerPort() const;

      ConnectionManagerPtr       connectionManagerPtr_;
      ZmqBIP15XDataConnectionPtr connectionPtr_;
      LoggerPtr                  loggerPtr_;
      ApplicationSettingsPtr     applicationSettingsPtr_;
      UserHasherPtr              userHasherPtr_;
      ChatUserPtr                currentUserPtr_;
      ClientConnectionLogicPtr   clientConnectionLogicPtr_;
      ClientPartyLogicPtr        clientPartyLogicPtr_;
   };

}

#endif // ChatClientLogic_h__
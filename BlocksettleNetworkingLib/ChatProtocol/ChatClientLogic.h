#ifndef CHATCLIENTLOGIC_H
#define CHATCLIENTLOGIC_H

#include <QThread>
#include <google/protobuf/message.h>

#include "ChatProtocol/ChatUser.h"
#include "ChatProtocol/ClientConnectionLogic.h"
#include "ChatProtocol/ClientPartyLogic.h"
#include "ChatProtocol/ClientDBService.h"
#include "ChatProtocol/CryptManager.h"

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
   using SearchUserReplyList = std::vector<std::string>;

   enum class ChatClientLogicError
   {
      NoError,
      ConnectionAlreadyInitialized,
      ConnectionAlreadyUsed,
      ZmqDataConnectionFailed,
      ClientPartyNotExist,
      PartyNotExist
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

      ClientPartyModelPtr clientPartyModelPtr() const { return clientPartyLogicPtr_->clientPartyModelPtr(); }

   public slots:
      void Init(const Chat::ConnectionManagerPtr& connectionManagerPtr, const Chat::ApplicationSettingsPtr& appSettings, const Chat::LoggerPtr& loggerPtr);
      void LoginToServer(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb& cb);
      void LogoutFromServer();
      void SendPartyMessage(const std::string& partyId, const std::string& data);
      void SetMessageSeen(const std::string& partyId, const std::string& messageId);
      void RequestPrivateParty(const std::string& userName);
      void RejectPrivateParty(const std::string& partyId);
      void DeletePrivateParty(const std::string& partyId);
      void AcceptPrivateParty(const std::string& partyId);
      void SearchUser(const std::string& userHash, const std::string& searchId);

   signals:
      void dataReceived(const std::string&);
      void connected(void);
      void disconnected(void);
      void error(DataConnectionListener::DataConnectionError);

      void messagePacketSent(const std::string& messageId);

      void finished();
      void chatClientError(const Chat::ChatClientLogicError& errorCode, const std::string& what = "");

      void chatUserUserNameChanged(const std::string& chatUserDisplayName);
      void clientLoggedOutFromServer();
      void partyModelChanged();
      void initDone();
      void properlyConnected();
      void searchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);

   private slots:
      void sendPacket(const google::protobuf::Message& message);
      void onCloseConnection();
      void handleLocalErrors(const ChatClientLogicError& errorCode, const std::string& what);
      void initDbDone();
      void privatePartyCreated(const std::string& partyId);
      void privatePartyAlreadyExist(const std::string& partyId);

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
      ClientDBServicePtr         clientDBServicePtr_;
      CryptManagerPtr            cryptManagerPtr_;
   };

}

Q_DECLARE_METATYPE(DataConnectionListener::DataConnectionError)
Q_DECLARE_METATYPE(Chat::ChatClientLogicError)
Q_DECLARE_METATYPE(Chat::ClientPartyLogicPtr)
Q_DECLARE_METATYPE(Chat::ChatUserPtr)

#endif // CHATCLIENTLOGIC_H

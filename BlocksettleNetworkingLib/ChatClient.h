#ifndef __CHAT_CLIENT_H__
#define __CHAT_CLIENT_H__


#include <QObject>
#include <QTimer>

#include "DataConnectionListener.h"
#include "ChatProtocol.h"


namespace spdlog {
   class logger;
}


namespace Chat
{
   class Request;
}


class ConnectionManager;
class ZmqSecuredDataConnection;
class ApplicationSettings;


class ChatClient : public QObject
             , public DataConnectionListener
             , public Chat::ResponseHandler
{
   Q_OBJECT

public:
   ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
            , const std::shared_ptr<ApplicationSettings> &appSettings
            , const std::shared_ptr<spdlog::logger>& logger);

   ~ChatClient() noexcept override = default;

   ChatClient(const ChatClient&) = delete;
   ChatClient& operator = (const ChatClient&) = delete;

   ChatClient(ChatClient&&) = delete;
   ChatClient& operator = (ChatClient&&) = delete;

   std::string loginToServer(const std::string& email, const std::string& jwt);

   void logout();

   void OnHeartbeatPong(Chat::HeartbeatPongResponse& response) override;
   void OnUsersList(Chat::UsersListResponse& response) override;
   void OnMessages(Chat::MessagesResponse& response) override;


public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   QString prependMessage(const QString& messageText, const QString& senderId = QString());

private:
   void sendHeartbeat();

   void sendRequest(const std::shared_ptr<Chat::Request>& request);

signals:
   void ConnectedToServer();
   void ConnectionClosed();
   void ConnectionError(int errorCode);
   void UserUpdate(const QString& userId);
   void MessageUpdate(const QDateTime& dateTime, const QString& text);


public slots:

   void onSendMessage(const QString& message);
   void onSetCurrentPrivateChat(const QString& userId);


private:
   std::shared_ptr<ConnectionManager>    connectionManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>      logger_;

   std::shared_ptr<ZmqSecuredDataConnection> connection_;

   QScopedPointer<QTimer>             heartbeatTimer_;

   std::string                     currentUserId_;
   std::string                     currentChatId_;

};

#endif

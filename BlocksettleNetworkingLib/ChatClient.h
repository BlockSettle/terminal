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
   ~ChatClient() noexcept override;

   ChatClient(const ChatClient&) = delete;
   ChatClient& operator = (const ChatClient&) = delete;
   ChatClient(ChatClient&&) = delete;
   ChatClient& operator = (ChatClient&&) = delete;

   std::string loginToServer(const std::string& email, const std::string& jwt);
   void logout();

   void OnHeartbeatPong(const Chat::HeartbeatPongResponse &) override;
   void OnUsersList(const Chat::UsersListResponse &) override;
   void OnMessages(const Chat::MessagesResponse &) override;
   void OnLoginReturned(const Chat::LoginResponse &) override;

   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   void onSendMessage(const QString& message, const QString &receiver);

private:
   void sendRequest(const std::shared_ptr<Chat::Request>& request);

signals:
   void ConnectedToServer();
   void ConnectionClosed();
   void ConnectionError(int errorCode);

   void LoginFailed();
   void UsersReplace(const std::vector<std::string>& users);
   void UsersAdd(const std::vector<std::string>& users);
   void UsersDel(const std::vector<std::string>& users);
   void MessagesUpdate(const std::vector<std::string>& messages);

private slots:
   void sendHeartbeat();

private:
   std::shared_ptr<ConnectionManager>    connectionManager_;
   std::shared_ptr<ApplicationSettings>  appSettings_;
   std::shared_ptr<spdlog::logger>       logger_;

   std::shared_ptr<ZmqSecuredDataConnection> connection_;

   QTimer            heartbeatTimer_;

   std::string       currentUserId_;
   std::atomic_bool  loggedIn_{ false };
};

#endif   // __CHAT_CLIENT_H__

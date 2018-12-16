#ifndef __CHAT_CLIENT_H__
#define __CHAT_CLIENT_H__


#include <QObject>
#include <QTimer>

#include "DataConnectionListener.h"


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
{
    Q_OBJECT

public:
    ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
               , const std::shared_ptr<ApplicationSettings> &appSettings
               , const std::shared_ptr<spdlog::logger>& logger
               , const std::string& serverPublicKey);

    ~ChatClient() noexcept override = default;

    ChatClient(const ChatClient&) = delete;
    ChatClient& operator = (const ChatClient&) = delete;

    ChatClient(ChatClient&&) = delete;
    ChatClient& operator = (ChatClient&&) = delete;

    void loginToServer(const std::string& hostname, const std::string& port
        , const std::string& login/*, const std::string& password*/);

public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

private:
    void sendHeartbeat();

    void sendRequest(const std::shared_ptr<Chat::Request>& request);

signals:
   void OnConnectedToServer();
   void OnConnectionClosed();
   void OnConnectionError(int errorCode);


private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::string                            serverPublicKey_;

   std::shared_ptr<ZmqSecuredDataConnection> connection_;

   QScopedPointer<QTimer>                 heartbeatTimer_;

};

#endif

#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__


#include <memory>
#include <map>
#include <vector>

#include "ServerConnectionListener.h"
#include "ChatProtocol.h"


namespace spdlog {
   class logger;
}


class ConnectionManager;
class ZmqSecuredServerConnection;
class ApplicationSettings;


class ChatServer : public ServerConnectionListener
                 , public Chat::RequestHandler
{
public:

    ChatServer(const std::shared_ptr<ConnectionManager>& connectionManager
               , const std::shared_ptr<ApplicationSettings> &appSettings
               , const std::shared_ptr<spdlog::logger>& logger);
    ~ChatServer() noexcept override = default;

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator = (const ChatServer&) = delete;

    ChatServer(ChatServer&&) = delete;
    ChatServer& operator = (ChatServer&&) = delete;

    void startServer(const std::string& hostname, const std::string& port);

    std::string getPublicKey() const;


    void OnDataFromClient(const std::string& clientId, const std::string& data) override;

    void OnClientConnected(const std::string& clientId) override;
    void OnClientDisconnected(const std::string& clientId) override;

    void OnPeerConnected(const std::string &) override;
    void OnPeerDisconnected(const std::string &) override;

    void OnHeartbeatPing(Chat::HeartbeatPingRequest& request) override;
    void OnLogin(Chat::LoginRequest& request) override;
    void OnSendMessage(Chat::SendMessageRequest& request) override;
    void OnOnlineUsers(Chat::OnlineUsersRequest& request) override;
    void OnRequestMessages(Chat::MessagesRequest& request) override;


private:

    void generateKeys();
    void sendResponse(const std::string& clientId, const std::shared_ptr<Chat::Response>& response);


private:

   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>        logger_;

   std::shared_ptr<ZmqSecuredServerConnection> connection_;

   std::string privateKey_;
   std::string publicKey_;

   std::map<std::string, bool>      clientsOnline_;
   std::vector<Chat::MessageData>   messages_;

};

#endif

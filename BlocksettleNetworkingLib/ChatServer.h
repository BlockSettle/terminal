#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__


#include <memory>

#include "ServerConnectionListener.h"


namespace spdlog {
   class logger;
}


class ConnectionManager;
class ZmqSecuredServerConnection;


class ChatServer : public ServerConnectionListener
{
public:

    ChatServer(const std::shared_ptr<ConnectionManager>& connectionManager);
    ~ChatServer() noexcept = default;

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator = (const ChatServer&) = delete;

    ChatServer(ChatServer&&) = delete;
    ChatServer& operator = (ChatServer&&) = delete;

    void startServer(const std::string& hostname, const std::string& port);


    void OnDataFromClient(const std::string& clientId, const std::string& data) override;

    void OnClientConnected(const std::string& clientId) override;
    void OnClientDisconnected(const std::string& clientId) override;

    void OnPeerConnected(const std::string &) override;
    void OnPeerDisconnected(const std::string &) override;


private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<spdlog::logger>        logger_;

   std::shared_ptr<ZmqSecuredServerConnection> connection_;
};

#endif

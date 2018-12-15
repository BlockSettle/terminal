#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__


#include <memory>

#include "ServerConnectionListener.h"


namespace spdlog {
   class logger;
}


class ConnectionManager;
class ZmqSecuredServerConnection;
class ApplicationSettings;


class ChatServer : public ServerConnectionListener
{
public:

    ChatServer(const std::shared_ptr<ConnectionManager>& connectionManager
               , const std::shared_ptr<ApplicationSettings> &appSettings
               , const std::shared_ptr<spdlog::logger>& logger);
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

    std::string getPublicKey() const;


private:

    void generateKeys();

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>        logger_;

   std::shared_ptr<ZmqSecuredServerConnection> connection_;

   std::string privateKey_;
   std::string publicKey_;

};

#endif

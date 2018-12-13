#include "ChatServer.h"

#include "ZmqSecuredServerConnection.h"
#include "ConnectionManager.h"

#include <spdlog/spdlog.h>


ChatServer::ChatServer(const std::shared_ptr<ConnectionManager>& connectionManager)
    : connectionManager_(connectionManager)
{
    std::cout << "ChatServer constructed!" << std::endl;
    connection_ = connectionManager_->CreateSecuredServerConnection();
}


void ChatServer::startServer(const std::string& hostname, const std::string& port)
{
    std::cout << "ChatServer starting with host " << hostname << ":" << port << " ..." << std::endl;
    connection_->BindConnection(hostname, port, this);

//    SPDLOG_DEBUG(logger_, "[ChatServer] startServer");
}


void ChatServer::OnDataFromClient(const std::string& clientId, const std::string& data)
{
//    logger_->debug("[ChatServer::OnDataFromClient]");
}


void ChatServer::OnClientConnected(const std::string& clientId)
{
//    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnClientDisconnected(const std::string& clientId)
{
//    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnPeerConnected(const std::string &)
{
//    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnPeerDisconnected(const std::string &)
{
//    logger_->debug("[ChatServer::OnClientConnected]");
}

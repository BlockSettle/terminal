#include "ChatServer.h"

#include "ZmqSecuredServerConnection.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "BinaryData.h"

#include <spdlog/spdlog.h>
#include <zmq.h>

#include <QDebug>


ChatServer::ChatServer(const std::shared_ptr<ConnectionManager>& connectionManager
                       , const std::shared_ptr<ApplicationSettings> &appSettings
                       , const std::shared_ptr<spdlog::logger>& logger)
    : connectionManager_(connectionManager)
    , appSettings_(appSettings)
    , logger_(logger)
{
    connectionManager_ = std::make_shared<ConnectionManager>(logger_);
    connection_ = connectionManager_->CreateSecuredServerConnection();

    generateKeys();

    connection_->SetKeyPair(publicKey_, privateKey_);

    qDebug() << "ChatServer constructed! Public Key: "
             << publicKey_.c_str() << " Private Key:" << privateKey_.c_str();
}


std::string ChatServer::getPublicKey() const
{
    return publicKey_;
}


void ChatServer::generateKeys()
{
    char pubKey[41];
    char prKey[41];

    int result = zmq_curve_keypair(pubKey, prKey);
    if (result != 0) {
       if (logger_) {
          logger_->error("[ZmqSecuredDataConnection::SetServerPublicKey] failed to generate key pair: {}"
             , zmq_strerror(zmq_errno()));
       }
    }

    publicKey_ = std::string(pubKey, 40);
    privateKey_ = std::string(prKey, 40);

//    auto authPrivKey = autheid::generateSecureRandom(40);
//    const std::string sPrivKey(authPrivKey.begin(), authPrivKey.end());
//    privateKey_ = BinaryData(sPrivKey).toHexStr();

//    qDebug() << "Generated private key:" << privateKey_.c_str() << "length=" << privateKey_.size();

//    auto authPubKey = autheid::getPublicKey(authPrivKey);
//    const std::string sPubKey(authPubKey.begin(), authPubKey.end());
//    publicKey_ = BinaryData(sPubKey).toHexStr();

    qDebug() << "Generated public key:" << publicKey_.c_str() << "length=" << publicKey_.size();
}


void ChatServer::OnHeartbeatPing(Chat::HeartbeatPingRequest& request)
{
    logger_->debug("[ChatServer::OnHeartbeatPing] \"{}\"", request.getClientId());

    auto heartbeatResponse = std::make_shared<Chat::HeartbeatPongResponse>();
    connection_->SendDataToClient(request.getClientId(), heartbeatResponse->getData());
}


void ChatServer::OnLogin(Chat::LoginRequest& request)
{
    logger_->debug("[ChatServer::OnLogin] \"{}\"", request.getAuthId());

    clientsOnline_[request.getAuthId()] = true;
}


void ChatServer::OnSendMessage(Chat::SendMessageRequest& request)
{
    logger_->debug("Received message from client: \"{0}\": \"{1}\""
                   , request.getSenderId(), request.getMessageData());

    clientsOnline_[request.getSenderId()] = true;

    auto& history = messages_[request.getReceiverId()];
    history.push_back(request.getMessageData());
}


void ChatServer::OnOnlineUsers(Chat::OnlineUsersRequest& request)
{
    logger_->debug("Received request for online users list from \"{0}\""
                   , request.getAuthId());

    std::vector<std::string> usersList;

    for (auto const& clientOnlinePair : clientsOnline_) {
        if (clientOnlinePair.second) {
            usersList.push_back(clientOnlinePair.first);
        }
    }

    auto usersListResponse = std::make_shared<Chat::UsersListResponse>(usersList);
    connection_->SendDataToClient(request.getClientId(), usersListResponse->getData());
}


void ChatServer::startServer(const std::string& hostname, const std::string& port)
{
    if (!connection_->BindConnection(hostname, port, this))
    {
        logger_->error("Error Binding connection \"{0}\" : \"{1}\" ...", hostname.c_str(), port.c_str());
    }

    logger_->debug("[ChatServer::startServer] Started successfully");
}


void ChatServer::OnDataFromClient(const std::string& clientId, const std::string& data)
{
    auto requestObject = Chat::Request::fromJSON(clientId, data);

    logger_->debug("[ChatServer::OnDataFromClient: \"{0}\"] \"{1}\"", requestObject->getClientId(), data);

    requestObject->handle(*this);
}


void ChatServer::OnClientConnected(const std::string& clientId)
{
    logger_->debug("[ChatServer::OnClientConnected] {}", clientId);
}


void ChatServer::OnClientDisconnected(const std::string& clientId)
{
    logger_->debug("[ChatServer::OnClientConnected] {}", clientId);
}


void ChatServer::OnPeerConnected(const std::string& peerId)
{
    logger_->debug("[ChatServer::OnPeerConnected] {}", peerId);
}


void ChatServer::OnPeerDisconnected(const std::string& peerId)
{
    logger_->debug("[ChatServer::OnPeerDisconnected] {}", peerId);
}

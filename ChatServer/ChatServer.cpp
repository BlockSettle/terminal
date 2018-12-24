#include "ChatServer.h"

#include "ZmqSecuredServerConnection.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "BinaryData.h"

#include <spdlog/spdlog.h>
#include <zmq.h>

#include <jwt/jwt.hpp>

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

    logger_->debug("Chat server started.");

}


std::string ChatServer::getPublicKey() const
{
    return publicKey_;
}


void ChatServer::generateKeys()
{
//    char pubKey[41];
//    char prKey[41];

//    int result = zmq_curve_keypair(pubKey, prKey);
//    if (result != 0) {
//       if (logger_) {
//          logger_->error("[ZmqSecuredDataConnection::SetServerPublicKey] failed to generate key pair: {}"
//             , zmq_strerror(zmq_errno()));
//       }
//    }

//    publicKey_ = std::string(pubKey, 40);
//    privateKey_ = std::string(prKey, 40);

    publicKey_  = "@:2IFYqVXa}+eRpKW9Q310j4cB%%nKe8$-v6bSOg";
    privateKey_ = "uPwwR001@P:Ik!]PxnPw1{hdzJxh7hG5}IUK%oJ6";

    publicKeyAutheID_ = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAv4wt5SHJdlq4ru51yOyo\
RgWqot7fkmd8Ud8+8mApgSl0s5kAf8NndeErR80jv8TtNjSIA6F61BTewaBg1aLT\
sudhCaQZJNKxeQbSLCuabP3WqZa7YFkMpUwLIJV5Cp9cYrxXlnuYyp02RUnAk39n\
UYXoCNJPt1AJPTzn4nz4UGf2qHW9WaUF5PAqe70cOyi3iGzfObRy1RRpDKgTs6SW\
NnHspIxatnxISdffMNyXt7ud/OxtylIGDAN+Jj+FawWRoou5aSKUrBUe1SbdK+D4\
Ktb20AgzmDEkIjgqDVwf8SRnH+Q2qduBRSlV/n2bGx6bYHOMvWy5b9MYkMD+GuLC\
gwIDAQAB";

    qDebug() << "Generated public key:" << publicKey_.c_str() << "length=" << publicKey_.size();
}


void ChatServer::sendResponse(const std::string& clientId, const std::shared_ptr<Chat::Response>& response)
{
    connection_->SendDataToClient(clientId, response->getData());
}


void ChatServer::OnHeartbeatPing(Chat::HeartbeatPingRequest& request)
{
    logger_->debug("[ChatServer::OnHeartbeatPing] \"{}\"", request.getClientId());

    auto heartbeatResponse = std::make_shared<Chat::HeartbeatPongResponse>();
    sendResponse(request.getClientId(), heartbeatResponse);
}


void ChatServer::OnLogin(Chat::LoginRequest& request)
{
    logger_->debug("[ChatServer::OnLogin] \"{}\"", request.getAuthId());

    clientsOnline_[request.getAuthId()] = true;

    logger_->debug("Requested verification of JWT: {}", request.getJWT());

    logger_->debug("Verification key: {}", appSettings_->get<std::string>(ApplicationSettings::authServerPubKey));
}


void ChatServer::OnSendMessage(Chat::SendMessageRequest& request)
{
    auto message = Chat::MessageData::fromJSON(request.getMessageData());

    std::string senderId = message->getSenderId().toStdString();
    std::string receiverId = message->getReceiverId().toStdString();

    logger_->debug("Received message from client: \"{0}\": \"{1}\""
                   , senderId
                   , message->getMessageData().toStdString());

    clientsOnline_[senderId] = true;

    messagesBySender_.insert(std::make_pair(senderId, message));
    messagesByReceiver_.insert(std::make_pair(receiverId, message));
    messages_.push_back(message);
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
    sendResponse(request.getClientId(), usersListResponse);
}


void ChatServer::OnRequestMessages(Chat::MessagesRequest& request)
{
    logger_->debug("Received request for messages chat \"{0}\""
                   , request.getSenderId());

    std::vector<std::string> responseMessages;

    std::set<std::shared_ptr<Chat::MessageData>> messagesFiltered;
    auto insertLambda = [&](MessagesByUserT::value_type& value) {
        auto msg = value.second;
        if ((msg->getSenderId().toStdString() == request.getSenderId()
                && msg->getReceiverId().toStdString() == request.getReceiverId())
                || (msg->getSenderId().toStdString() == request.getReceiverId()
                && msg->getReceiverId().toStdString() == request.getSenderId())) {
            if (messagesFiltered.insert(msg).second) {
                responseMessages.push_back(msg->toJsonString());
            }
        }
    };

    auto searchItBeginEnd = messagesBySender_.equal_range(request.getSenderId());
    std::for_each(searchItBeginEnd.first, searchItBeginEnd.second, insertLambda);

    searchItBeginEnd = messagesBySender_.equal_range(request.getReceiverId());
    std::for_each(searchItBeginEnd.first, searchItBeginEnd.second, insertLambda);

    searchItBeginEnd = messagesByReceiver_.equal_range(request.getReceiverId());
    std::for_each(searchItBeginEnd.first, searchItBeginEnd.second, insertLambda);

    searchItBeginEnd = messagesByReceiver_.equal_range(request.getSenderId());
    std::for_each(searchItBeginEnd.first, searchItBeginEnd.second, insertLambda);

    auto messagesResponse = std::make_shared<Chat::MessagesResponse>(std::move(responseMessages));
    sendResponse(request.getClientId(), messagesResponse);
}


void ChatServer::startServer(const std::string& hostname, const std::string& port)
{
    if (!connection_->BindConnection(hostname, port, this))
    {
        logger_->error("Error Binding connection \"{0}\" : \"{1}\" ...", hostname.c_str(), port.c_str());
        return;
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

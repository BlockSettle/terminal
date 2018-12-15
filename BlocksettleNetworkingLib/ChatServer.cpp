#define SPDLOG_DEBUG_ON

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


void ChatServer::startServer(const std::string& hostname, const std::string& port)
{
    qDebug() << "ChatServer starting with host " << hostname.c_str() << ":" << port.c_str() << " ...";
    if (!connection_->BindConnection(hostname, port, this))
    {
        qDebug() << "Error Binding connection " << hostname.c_str() << ":" << port.c_str() << " ...";
    }

//    SPDLOG_DEBUG(logger_, "[ChatServer] startServer");
}


void ChatServer::OnDataFromClient(const std::string& clientId, const std::string& data)
{
    qDebug() << "[ChatServer]: OnDataFromClient" << clientId.c_str() << data.c_str();
//    logger_->debug("[ChatServer::OnDataFromClient]");
}


void ChatServer::OnClientConnected(const std::string& clientId)
{
    qDebug() << "[ChatServer]: OnClientConnected" << clientId.c_str();
    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnClientDisconnected(const std::string& clientId)
{
    qDebug() << "[ChatServer]: OnClientDisconnected" << clientId.c_str();
    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnPeerConnected(const std::string &)
{
    logger_->debug("[ChatServer::OnClientConnected]");
}


void ChatServer::OnPeerDisconnected(const std::string &)
{
    logger_->debug("[ChatServer::OnClientConnected]");
}

#include "AutheIDClient.h"

#include "ConnectionManager.h"
#include "ZmqSecuredDataConnection.h"

#include "EncryptUtils.h"

#include <spdlog/spdlog.h>
#include "botan/base64.h"


using namespace AutheID::RP;

namespace
{
    const int kConnectTimeoutSeconds = 10;

    // Obtained from http://185.213.153.44:8181/key
    const std::string kApiKey = "Pj+Q9SsZloftMkmE7EhA8v2Bz1ZC9aOmUkAKTBW9hagJ";

}

AutheIDClient::AutheIDClient(const std::shared_ptr<spdlog::logger> &logger
                                      , const std::pair<autheid::PrivateKey, autheid::PublicKey> &authKeys
                                      , QObject *parent)
     : QObject(parent)
     , connectionManager_(new ConnectionManager(logger))
     , logger_(logger)
     , authKeys_(authKeys)
     , timer_(new QTimer(this))
{
     QObject::connect(timer_.get(), &QTimer::timeout, this, &AutheIDClient::timeout);
}


AutheIDClient::~AutheIDClient() noexcept
{
     cancel();
}


bool AutheIDClient::authenticate(const std::string email)
{
     cancel();
     email_ = email;

     CreateRequest request;
     auto signRequest = request.mutable_signature();
     signRequest->set_type(SignatureDataProtobuf);

     request.set_title("Some title!");
     request.set_type(RequestAuthenticate);
     request.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());
     request.set_apikey(kApiKey);
     request.set_userid(email);

     QMetaObject::invokeMethod(timer_.get(), [this] {
         timer_->start(kConnectTimeoutSeconds * 1000);
     });

     logger_->debug("[AutheIDClient::authenticate] Payload sent {}", request.SerializeAsString());
     return sendToAuthServer(request.SerializeAsString(), PayloadCreate);
}


void AutheIDClient::connectToAuthServer(const std::string &serverPubKey
    , const std::string &serverHost, const std::string &serverPort)
{
    connection_ = connectionManager_->CreateSecuredDataConnection();
    if (!connection_) {
        logger_->error("connection_ == nullptr");
        throw std::runtime_error("invalid connection");
    }

    if (!connection_->SetServerPublicKey(serverPubKey)) {
        logger_->error("AutheIDClient::SetServerPublicKey failed");
        throw std::runtime_error("failed to set connection public key");
    }

    if (!connection_->openConnection(serverHost, serverPort, this)) {
        logger_->error("AutheIDClient::openConnection failed");
        throw std::runtime_error("failed to open connection to " + serverHost + ":" + serverPort);
    }
}


void AutheIDClient::processResultReply(const uint8_t *payload, size_t payloadSize)
{
     logger_->debug("AutheIDClient::processResultReply Payload size: {}", payloadSize);

    ResultReply reply;
    if (!reply.ParseFromArray(payload, payloadSize)) {
        logger_->error("Can't decode ResultReply packet");
        emit failed(tr("Invalid result reply"));
        return;
    }

     if (reply.requestid() != requestId_) {
          logger_->debug("AutheIDClient::processResultReply reply.requestid() != requestId_: {0} != {1}", reply.requestid(), requestId_);
          return;
     }

    requestId_.clear();

    std::string jwtToken = reply.authenticate().jwt();
    logger_->debug("JWT: {}", jwtToken);


    auto secureBytesJwt = autheid::decryptData(jwtToken.data(), jwtToken.size(), authKeys_.first);
    auto decryptedJwt = Botan::base64_encode(secureBytesJwt.data(), secureBytesJwt.size());

    logger_->debug("JWT-decoded: {}", secureBytesJwt.size());

//    if (reply.has_signature()) {
//        processSignatureReply(reply.signature());
//        return;
//    }

    if (reply.encsecurereply().empty() || reply.deviceid().empty()) {
        emit failed(tr("Cancelled"));
        return;
    }

    autheid::SecureBytes secureReplyData = autheid::decryptData(reply.encsecurereply(), authKeys_.first);
    if (secureReplyData.empty()) {
        emit failed(tr("Decrypt failed"));
        return;
    }

    SecureReply secureReply;
    if (!secureReply.ParseFromArray(secureReplyData.data(), secureReplyData.size())) {
        emit failed(tr("Invalid secure reply"));
        return;
    }

    //emit succeeded(encKey, SecureBinaryData(deviceKey));
}


void AutheIDClient::processCreateReply(const uint8_t *payload, size_t payloadSize)
{
     logger_->debug("AutheIDClient::processCreateReply Payload size: {}", payloadSize);
    QMetaObject::invokeMethod(timer_.get(), [this] {
        timer_->stop();
    });

    CreateReply reply;
    if (!reply.ParseFromArray(payload, payloadSize)) {
        logger_->error("Can't decode ResultReply packet");
        emit failed(tr("Invalid create reply"));
        return;
    }

    if (!reply.success() || reply.requestid().empty()) {
        logger_->error("Create request failed: {}", reply.errormsg());
        emit failed(tr("Request failed"));
        return;
    }

    requestId_ = reply.requestid();

    ResultRequest request;
    request.set_requestid(requestId_);
    sendToAuthServer(request.SerializeAsString(), PayloadResult);
}


void AutheIDClient::OnDataReceived(const std::string &data)
{
     timer_->stop();
     logger_->debug("[AutheIDClient::OnDataReceived] {}", data);

    ServerPacket packet;
    if (!packet.ParseFromString(data)) {
        logger_->error("Invalid packet data from AuthServer");
        emit failed(tr("Invalid packet"));
        return;
    }

    if (packet.encpayload().empty()) {
        logger_->error("No payload received from AuthServer");
        emit failed(tr("Missing payload"));
        return;
    }

    const auto &decryptedPayload = autheid::decryptData(packet.encpayload(), authKeys_.first);

    logger_->debug("[AutheIDClient::OnDataReceived] type:{0}", packet.type());

    switch (packet.type()) {
    case PayloadCreate:
        QMetaObject::invokeMethod(this, [this, decryptedPayload] {
            processCreateReply(decryptedPayload.data(), decryptedPayload.size());
        });
        break;
    case PayloadResult:
        QMetaObject::invokeMethod(this, [this, decryptedPayload] {
            processResultReply(decryptedPayload.data(), decryptedPayload.size());
        });
        break;
    case PayloadCancel:
        break;
    default:
        logger_->error("Got unknown packet type from AuthServer {}", packet.type());
        emit failed(tr("Unknown packet"));
    }
}


void AutheIDClient::OnConnected()
{
     logger_->debug("AutheIDClient::OnConnected");
}


void AutheIDClient::OnDisconnected()
{
     logger_->debug("AutheIDClient::OnDisconnected");
}


void AutheIDClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
     Q_UNUSED(errorCode)
     logger_->error("AutheIDClient::OnError");
     emit failed(tr("Connection failed"));
}


bool AutheIDClient::sendToAuthServer(const std::string &payload, const AutheID::RP::PayloadType type)
{
    ClientPacket packet;
    packet.set_type(type);
    packet.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());

    const auto signature = autheid::signData(payload, authKeys_.first);

    packet.set_rasign(signature.data(), signature.size());

    packet.set_payload(payload.data(), payload.size());

    return connection_->send(packet.SerializeAsString());
}


void AutheIDClient::cancel()
{
    timer_->stop();

    if (!connection_ || requestId_.empty()) {
        return;
    }

    CancelRequest request;
    request.set_requestid(requestId_);

    sendToAuthServer(request.SerializeAsString(), PayloadCancel);

    connection_->closeConnection();

    requestId_.clear();
}


void AutheIDClient::timeout()
{
    cancel();
    logger_->error("Connection to AuthServer failed, no answer received");
    emit failed(tr("Server offline"));
}

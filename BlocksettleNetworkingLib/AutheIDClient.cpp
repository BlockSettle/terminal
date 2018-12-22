#include "AutheIDClient.h"

#include <spdlog/spdlog.h>
#include <QTimer>

#include "ConnectionManager.h"
#include "MobileUtils.h"
#include "RequestReplyCommand.h"
#include "ZmqSecuredDataConnection.h"
#include "ApplicationSettings.h"

using namespace AutheID::RP;

namespace
{
   const int kConnectTimeoutSeconds = 10;

   const int kKeySize = 32;

   const std::string kApiKey = "Pj+Q9SsZloftMkmE7EhA8v2Bz1ZC9aOmUkAKTBW9hagJ";

   static const char kSeparatorSymbol = ':';

}

AutheIDClient::DeviceInfo AutheIDClient::getDeviceInfo(const std::string &encKey)
{
   DeviceInfo result;

   // encKey could be in form email, email:deviceId and email:deviceId:deviceName formats
   size_t firstSep = encKey.find(kSeparatorSymbol);
   if (firstSep != std::string::npos) {
      size_t secondSep = encKey.find(kSeparatorSymbol, firstSep + 1);
      size_t deviceIdLen = std::string::npos;
      if (secondSep != std::string::npos) {
         deviceIdLen = secondSep - firstSep - 1;
         result.deviceName = encKey.substr(secondSep + 1);
      }
      result.deviceId = encKey.substr(firstSep + 1, deviceIdLen);
   }

   result.userId = encKey.substr(0, firstSep);
   return result;
}

AutheIDClient::AutheIDClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::pair<autheid::PrivateKey, autheid::PublicKey> &authKeys
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , authKeys_(authKeys)
   , resultAuth_(false)
{
   connectionManager_.reset(new ConnectionManager(logger));
   timer_ = new QTimer(this);
   QObject::connect(timer_, &QTimer::timeout, this, &AutheIDClient::timeout);
}

AutheIDClient::~AutheIDClient()
{
   cancel();
}

void AutheIDClient::connect(const std::string &serverPubKey
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

bool AutheIDClient::isConnected() const
{
   return (connection_ && connection_->isActive());
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

bool AutheIDClient::start(RequestType requestType, const std::string &email
   , const std::string &walletId, const std::vector<std::string> &knownDeviceIds)
{
   cancel();

   email_ = email;

   QString action = getAutheIDClientRequestText(requestType);
   bool newDevice = isAutheIDClientNewDeviceNeeded(requestType);
   int timeout = getAutheIDClientTimeout(requestType);

   CreateRequest request;
   request.set_type(RequestDeviceKey);
   request.mutable_devicekey()->set_keyid(walletId);
   request.set_expiration(timeout);
   request.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());

   request.set_title(action.toStdString() + "\nWallet ID:" + walletId);
   request.set_apikey(kApiKey);
   request.set_userid(email_);
   request.mutable_devicekey()->set_usenewdevices(newDevice);

   switch (requestType) {
   case ActivateWallet:
      request.mutable_devicekey()->set_registerkey(RegisterKeyReplace);
      break;
   case DeactivateWallet:
      request.mutable_devicekey()->set_registerkey(RegisterKeyClear);
      break;
   case ActivateWalletNewDevice:
      request.mutable_devicekey()->set_registerkey(RegisterKeyAdd);
      break;
   case DeactivateWalletDevice:
      // No need to update anything, the server will sync it as needed to known device list
      request.mutable_devicekey()->set_registerkey(RegisterKeyKeep);
      break;
   default:
      request.mutable_devicekey()->set_registerkey(RegisterKeyKeep);
      break;
   }

   for (const std::string& knownDeviceId : knownDeviceIds) {
      request.mutable_devicekey()->add_knowndeviceids(knownDeviceId);
   }

   QMetaObject::invokeMethod(timer_, [this] {
      timer_->start(kConnectTimeoutSeconds * 1000);
   });

   return sendToAuthServer(request.SerializeAsString(), PayloadCreate);
}

bool AutheIDClient::authenticate(const std::string& email, const std::shared_ptr<ApplicationSettings> &appSettings)
{
   try {
      if (!isConnected())
      {
         connect(appSettings->get<std::string>(ApplicationSettings::authServerPubKey)
           , appSettings->get<std::string>(ApplicationSettings::authServerHost)
           , appSettings->get<std::string>(ApplicationSettings::authServerPort));
      }
      return requestAuth(email);
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to connect: {}", __func__, e.what());
      emit failed(tr("Failed to connect to Auth eID"));
      return false;
   }
}

bool AutheIDClient::requestAuth(const std::string& email)
{
   cancel();
   email_ = email;
   resultAuth_ = true;

   CreateRequest request;
   auto signRequest = request.mutable_signature();
   signRequest->set_type(SignatureDataProtobuf);

   request.set_title("Terminal Login");
   request.set_type(RequestAuthenticate);
   request.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());
   request.set_apikey(kApiKey);
   request.set_userid(email);

   QMetaObject::invokeMethod(timer_, [this] {
      timer_->start(kConnectTimeoutSeconds * 1000);
   });

   return sendToAuthServer(request.SerializeAsString(), PayloadCreate);
}

bool AutheIDClient::sign(const BinaryData &data, const std::string &email
   , const QString &title, const QString &description, int expiration)
{
   cancel();
   email_ = email;

   CreateRequest request;
   auto signRequest = request.mutable_signature();
   signRequest->set_type(SignatureDataProtobuf);
   signRequest->set_invisibledata(data.toBinStr());

   request.set_type(RequestSignature);
   request.set_expiration(expiration);
   request.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());

   request.set_title(title.toStdString());
   request.set_description(description.toStdString());
   request.set_apikey(kApiKey);
   request.set_userid(email);

   QMetaObject::invokeMethod(timer_, [this] {
      timer_->start(kConnectTimeoutSeconds * 1000);
   });

   return sendToAuthServer(request.SerializeAsString(), PayloadCreate);
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

void AutheIDClient::processCreateReply(const uint8_t *payload, size_t payloadSize)
{
   QMetaObject::invokeMethod(timer_, [this] {
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

void AutheIDClient::processResultReply(const uint8_t *payload, size_t payloadSize)
{
   ResultReply reply;
   if (!reply.ParseFromArray(payload, payloadSize)) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(tr("Invalid result reply"));
      return;
   }

   if (reply.requestid() != requestId_) {
      return;
   }

   requestId_.clear();

   if (reply.has_signature()) {
      processSignatureReply(reply.signature());
      return;
   }

   if (resultAuth_)
   {
       std::string jwtToken = reply.authenticate().jwt();
       if (!jwtToken.empty())
       {
            emit authSuccess(jwtToken);
       }
       else
       {
            emit failed(tr("Not authenticated"));
       }
       return;
   }

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

   const std::string &deviceKey = secureReply.devicekey();

   if (deviceKey.size() != kKeySize) {
      emit failed(tr("Invalid key size"));
      return;
   }

   std::string encKey = email_
      + kSeparatorSymbol + reply.deviceid()
      + kSeparatorSymbol + reply.devicename();

   emit succeeded(encKey, SecureBinaryData(deviceKey));
}

void AutheIDClient::OnDataReceived(const std::string &data)
{
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

void AutheIDClient::timeout()
{
   cancel();
   logger_->error("Connection to AuthServer failed, no answer received");
   emit failed(tr("Server offline"));
}

void AutheIDClient::OnConnected()
{
}

void AutheIDClient::OnDisconnected()
{
}

void AutheIDClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   emit failed(tr("Connection failed"));
}

QString AutheIDClient::getAutheIDClientRequestText(RequestType requestType)
{
   switch (requestType) {
   case ActivateWallet:
      return tr("Activate Auth eID Signing");
   case DeactivateWallet:
      return tr("Deactivate wallet");
   case SignWallet:
      return tr("Sign transaction");
   case BackupWallet:
      return tr("Backup wallet");
   case ActivateWalletOldDevice:
      return tr("Activate wallet (existing device)");
   case ActivateWalletNewDevice:
      return tr("Activate wallet (new device)");
   case DeactivateWalletDevice:
      return tr("Deactivate wallet device");
   case VerifyWalletKey:
      return tr("Confirm Auth eID Signing");
   case ActivateOTP:
      return tr("Activate OTP");
   case SettlementTransaction:
      return tr("Sign transaction");
   default:
      throw std::logic_error("Invalid AutheIDClient::RequestType value");
   }
}

bool AutheIDClient::isAutheIDClientNewDeviceNeeded(RequestType requestType)
{
   switch (requestType) {
   case ActivateWallet:
   case ActivateWalletNewDevice:
   case ActivateOTP:
      return true;
   default:
      return false;
   }
}

int AutheIDClient::getAutheIDClientTimeout(RequestType requestType)
{
   switch (requestType) {
   case SettlementTransaction:
      return 30;
   default:
      return 120;
   }
}

void AutheIDClient::processSignatureReply(const SignatureReply &reply)
{
   if (reply.signaturedata().empty() || reply.sign().empty()) {
      emit failed(tr("Missing mandatory signature data in reply"));
      return;
   }
   if (reply.type() != SignatureDataProtobuf) {
      emit failed(tr("Invalid signature serialization type"));
      return;
   }
   SignatureData sigData;
   if (!sigData.ParseFromString(reply.signaturedata())) {
      emit failed(tr("Failed to parse signature data"));
      return;
   }
   if (!reply.userpubkey().empty()) {
      const auto &pubKey = autheid::publicKeyFromString(reply.userpubkey());
      if (autheid::verifyData(reply.signaturedata(), reply.sign(), pubKey)) {
         emit failed(tr("Signature validation failed"));
         return;
      }
   }
   emit signSuccess(reply.signaturedata(), sigData.invisibledata(), reply.sign());
}

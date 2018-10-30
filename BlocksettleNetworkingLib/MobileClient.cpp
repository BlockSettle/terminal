#include "MobileClient.h"
#include <botan/point_gfp.h>
#include <spdlog/spdlog.h>
#include <QDebug>
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "ZmqSecuredDataConnection.h"

using namespace AutheID::RP;

namespace
{
   const int kTimeoutSeconds = 120;

   const int kKeySize = 32;

   const std::string kServerApiKey = "2526f47d4b3925b2"; // Obtained from http://185.213.153.44:8181/key
   const std::string kPrivateKey = "QHBb3KtxO1nF07cIQ77JYvAG5G6K/GuEgjakNa9y1yg=";
   const std::string kServerPubKey = "ArtQBOQrA2z7oIrCHwqq/yh/0F8rozWTGWvk3RL92fbu";
}

MobileClient::MobileClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::pair<autheid::PrivateKey, autheid::PublicKey> &authKeys
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , authKeys_(authKeys)
   , serverPubKey_(autheid::publicKeyFromString(kServerPubKey))
{
   connectionManager_.reset(new ConnectionManager(logger));

}

std::string MobileClient::toBase64(const std::string &s)
{
   return QByteArray::fromStdString(s).toBase64().toStdString();
}

std::vector<uint8_t> MobileClient::fromBase64(const std::string &s)
{
   const auto &str = QByteArray::fromBase64(QByteArray::fromStdString(s)).toStdString();
   return std::vector<uint8_t>(str.begin(), str.end());
}

void MobileClient::init(const std::string &serverPubKey
   , const std::string &serverHost, const std::string &serverPort)
{
   if (connection_) {
      return;
   }

   connection_ = connectionManager_->CreateSecuredDataConnection();
   connection_->SetServerPublicKey(serverPubKey);
   connection_->openConnection(serverHost, serverPort, this);
}

MobileClient::~MobileClient() = default;

static void PadData(BinaryData &data)
{
   const auto rem = data.getSize() % BTC_AES::BLOCKSIZE;
   if (rem) {
      data.resize(data.getSize() - rem + BTC_AES::BLOCKSIZE);
   }
}

bool MobileClient::sendToAuthServer(const std::string &payload, const AutheID::RP::EnvelopeRequestType type)
{
   RequestEnvelope envelope;
   envelope.set_type(type);
   envelope.set_rapubkey(autheid::publicKeyToString(authKeys_.second));
   envelope.set_userid(email_);
   envelope.set_usertag(tag_);
   envelope.set_apikey(kServerApiKey);

   const auto signature = autheid::signData(payload, authKeys_.first);
   envelope.set_rasign(signature.data(), signature.size());

   const auto encPayload = autheid::encryptData(payload, serverPubKey_);
   envelope.set_payload(encPayload.data(), encPayload.size());

   return connection_->send(envelope.SerializeAsString());
}

bool MobileClient::start(MobileClientRequest requestType
   , const std::string &email, const std::string &walletId)
{
   if (!connection_) {
      return false;
   }

   cancel();

   tag_ = BinaryData::StrToIntLE<uint64_t>(SecureBinaryData().GenerateRandom(sizeof(tag_)));
   email_ = email;
   walletId_ = walletId;

   GetDeviceKeyRequest request;
   request.set_keyid(walletId_);
   request.set_expiration(kTimeoutSeconds);

   QString action = getMobileClientRequestText(requestType);
   bool newDevice = isMobileClientNewDeviceNeeded(requestType);

   request.set_title(action.toStdString() + " " + walletId);
   request.set_usenewdevices(newDevice);

   return sendToAuthServer(request.SerializeAsString(), GetDeviceKeyType);
}

void MobileClient::cancel()
{
   if (!connection_) {
      return;
   }

   if (tag_ == 0) {
      return;
   }

   CancelDeviceKeyRequest request;
   request.set_keyid(walletId_);

   sendToAuthServer(request.SerializeAsString(), CancelDeviceKeyType);

   tag_ = 0;
}

void MobileClient::updateServer(const string &deviceId, const string &walletId, bool isPaired, bool deleteAll)
{
   UpdateDeviceWalletRequest request;
   request.set_deviceid(deviceId);
   request.set_walletid(walletId);
   request.set_ispaired(isPaired);
   request.set_deleteall(deleteAll);

   tag_ = BinaryData::StrToIntLE<uint64_t>(SecureBinaryData().GenerateRandom(sizeof(tag_)));

   sendToAuthServer(request.SerializeAsString(), UpdateDeviceWalletType);
}

// Called from background thread!
void MobileClient::processGetKeyReply(const std::string &payload, uint64_t tag)
{
   if (tag != tag_) {
      logger_->warn("Skip AuthApp response with unknown tag");
      return;
   }

   GetDeviceKeyReply reply;
   if (!reply.ParseFromString(payload)) {
      logger_->error("Can't decode MobileAppGetKeyResponse packet");
      emit failed(tr("Can't decode packet from AuthServer"));
      return;
   }
   if (reply.key().empty() || reply.deviceid().empty()) {
      emit failed(tr("Cancelled"));
      return;
   }

   emit succeeded(reply.deviceid(), reply.key());
}

void MobileClient::processUpdateDeviceWalletReply(const string &payload, uint64_t tag)
{
   logger_->info("Process UpdateDeviceWallet reply");
   UpdateDeviceWalletReply reply;

   if (tag != tag_) {
      logger_->warn("Skip AuthApp response with unknown tag");
      return;
   }

   if (!reply.ParseFromString(payload)) {
      logger_->error("Can't decode UpdateDeviceWalletReply packet");
      emit updateServerFinished(false);
      return;
   }

   emit updateServerFinished(reply.success());
}

void MobileClient::OnDataReceived(const string &data)
{
   ReplyEnvelope envelope;
   if (!envelope.ParseFromString(data)) {
      logger_->error("Invalid packet data from AuthServer");
      emit failed(tr("Invalid packet data from AuthServer"));
      return;
   }

   if (envelope.encpayload().empty()) {
      if (envelope.type() == HeartbeatType) {
         //TODO: handle heartbeat
      }
      else {
         logger_->error("No payload received from AuthServer");
         emit failed(tr("Missing payload from AuthServer"));
         return;
      }
   }

   const auto &decrypted = autheid::decryptData(envelope.encpayload(), authKeys_.first);
   const std::string decPayload(decrypted.begin(), decrypted.end());

   switch (envelope.type()) {
   case GetDeviceKeyType:
      processGetKeyReply(decPayload, envelope.usertag());
      break;
   case UpdateDeviceWalletType:
      processUpdateDeviceWalletReply(decPayload, envelope.usertag());
      break;
   default:
      logger_->warn("Got unknown packet type from AuthServer {}", envelope.type());
      emit failed(tr("Got unknown packet type from AuthServer"));
   }
}

void MobileClient::OnConnected()
{
}

void MobileClient::OnDisconnected()
{
}

void MobileClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   emit failed(tr("Connection to the AuthServer failed"));
}

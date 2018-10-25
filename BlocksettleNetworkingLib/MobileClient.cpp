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
   , const std::pair<Botan::ECDH_PrivateKey, Botan::ECDH_PublicKey> &authKeys
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , authKeys_(authKeys)
   , domain_("secp256k1")
   , eciesParams_(domain_, "KDF2(SHA-256)", "ChaCha(20)", 32, "HMAC(SHA-256)", 20,
      Botan::PointGFp::COMPRESSED, Botan::ECIES_Flags::NONE)
   , serverPubKey_(domain_, Botan::OS2ECP(fromBase64(kServerPubKey), domain_.get_curve()))
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
   const auto &vPubKey = authKeys_.second.public_value();
   envelope.set_publickey(std::string(vPubKey.begin(), vPubKey.end()));
   envelope.set_userid(email_);
   envelope.set_usertag(tag_);
   envelope.set_apikey(kServerApiKey);

   Botan::ECIES_Encryptor encryptor(rng_, eciesParams_);
   encryptor.set_other_key(serverPubKey_.public_point());
   const auto iv = std::vector<uint8_t>(0, 0);
   encryptor.set_initialization_vector(iv);

   Botan::secure_vector<uint8_t> payloadData(payload.begin(), payload.end());
   const auto &encData = encryptor.encrypt(payloadData, rng_);

   if (encData.empty()) {
      logger_->error("failed to encrypt payload");
      emit failed(tr("failed to encrypt payload"));
      return false;
   }
   const std::string encPayload(encData.begin(), encData.end());
//   envelope.set_payload(encPayload);   // toBase64?
   envelope.set_payload(payload);

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

   Botan::ECIES_Decryptor decryptor(authKeys_.first, eciesParams_, rng_);
   const auto iv = std::vector<uint8_t>(0, 0);
   decryptor.set_initialization_vector(iv);

   const BinaryData encData(payload);
   const auto &decrypted = decryptor.decrypt(encData.getPtr(), encData.getSize());
   const std::string decPayload(decrypted.begin(), decrypted.end());

   GetDeviceKeyReply reply;
   if (!reply.ParseFromString(decPayload)) {
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

   if (envelope.payload().empty()) {
      if (envelope.type() == HeartbeatType) {
         //TODO: handle heartbeat
      }
      else {
         logger_->error("No payload received from AuthServer");
         emit failed(tr("Missing payload from AuthServer"));
         return;
      }
   }

   switch (envelope.type()) {
   case GetDeviceKeyType:
      processGetKeyReply(envelope.payload(), envelope.usertag());
      break;
   case UpdateDeviceWalletType:
      processUpdateDeviceWalletReply(envelope.payload(), envelope.usertag());
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

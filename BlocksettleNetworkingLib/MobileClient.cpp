#include "MobileClient.h"

#include <spdlog/spdlog.h>
#include <QDebug>
#include <cryptopp/osrng.h>
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

MobileClient::MobileClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , ownPrivKey_(QByteArray::fromBase64(QByteArray::fromStdString(kPrivateKey)).toStdString())
   , serverPubKey_(CryptoECDSA().UncompressPoint(QByteArray::fromBase64(QByteArray::fromStdString(kServerPubKey)).toStdString()))
{
   connectionManager_.reset(new ConnectionManager(logger));

   CryptoPP::AutoSeededRandomPool rng;

   // NOTE: Don't forget to change AuthApp code!
   CryptoPP::InvertibleRSAFunction parameters;
   parameters.GenerateRandomWithKeySize(rng, 2048);
   privateKey_ = CryptoPP::RSA::PrivateKey(parameters);
   CryptoPP::RSA::PublicKey publicKey = CryptoPP::RSA::PublicKey(parameters);

   CryptoPP::StringSink sink(publicKey_);
   publicKey.Save(sink);
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

bool MobileClient::sendToAuthServer(const std::string &payload
   , const AutheID::RP::EnvelopeRequestType type)
{
   RequestEnvelope envelope;
   envelope.set_type(type);
   envelope.set_publickey(publicKey_);
   envelope.set_userid(email_);
   envelope.set_usertag(tag_);
   envelope.set_apikey(kServerApiKey);

   envelope.set_payload(payload);
   const auto signature = CryptoECDSA().SignData(payload, ownPrivKey_);
   envelope.set_signature(QByteArray::fromStdString(signature.toBinStr()).toBase64().toStdString());

   //TODO: add payload encryption with server's pubKey

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
   GetDeviceKeyReply reply;
   if (!reply.ParseFromString(payload)) {
      logger_->error("Can't decode MobileAppGetKeyResponse packet");
      emit failed(tr("Can't decode packet from AuthServer"));
      return;
   }

   if (tag != tag_) {
      logger_->warn("Skip AuthApp response with unknown tag");
      return;
   }

   if (reply.key().empty() || reply.deviceid().empty()) {
      emit failed(tr("Canceled"));
      return;
   }

   // NOTE: Don't forget to change AuthApp code if algorithm or parameters is changed!

   CryptoPP::SecByteBlock key(kKeySize);
   try {
      CryptoPP::RSAES_PKCS1v15_Decryptor d(privateKey_);
      CryptoPP::AutoSeededRandomPool rng;

      const auto &encryptedKey = reply.key();

      // CryptoPP takes ownership of raw pointers
      auto sink = new CryptoPP::ArraySink(key.begin(), key.size());
      auto filter = new CryptoPP::PK_DecryptorFilter(rng, d, sink);
      CryptoPP::ArraySource(reinterpret_cast<const uint8_t*>(encryptedKey.data())
         , encryptedKey.size(), true, filter);

      if (sink->TotalPutLength() != kKeySize) {
         throw std::runtime_error("Got key with invalid size");
      }

   } catch (const std::exception &e) {
      logger_->error("CryptoPP decrypt error: {}", e.what());
      emit failed(tr("CryptoPP decrypt error"));
      return;
   }

   SecureBinaryData keyCopy(key.data(), key.size());

   emit succeeded(reply.deviceid(), keyCopy);
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
   else {
      const SecureBinaryData signature = QByteArray::fromBase64(QByteArray::fromStdString(envelope.signature())).toStdString();
      if (signature.isNull()) {
         logger_->error("No payload signature from AuthServer");
         emit failed(tr("Missing payload signature from AuthServer"));
         return;
      }
      if (!CryptoECDSA().VerifyData(envelope.payload(), signature, serverPubKey_)) {
         logger_->error("failed to verify server reply");
         emit failed(tr("server signature not verified"));
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

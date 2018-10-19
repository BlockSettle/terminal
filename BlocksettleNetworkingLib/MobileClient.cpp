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
   const int kKeySize = 32;
   const std::string kServerApiKey = "2526f47d4b3925b2"; // Obtained from http://185.213.153.44:8181/key
   const std::string kPrivateKey = "QHBb3KtxO1nF07cIQ77JYvAG5G6K/GuEgjakNa9y1yg=";
   const std::string kServerPubKey = "ArtQBOQrA2z7oIrCHwqq/yh/0F8rozWTGWvk3RL92fbu";
}

MobileClient::MobileClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , ownPrivKey_(fromBase64(kPrivateKey))
   , serverPubKey_(CryptoECDSA().UncompressPoint(fromBase64(kServerPubKey)))
{
   connectionManager_.reset(new ConnectionManager(logger));

   // NOTE: Don't forget to change AuthApp code!
   CryptoPP::InvertibleRSAFunction parameters;
   parameters.GenerateRandomWithKeySize(rng_, 2048);
   privateKey_ = CryptoPP::RSA::PrivateKey(parameters);
   CryptoPP::RSA::PublicKey publicKey = CryptoPP::RSA::PublicKey(parameters);

   CryptoPP::StringSink sink(publicKey_);
   publicKey.Save(sink);
}

std::string MobileClient::toBase64(const std::string &s)
{
   return QByteArray::fromStdString(s).toBase64().toStdString();
}

std::string MobileClient::fromBase64(const std::string &s)
{
   return QByteArray::fromBase64(QByteArray::fromStdString(s)).toStdString();
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

static void PadData(SecureBinaryData &data)
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
   envelope.set_publickey(publicKey_);
   envelope.set_userid(email_);
   envelope.set_usertag(tag_);
   envelope.set_apikey(kServerApiKey);

   const auto signature = CryptoECDSA().SignData(payload, ownPrivKey_);
   envelope.set_signature(QByteArray::fromStdString(signature.toBinStr()).toBase64().toStdString());

   const auto password = SecureBinaryData().GenerateRandom(16);
   const auto parsedPubKey = CryptoECDSA::ParsePublicKey(serverPubKey_);
   CryptoPP::ECIES<CryptoPP::ECP, CryptoPP::SHA256>::Encryptor encryptor(parsedPubKey);
   std::string encPass;
   CryptoPP::StringSource ss1(password.toBinStr(), true
      , new CryptoPP::PK_EncryptorFilter(rng_, encryptor, new CryptoPP::StringSink(encPass)));

   SecureBinaryData iv(BTC_AES::BLOCKSIZE);
   SecureBinaryData data = payload;
   PadData(data);
   const auto encPayload = CryptoAES().EncryptCBC(payload, password, iv);
   if (encPayload.isNull()) {
      logger_->error("failed to encrypt payload");
      emit failed(tr("failed to encrypt payload"));
      return false;
   }

   envelope.set_encryptedpass(toBase64(encPass));
   envelope.set_payload(toBase64(encPayload.toBinStr()));

   return connection_->send(envelope.SerializeAsString());
}

bool MobileClient::start(const std::string &email, const std::string &walletId)
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
   request.set_expiration(60);
   request.set_title("Unlock wallet " + walletId);

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

// Called from background thread!
void MobileClient::processGetKeyResponse(const std::string &payload, uint64_t tag)
{
   GetDeviceKeyReply response;
   if (!response.ParseFromString(payload)) {
      logger_->error("Can't decode MobileAppGetKeyResponse packet");
      emit failed(tr("Can't decode packet from AuthServer"));
      return;
   }

   if (tag != tag_) {
      logger_->warn("Skip AuthApp response with unknown tag");
      return;
   }

   if (response.key().empty() || response.deviceid().empty()) {
      emit failed(tr("Canceled"));
      return;
   }

   // NOTE: Don't forget to change AuthApp code if algorithm or parameters is changed!

   CryptoPP::SecByteBlock key(kKeySize);
   try {
      CryptoPP::RSAES_PKCS1v15_Decryptor d(privateKey_);
      CryptoPP::AutoSeededRandomPool rng;

      const auto &encryptedKey = response.key();

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

   emit succeeded(keyCopy);
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
      processGetKeyResponse(envelope.payload(), envelope.usertag());
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

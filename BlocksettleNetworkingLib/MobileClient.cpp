#include "MobileClient.h"

#include <spdlog/spdlog.h>
#include <QDebug>
#include <cryptopp/osrng.h>
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "ZmqSecuredDataConnection.h"
#include "auth_server.pb.h"

using namespace Blocksettle::AuthServer;

namespace
{

const int kKeySize = 32;

}

MobileClient::MobileClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent)
   , logger_(logger)
{
   connectionManager_.reset(new ConnectionManager(logger));

   CryptoPP::AutoSeededRandomPool rng;

   // NOTE: Don't forgot to change AuthApp code!
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

bool MobileClient::start(const std::string &email, const std::string &walletId)
{
   if (!connection_) {
      return false;
   }

   cancel();

   tag_ = BinaryData::StrToIntLE<uint64_t>(SecureBinaryData().GenerateRandom(sizeof(tag_)));
   email_ = email;
   walletId_ = walletId;

   GetKeyRequest request;
   request.set_tag(tag_);
   request.set_email(email_);
   request.set_publickey(publicKey_);
   request.set_walletid(walletId_);

   Packet packet;
   packet.set_type(PacketType::GetKeyRequestType);
   packet.set_data(request.SerializeAsString());

   connection_->send(packet.SerializeAsString());

   return true;
}

void MobileClient::cancel()
{
   if (!connection_) {
      return;
   }

   if (tag_ == 0) {
      return;
   }

   GetKeyCancelMessage request;
   request.set_tag(tag_);
   request.set_email(email_);
   request.set_walletid(walletId_);

   Packet packet;
   packet.set_type(PacketType::GetKeyCancelMessageType);
   packet.set_data(request.SerializeAsString());

   connection_->send(packet.SerializeAsString());

   tag_ = 0;
}

// Called from background thread!
void MobileClient::processGetKeyResponse(const Packet &packet)
{
   GetKeyResponse response;
   bool result = response.ParseFromString(packet.data());
   if (!result) {
      logger_->error("Can't decode MobileAppGetKeyResponse packet");
      emit failed(tr("Can't decode packet from AuthServer"));
      return;
   }

   if (response.tag() != tag_) {
      logger_->info("Skip AuthApp response with unknown tag");
      return;
   }

   if (response.encryptedkey().empty()) {
      emit failed(tr("Canceled"));
      return;
   }

   // NOTE: Don't forgot to change AuthApp code if algorithm or parameters is changed!

   CryptoPP::SecByteBlock key(kKeySize);
   try {
      CryptoPP::RSAES_PKCS1v15_Decryptor d(privateKey_);
      CryptoPP::AutoSeededRandomPool rng;

      std::string encryptedKey = response.encryptedkey();

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
   Packet packet;
   bool result = packet.ParseFromString(data);
   if (!result) {
      logger_->error("Invalid packet data from AuthServer");
      emit failed(tr("Invalid packet data from AuthServer"));
      return;
   }

   switch (packet.type()) {
   case GetKeyResponseType:
      processGetKeyResponse(packet);
      break;
   default:
      logger_->error("Got unknown packet type from AuthServer {}", packet.type());
      emit failed(tr("Got unknown packet type from AuthServer"));
      return;
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

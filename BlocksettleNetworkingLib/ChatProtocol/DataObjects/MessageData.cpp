#include "MessageData.h"

#include "../ProtocolDefinitions.h"

#include <QDebug>

#include <disable_warnings.h>

#include <botan/hex.h>
#include <botan/ecdh.h>
#include <botan/ecies.h>
#include <botan/ecdsa.h>
#include <botan/aead.h>
#include <botan/auto_rng.h>

#include <enable_warnings.h>

namespace Chat {

   const std::string EC_GROUP = "secp256k1";
   const std::string KDF2 = "KDF2(SHA-256)";
   const size_t NONCE_SIZE = 24;
   const size_t SYMMETRIC_KEY_LEN = 32;
   const std::string AEAD_ALGO = "ChaCha20Poly1305";

   MessageData::MessageData(const QString& senderId, const QString& receiverId, const QString &id, const QDateTime& dateTime, 
      const QString& messageData, int state)
      : DataObject(DataObject::Type::MessageData),
      id_(id),
      senderId_(senderId),
      receiverId_(receiverId),
      dateTime_(dateTime),
      messageData_(messageData), 
      state_(state),
      encryptionType_(EncryptionType::Unencrypted)
   {
   }
   
   QJsonObject MessageData::toJson() const
   {
      QJsonObject data = DataObject::toJson();
   
      data[SenderIdKey] = senderId_;
      data[ReceiverIdKey] = receiverId_;
      data[DateTimeKey] = dateTime_.toMSecsSinceEpoch();
      data[MessageKey] = messageData_;
      data[StatusKey] = state_;
      data[MessageIdKey] = id_;
      data[Nonce] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(nonce_.data()), int(nonce_.size())).toBase64());
      data[EncryptionTypeKey] = static_cast<int>(encryptionType());
      return data;
   }
   
   std::shared_ptr<MessageData> MessageData::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   
      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      QDateTime dtm = QDateTime::fromMSecsSinceEpoch(data[DateTimeKey].toDouble());
      QString messageData = data[MessageKey].toString();
      QString id =  data[MessageIdKey].toString();
      const int state = data[StatusKey].toInt();
      QByteArray local_nonce = QByteArray::fromBase64(data[Nonce].toString().toLocal8Bit());
      Botan::SecureVector<uint8_t> nonce(local_nonce.begin(), local_nonce.end());
   
      std::shared_ptr<MessageData> msg = std::make_shared<MessageData>(senderId, receiverId, id, dtm, messageData, state);
      msg->setNonce(nonce);
      msg->setEncryptionType(static_cast<EncryptionType>(data[EncryptionTypeKey].toInt()));
      return msg;
   }
   
   void MessageData::setFlag(const State state)
   {
      state_ |= (int)state;
   }
   
   void MessageData::unsetFlag(const MessageData::State state)
   {
      state_ &= ~(int)state;
   }

   bool MessageData::testFlag(const MessageData::State stateFlag)
   {
      return state_ & static_cast<int>(stateFlag);
   }

   void MessageData::updateState(const int newState)
   {
      state_ = newState;
   }
   
   bool MessageData::decrypt(const autheid::PrivateKey& privKey)
   {
      if (encryptionType_ != EncryptionType::IES) {
         return false;
      }
      const auto message_bytes = QByteArray::fromBase64(messageData_.toUtf8());
      const auto decryptedData = autheid::decryptData(
         message_bytes.data(), message_bytes.size(), privKey);
      messageData_ = QString::fromUtf8((char*)decryptedData.data(), decryptedData.size());
      encryptionType_ = EncryptionType::Unencrypted;
      return true;
   }
   
   bool MessageData::encrypt(const autheid::PublicKey& pubKey)
   {
      if (encryptionType_ != EncryptionType::Unencrypted) {
         return false;
      }
      const QByteArray message_bytes = messageData_.toUtf8();
      auto data = autheid::encryptData(message_bytes.data(), size_t(message_bytes.size()), pubKey);
      messageData_ = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(data.data()), int(data.size())).toBase64());
      encryptionType_ = EncryptionType::IES;
      return true;
   }

   bool MessageData::encryptAead(const BinaryData& receiverPubKey, const SecureBinaryData& ownPrivKey, const Botan::SecureVector<uint8_t> &nonce, const std::shared_ptr<spdlog::logger>& logger)
   {
      if (encryptionType_ != EncryptionType::Unencrypted)
      {
         return false;
      }

      // Botan variant of public key
      Botan::EC_Group kDomain(EC_GROUP);
      Botan::PointGFp receiverPublicKeyValue = kDomain.OS2ECP(receiverPubKey.getPtr(), receiverPubKey.getSize());
      Botan::ECDH_PublicKey receiverPublicKeyDecoded(kDomain, receiverPublicKeyValue);

      Botan::AutoSeeded_RNG rng;

      // Botan variant of private key
      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(ownPrivKey.getPtr(), ownPrivKey.getSize());
      Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
      privateKeyValue.clear();

      // Generate symmetric key from public and private key
      Botan::PK_Key_Agreement key_agreement(privateKeyDecoded, rng, KDF2);
      Botan::SymmetricKey symmetricKey = key_agreement.derive_key(SYMMETRIC_KEY_LEN, receiverPublicKeyDecoded.public_value()).bits_of();

      std::unique_ptr<Botan::AEAD_Mode> encryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::ENCRYPTION);

      try
      {
         encryptor->set_key(symmetricKey);
      }
      catch (Botan::Exception& e)
      {
         logger->error("[MessageData::{}] Invalid symmetric key {}", __func__, e.what());
         return false;
      }

      nonce_ = nonce;

      try {
         encryptor->start(nonce_);
      }
      catch (Botan::Exception &e) {
         logger->error("[MessageData::{}] Invalid nonce {}", __func__, e.what());
         return false;
      }

      const QByteArray message_bytes = messageData_.toUtf8();
      Botan::SecureVector<uint8_t> encrypted_data(message_bytes.begin(), message_bytes.end());

      try
      {
         encryptor->finish(encrypted_data);
      }
      catch (Botan::Exception &e)
      {
         logger->debug("[MessageData::{}] Encryption message failed {}", __func__, e.what());
         return false;
      }

      messageData_ = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encrypted_data.data()), int(encrypted_data.size())).toBase64());
      encryptionType_ = EncryptionType::AEAD;

      return true;
   }

   bool MessageData::decryptAead(const BinaryData& senderPubKey, const SecureBinaryData& ownPrivKey, const std::shared_ptr<spdlog::logger>& logger)
   {
      if (encryptionType_ != EncryptionType::AEAD)
      {
         return false;
      }

      Botan::EC_Group kDomain(EC_GROUP);
      Botan::PointGFp senderPublicKeyValue = kDomain.OS2ECP(senderPubKey.getPtr(), senderPubKey.getSize());
      Botan::ECDH_PublicKey senderPublicKeyDecoded(kDomain, senderPublicKeyValue);

      Botan::AutoSeeded_RNG rng;

      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(ownPrivKey.getPtr(), ownPrivKey.getSize());
      Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
      privateKeyValue.clear();

      Botan::PK_Key_Agreement key_agreement(privateKeyDecoded, rng, KDF2);
      Botan::SymmetricKey symmetricKey = key_agreement.derive_key(SYMMETRIC_KEY_LEN, senderPublicKeyDecoded.public_value()).bits_of();

      std::unique_ptr<Botan::AEAD_Mode> decryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::DECRYPTION);

      try
      {
         decryptor->set_key(symmetricKey);
      }
      catch (Botan::Exception& e)
      {
         logger->error("[MessageData::{}] Invalid symmetric key {}", __func__, e.what());
         return false;
      }

      try {
         decryptor->start(nonce_);
      }
      catch (Botan::Exception &e) {
         logger->error("[MessageData::{}] Invalid nonce {}", __func__, e.what());
         return false;
      }

      const QByteArray message_bytes = QByteArray::fromBase64(messageData_.toLatin1());
      Botan::SecureVector<uint8_t> decrypted_data(message_bytes.begin(), message_bytes.end());

      if (decrypted_data.size() < decryptor->minimum_final_size())
      {
         logger->error("[MessageData::{}] Decryption data size ({}) is less than the anticipated size ({})", __func__, decrypted_data.size(), decryptor->minimum_final_size());
         return false;
      }

      try {
         decryptor->finish(decrypted_data);
      }
      catch (Botan::Exception&e) {
         logger->debug("[MessageData::{}] Decryption message failed {}", __func__, e.what());
         return false;
      }

      messageData_ = QString::fromUtf8((char*)decrypted_data.data(), (int)decrypted_data.size());
      encryptionType_ = EncryptionType::Unencrypted;

      return true;
   }
   
   QString MessageData::setId(const QString& id)
   {
      QString oldId = id_;
      id_ = id;
      return oldId;
   }

   void MessageData::setNonce(const Botan::SecureVector<uint8_t> &nonce)
   {
      nonce_ = nonce;
   }

   Botan::SecureVector<uint8_t> MessageData::getNonce() const
   {
      return nonce_;
   }

   size_t MessageData::getDefaultNonceSize() const
   {
      return NONCE_SIZE;
   }

   MessageData::EncryptionType MessageData::encryptionType() const
   {
      return encryptionType_;
   }

   void MessageData::setEncryptionType(const MessageData::EncryptionType &type)
   {
      encryptionType_ = type;
   }
}

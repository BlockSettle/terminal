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

   const Botan::EC_Group kDomain("secp256k1");
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
      state_(state)
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
      return data;
   }
   
//   std::string MessageData::toJsonString() const
//   {
//      return serializeData(this);
//   }
   
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
      autheid::SecureBytes nonce(local_nonce.begin(), local_nonce.end());
   
      return std::make_shared<MessageData>(senderId, receiverId, id, dtm, messageData, state);
   }
   
   void MessageData::setFlag(const State state)
   {
      state_ |= (int)state;
   }
   
   void MessageData::unsetFlag(const MessageData::State state)
   {
      state_ &= ~(int)state;
   }

   void MessageData::updateState(const int newState)
   {
      int mask = ~static_cast<int>(Chat::MessageData::State::Encrypted);
      int set = mask & newState;
      int unset = ~(set ^ mask);
      state_ = (state_ & unset) | set;
   }
   
   bool MessageData::decrypt(const autheid::PrivateKey& privKey)
   {
      if (!(state_ & (int)State::Encrypted)) {
         return false;
      }
      const auto message_bytes = QByteArray::fromBase64(messageData_.toLocal8Bit());
      const auto decryptedData = autheid::decryptData(
         message_bytes.data(), message_bytes.size(), privKey);
      messageData_ = QString::fromLocal8Bit((char*)decryptedData.data(), decryptedData.size());
      state_ &= ~(int)State::Encrypted;
      return true;
   }
   
   bool MessageData::encrypt(const autheid::PublicKey& pubKey)
   {
      if (state_ & (int)State::Encrypted) {
         return false;
      }
      const QByteArray message_bytes = messageData_.toLocal8Bit();
      auto data = autheid::encryptData(message_bytes.data(), size_t(message_bytes.size()), pubKey);
      messageData_ = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(data.data()), int(data.size())).toBase64());
      state_ |= (int)State::Encrypted;
      return true;
   }

   bool MessageData::encrypt_aead(const autheid::PublicKey& receiverPubKey, const autheid::PrivateKey& ownPrivKey)
   {
      if ((state_ & (int)State::Encrypted_AEAD))
      {
         return false;
      }

      auto receiverPublicKeyValue = Botan::OS2ECP(receiverPubKey.data(), receiverPubKey.size(), kDomain.get_curve());
      Botan::ECDH_PublicKey receiverPublicKeyDecoded(kDomain, receiverPublicKeyValue);

      Botan::AutoSeeded_RNG rng;

      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(ownPrivKey);
      Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
      privateKeyValue.clear();

      Botan::PK_Key_Agreement key_agreement(privateKeyDecoded, rng, KDF2);
      Botan::SymmetricKey symmetricKey = key_agreement.derive_key(SYMMETRIC_KEY_LEN, receiverPublicKeyDecoded.public_value()).bits_of();

      std::unique_ptr<Botan::AEAD_Mode> encryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::ENCRYPTION);
      //qDebug() << "Enc Algo name:" << enc->name().c_str();
      //std::vector<std::string> providers = enc->providers("ChaCha20Poly1305");

      encryptor->set_key(symmetricKey);
      //qDebug() << "Symmetric key:" << QString::fromStdString(Botan::hex_encode(symmetricKey.bits_of()));

      nonce_ = rng.random_vec(NONCE_SIZE);
      //qDebug() << "Nonce:" << QString::fromStdString(Botan::hex_encode(nonce));
      encryptor->start(nonce_);

      const QByteArray message_bytes = messageData_.toLocal8Bit();
      autheid::SecureBytes encrypted_data(message_bytes.begin(), message_bytes.end());

      encryptor->finish(encrypted_data);

      messageData_ = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encrypted_data.data()), int(encrypted_data.size())).toBase64());
      state_ |= (int)State::Encrypted_AEAD;
      //const std::string test(buffer_to_encrypt.begin(), buffer_to_encrypt.end());
      //qDebug() << "test:" << test.c_str() << QString::fromStdString(BinaryData(test).toHexStr());

      //qDebug() << "ENC buf:" << QString::fromStdString(Botan::hex_encode(buf)) << QString::fromLocal8Bit((char*)buf.data(), buf.size());
      
      return true;
   }

   bool MessageData::decrypt_aead(const autheid::PublicKey& senderPubKey, const autheid::PrivateKey& ownPrivKey)
   {
      if (!(state_ & (int)State::Encrypted_AEAD))
      {
         return false;
      }

      auto senderPublicKeyValue = Botan::OS2ECP(senderPubKey.data(), senderPubKey.size(), kDomain.get_curve());
      Botan::ECDH_PublicKey senderPublicKeyDecoded(kDomain, senderPublicKeyValue);

      Botan::AutoSeeded_RNG rng;

      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(ownPrivKey);
      Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
      privateKeyValue.clear();

      Botan::PK_Key_Agreement key_agreement(privateKeyDecoded, rng, KDF2);
      Botan::SymmetricKey symmetricKey = key_agreement.derive_key(SYMMETRIC_KEY_LEN, senderPublicKeyDecoded.public_value()).bits_of();


      std::unique_ptr<Botan::AEAD_Mode> decryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::DECRYPTION);
      //qDebug() << "Dec Algo name:" << dec->name().c_str();

      const QByteArray message_bytes = messageData_.toLocal8Bit();
      autheid::SecureBytes decrypted_data(message_bytes.begin(), message_bytes.end());

      if (decrypted_data.size() < decryptor->minimum_final_size())
      {
         // TODO
         // wrong input size - return error
      }

      decryptor->set_key(symmetricKey);
      decryptor->start(nonce_);

      decryptor->finish(decrypted_data);

      const std::string test2(decrypted_data.begin(), decrypted_data.end());
      qDebug() << "test:" << test2.c_str() << QString::fromStdString(BinaryData(test2).toHexStr());

      qDebug() << "DEC:" << QString::fromLocal8Bit((char*)decrypted_data.data(), decrypted_data.size()) << Botan::hex_encode(decrypted_data).c_str();

      messageData_ = QString::fromLocal8Bit((char*)decrypted_data.data(), decrypted_data.size());
      state_ &= ~(int)State::Encrypted_AEAD;

      return true;
   }
   
   QString MessageData::setId(const QString& id)
   {
      QString oldId = id_;
      id_ = id;
      return oldId;
   }

   void MessageData::setNonce(const autheid::SecureBytes &nonce)
   {
      nonce_ = nonce;
   }
}

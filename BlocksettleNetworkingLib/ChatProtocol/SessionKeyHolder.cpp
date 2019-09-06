#include "ChatProtocol/SessionKeyHolder.h"

#include <disable_warnings.h>
#include <botan/auto_rng.h>
#include <botan/bigint.h>
#include <botan/ecdh.h>

#include <spdlog/spdlog.h>
#include <enable_warnings.h>

#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"

constexpr size_t kPrivateKeySize = 32;
const Botan::EC_Group kDomain("secp256k1");

using namespace Chat;

SessionKeyHolder::SessionKeyHolder(const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */) : loggerPtr_(loggerPtr), QObject(parent)
{
   connect(this, &SessionKeyHolder::error, this, &SessionKeyHolder::handleLocalErrors);
}

void SessionKeyHolder::requestSessionKeysForUser(const std::string& userName, const BinaryData& remotePublicKey)
{
   if (remotePublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::PublicKeyEmpty, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   SessionKeyDataPtr sessionKeyDataPtr = sessionKeyDataForUser(userName);

   if (sessionKeyDataPtr->isInitialized())
   {
      // key found
      emit sessionKeysForUser(sessionKeyDataPtr);
      return;
   }

   // not found, request new exchange
   BinaryData encodedPublicKey = iesEncryptLocalSessionPublicKey(sessionKeyDataPtr, remotePublicKey);

   if (encodedPublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::IesEncoding, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   emit requestSessionKeyExchange(userName, encodedPublicKey);
}

BinaryData SessionKeyHolder::iesEncryptLocalSessionPublicKey(const Chat::SessionKeyDataPtr& sessionKeyDataPtr, const BinaryData& remotePublicKey) const
{
   auto enc = Encryption::IES_Encryption::create(loggerPtr_);
   enc->setPublicKey(remotePublicKey);
   enc->setData(sessionKeyDataPtr->localSessionPublicKey().toHexStr());

   try {
      Botan::SecureVector<uint8_t> encodedData;
      enc->finish(encodedData);

      BinaryData encodedResult(encodedData.data(), encodedData.size());
      return encodedResult;
   }
   catch (std::exception& e) {
      loggerPtr_->error("[SessionKeyHolder::iesEncryptLocalSessionPublicKey] Failed to encrypt msg by ies {}", __func__, e.what());
      return BinaryData();
   }
}

SessionKeyDataPtr SessionKeyHolder::sessionKeyDataForUser(const std::string& userName)
{
   const auto it = sessionKeyDataList_.find(userName);
   if (it == sessionKeyDataList_.end())
   {
      // not found, create new one
      SessionKeyDataPtr sessionKeyDataPtr = std::make_shared<SessionKeyData>(userName);
      generateLocalKeys(sessionKeyDataPtr);
      sessionKeyDataList_.emplace(userName, sessionKeyDataPtr);
      return sessionKeyDataPtr;
   }

   return (it->second);
}

void SessionKeyHolder::generateLocalKeys(const SessionKeyDataPtr& sessionKeyDataPtr)
{
   Botan::AutoSeeded_RNG rnd;
   Botan::SecureVector<uint8_t> privateKey = rnd.random_vec(kPrivateKeySize);

   Botan::BigInt privateKeyValue;
   privateKeyValue.binary_decode(privateKey);
   Botan::ECDH_PrivateKey privateKeyEC(rnd, kDomain, privateKeyValue);
   privateKeyValue.clear();

   std::vector<uint8_t> publicKey = privateKeyEC.public_point().encode(Botan::PointGFp::COMPRESSED);

   SecureBinaryData localPrivateKey(privateKey.data(), privateKey.size());
   BinaryData localPublicKey(publicKey.data(), publicKey.size());

   sessionKeyDataPtr->setLocalSessionPrivateKey(localPrivateKey);
   sessionKeyDataPtr->setLocalSessionPublicKey(localPublicKey);
}

void SessionKeyHolder::handleLocalErrors(const Chat::SessionKeyHolderError& errorCode, const std::string& what)
{
   loggerPtr_->debug("[SessionKeyHolder::handleLocalErrors] Error: {}, what: {}", static_cast<int>(errorCode), what);
}

BinaryData SessionKeyHolder::iesDecryptData(const BinaryData& encodedData, const SecureBinaryData& privateKey)
{
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(loggerPtr_);
   dec->setPrivateKey(privateKey);
   dec->setData(encodedData.toBinStr());

   Botan::SecureVector<uint8_t> decodedData;
   try {
      dec->finish(decodedData);
   }
   catch (const std::exception& e) {
      emit error(SessionKeyHolderError::IesDecodingSessionKeyRequest, e.what());
      return BinaryData();
   }

   BinaryData decodedBinaryData = BinaryData::CreateFromHex(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()).toStdString());

   return decodedBinaryData;
}

void SessionKeyHolder::onIncomingRequestSessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey, const SecureBinaryData& ownPrivateKey)
{
   SessionKeyDataPtr sessionKeyDataPtr = sessionKeyDataForUser(userName);
   sessionKeyDataPtr->setInitialized(false);

   // decrypt by ies received public key
   BinaryData sessionRemotePublicKey = iesDecryptData(incomingEncodedPublicKey, ownPrivateKey);

   if (sessionRemotePublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::IesDecoding, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   sessionKeyDataPtr->setSessionRemotePublicKey(sessionRemotePublicKey);

   // reply session key exchange
   BinaryData encodedPublicKey = iesEncryptLocalSessionPublicKey(sessionKeyDataPtr, sessionRemotePublicKey);

   if (encodedPublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::IesEncoding, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   emit replySessionKeyExchange(userName, encodedPublicKey);
}

void SessionKeyHolder::onIncomingReplySessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey)
{
   SessionKeyDataPtr sessionKeyDataPtr = sessionKeyDataForUser(userName);
   sessionKeyDataPtr->setInitialized(false);

   // decrypt by ies received public key
   BinaryData sessionRemotePublicKey = iesDecryptData(incomingEncodedPublicKey, sessionKeyDataPtr->localSessionPrivateKey());

   if (sessionRemotePublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::IesDecoding, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   sessionKeyDataPtr->setSessionRemotePublicKey(sessionRemotePublicKey);
   sessionKeyDataPtr->setInitialized(true);

   // session initialized
   emit sessionKeysForUser(sessionKeyDataPtr);
}

void SessionKeyHolder::clearSessionForUser(const std::string& userName)
{
   sessionKeyDataList_.erase(userName);
}

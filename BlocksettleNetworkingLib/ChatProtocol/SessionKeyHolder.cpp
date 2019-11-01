#include "ChatProtocol/SessionKeyHolder.h"

#include <disable_warnings.h>
#include <botan/auto_rng.h>
#include <botan/bigint.h>
#include <botan/ecdh.h>

#include <spdlog/spdlog.h>
#include <enable_warnings.h>
#include <utility>

#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"

constexpr size_t kPrivateKeySize = 32;
const Botan::EC_Group kDomain("secp256k1");

using namespace Chat;

SessionKeyHolder::SessionKeyHolder(LoggerPtr loggerPtr, QObject* parent /* = nullptr */) 
: QObject(parent), loggerPtr_(std::move(loggerPtr))
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

   const auto sessionKeyDataPtr = sessionKeyDataForUser(userName);

   if (sessionKeyDataPtr->isInitialized())
   {
      // key found
      emit sessionKeysForUser(sessionKeyDataPtr);
      return;
   }

   // not found, request new exchange
   const auto encodedPublicKey = iesEncryptLocalSessionPublicKey(sessionKeyDataPtr, remotePublicKey);

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
      auto sessionKeyDataPtr = std::make_shared<SessionKeyData>(userName);
      generateLocalKeys(sessionKeyDataPtr);
      sessionKeyDataList_.emplace(userName, sessionKeyDataPtr);
      return sessionKeyDataPtr;
   }

   return (it->second);
}

void SessionKeyHolder::generateLocalKeys(const SessionKeyDataPtr& sessionKeyDataPtr)
{
   Botan::AutoSeeded_RNG rnd;
   auto privateKey = rnd.random_vec(kPrivateKeySize);

   Botan::BigInt privateKeyValue;
   privateKeyValue.binary_decode(privateKey);
   const Botan::ECDH_PrivateKey privateKeyEC(rnd, kDomain, privateKeyValue);
   privateKeyValue.clear();

   auto publicKey = privateKeyEC.public_point().encode(Botan::PointGFp::COMPRESSED);

   const SecureBinaryData localPrivateKey(privateKey.data(), privateKey.size());
   const BinaryData localPublicKey(publicKey.data(), publicKey.size());

   sessionKeyDataPtr->setLocalSessionPrivateKey(localPrivateKey);
   sessionKeyDataPtr->setLocalSessionPublicKey(localPublicKey);
}

void SessionKeyHolder::handleLocalErrors(const Chat::SessionKeyHolderError& errorCode, const std::string& what) const
{
   loggerPtr_->debug("[SessionKeyHolder::handleLocalErrors] Error: {}, what: {}", static_cast<int>(errorCode), what);
}

BinaryData SessionKeyHolder::iesDecryptData(const BinaryData& encodedData, const SecureBinaryData& privateKey)
{
   auto dec = Encryption::IES_Decryption::create(loggerPtr_);
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

   auto decodedBinaryData = BinaryData::CreateFromHex(QString::fromUtf8(reinterpret_cast<char*>(decodedData.data()), static_cast<int>(decodedData.size())).toStdString());

   return decodedBinaryData;
}

void SessionKeyHolder::onIncomingRequestSessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey, const SecureBinaryData& ownPrivateKey)
{
   auto sessionKeyDataPtr = sessionKeyDataForUser(userName);
   sessionKeyDataPtr->setInitialized(false);

   // decrypt by ies received public key
   const auto sessionRemotePublicKey = iesDecryptData(incomingEncodedPublicKey, ownPrivateKey);

   if (sessionRemotePublicKey.getSize() == 0)
   {
      emit error(SessionKeyHolderError::IesDecoding, userName);
      emit sessionKeysForUserFailed(userName);
      return;
   }

   sessionKeyDataPtr->setSessionRemotePublicKey(sessionRemotePublicKey);

   // reply session key exchange
   const auto encodedPublicKey = iesEncryptLocalSessionPublicKey(sessionKeyDataPtr, sessionRemotePublicKey);

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
   auto sessionKeyDataPtr = sessionKeyDataForUser(userName);
   sessionKeyDataPtr->setInitialized(false);

   // decrypt by ies received public key
   const auto sessionRemotePublicKey = iesDecryptData(incomingEncodedPublicKey, sessionKeyDataPtr->localSessionPrivateKey());

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

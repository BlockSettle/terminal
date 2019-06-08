#include "ChatSessionKey.h"

#include <algorithm>

#include <disable_warnings.h>
#include <botan/auto_rng.h>
#include <botan/bigint.h>
#include <botan/ecdh.h>

#include <spdlog/spdlog.h>
#include <enable_warnings.h>

#include <Encryption/IES_Encryption.h>

namespace Chat {

   constexpr size_t kPrivateKeySize = 32;
   const Botan::EC_Group kDomain("secp256k1");
   
   ChatSessionKey::ChatSessionKey(const std::shared_ptr<spdlog::logger>& logger) : logger_(logger)
   {}

   ChatSessionKeyDataPtr ChatSessionKey::findSessionForUser(const std::string &receiverId) const
   {
      ChatSessionDataPtrList::const_iterator chatSessionKeyDataIt =
         std::find_if(_chatSessionKeyDataList.begin(), _chatSessionKeyDataList.end(), [receiverId](const ChatSessionKeyDataPtr& chatSessionDataPtr)
            {
               return chatSessionDataPtr->receiverId() == receiverId;
            });

      if (chatSessionKeyDataIt == _chatSessionKeyDataList.end()) {
         return nullptr;
      }

      return (*chatSessionKeyDataIt);
   }

   ChatSessionKeyDataPtr ChatSessionKey::generateLocalKeysForUser(const std::string &receiverId)
   {
      ChatSessionKeyDataPtr chatSessionKeyDataPtr = findSessionForUser(receiverId);
      if (chatSessionKeyDataPtr != nullptr) {
         // already generated
         return chatSessionKeyDataPtr;
      }

      Botan::AutoSeeded_RNG rnd;
      Botan::SecureVector<uint8_t> privateKey = rnd.random_vec(kPrivateKeySize);

      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(privateKey);
      Botan::ECDH_PrivateKey privateKeyEC(rnd, kDomain, privateKeyValue);
      privateKeyValue.clear();

      std::vector<uint8_t> publicKey = privateKeyEC.public_point().encode(Botan::PointGFp::COMPRESSED);

      SecureBinaryData localPrivateKey(privateKey.data(), privateKey.size());
      BinaryData localPublicKey(publicKey.data(), publicKey.size());

      chatSessionKeyDataPtr = std::make_shared<Chat::ChatSessionKeyData>();
      chatSessionKeyDataPtr->setLocalPrivateKey(localPrivateKey);
      chatSessionKeyDataPtr->setLocalPublicKey(localPublicKey);
      chatSessionKeyDataPtr->setReceiverId(receiverId);

      _chatSessionKeyDataList.emplace_back(chatSessionKeyDataPtr);

      return chatSessionKeyDataPtr;
   }

   bool ChatSessionKey::updateRemotePublicKeyForUser(const std::string &receiverId, const BinaryData &remotePublicKey) const
   {
      auto chatSessionDataPtr = findSessionForUser(receiverId);

      if (chatSessionDataPtr == nullptr) {
         return false;
      }

      chatSessionDataPtr->setRemotePublicKey(remotePublicKey);

      return true;
   }

   BinaryData ChatSessionKey::iesEncryptLocalPublicKey(const std::string& receiverId, const BinaryData& remotePublicKey) const
   {
      auto chatSessionDataPtr = findSessionForUser(receiverId);

      if (chatSessionDataPtr == nullptr) {
         throw std::runtime_error("ChatSessionData for give user is empty!");
      }

      std::unique_ptr<Encryption::IES_Encryption> enc = Encryption::IES_Encryption::create(logger_);
      enc->setPublicKey(remotePublicKey);
      enc->setData(chatSessionDataPtr->localPublicKey().toHexStr());

      try {
         Botan::SecureVector<uint8_t> encodedData;
         enc->finish(encodedData);

         BinaryData encodedResult(encodedData.data(), encodedData.size());
         return encodedResult;
      }
      catch (std::exception& e) {
         logger_->error("[ChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());
         return BinaryData();
      }
   }

   bool ChatSessionKey::isExchangeForUserSucceeded(const std::string& receiverId)
   {
      auto chatSessionDataPtr = findSessionForUser(receiverId);

      if (chatSessionDataPtr == nullptr) {
         return false;
      }

      if (chatSessionDataPtr->remotePublicKey().getSize() == 0) {
         return false;
      }

      return true;
   }

   void ChatSessionKey::clearSessionForUser(const std::string& receiverId)
   {

      auto chatSessionDataPtrIterator = std::remove_if(_chatSessionKeyDataList.begin(), _chatSessionKeyDataList.end(), [receiverId](const ChatSessionKeyDataPtr& chatSessionDataPtr)
      {
         return chatSessionDataPtr->receiverId().compare(receiverId) == 0;
      });

      if (chatSessionDataPtrIterator == _chatSessionKeyDataList.end()) {
         return;
      }

      _chatSessionKeyDataList.erase(chatSessionDataPtrIterator);
   }

   void ChatSessionKey::clearAll()
   {
      _chatSessionKeyDataList.clear();
   }

}

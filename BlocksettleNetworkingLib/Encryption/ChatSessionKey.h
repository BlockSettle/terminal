#ifndef ChatSessionKey_h__
#define ChatSessionKey_h__

#include "ChatSessionKeyData.h"

#include <atomic>
#include <unordered_map>
#include <memory>

namespace spdlog
{
   class logger;
}

namespace Chat {
   class ChatSessionKey
   {
   public:
      explicit ChatSessionKey(const std::shared_ptr<spdlog::logger> &logger);

      ChatSessionKeyDataPtr findSessionForUser(const std::string &receiverId) const;
      ChatSessionKeyDataPtr generateLocalKeysForUser(const std::string &receiverId);
      bool updateRemotePublicKeyForUser(const std::string &receiverId, const BinaryData &remotePublicKey) const;
      BinaryData iesEncryptLocalPublicKey(const std::string& receiverId, const BinaryData& remotePublicKey) const;

      bool isExchangeForUserSucceeded(const std::string &receiverId);
      void clearSessionForUser(const std::string& receiverId);
      void clearAll();

   private:
      mutable std::atomic_flag                                 lock_ = ATOMIC_FLAG_INIT;
      std::unordered_map<std::string, ChatSessionKeyDataPtr>   chatSessionKeyDataList_;
      std::shared_ptr<spdlog::logger> logger_ = nullptr;
   };

   using ChatSessionKeyPtr = std::shared_ptr<ChatSessionKey>;

};

#endif // ChatSessionKey_h__

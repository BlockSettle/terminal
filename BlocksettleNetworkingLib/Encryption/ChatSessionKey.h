#ifndef ChatSessionKey_h__
#define ChatSessionKey_h__

#include "ChatSessionKeyData.h"

#include <list>
#include <memory>

namespace spdlog
{
   class logger;
}

namespace Chat {

   typedef std::list<ChatSessionKeyDataPtr> ChatSessionDataPtrList;

   class ChatSessionKey
   {
   public:
      ChatSessionKey(const std::shared_ptr<spdlog::logger> &logger);
      ChatSessionKeyDataPtr findSessionForUser(const std::string &receiverId) const;
      ChatSessionKeyDataPtr generateLocalKeysForUser(const std::string &receiverId);
      bool updateRemotePublicKeyForUser(const std::string &receiverId, const BinaryData &remotePublicKey) const;
      BinaryData iesEncryptLocalPublicKey(const std::string& receiverId, const BinaryData& remotePublicKey) const;

      bool isExchangeForUserSucceeded(const std::string &receiverId);
      void clearSessionForUser(const std::string& receiverId);
      void clearAll();

   private:
      ChatSessionDataPtrList _chatSessionKeyDataList;
      std::shared_ptr<spdlog::logger> logger_ = nullptr;
   };

   typedef std::shared_ptr<ChatSessionKey> ChatSessionKeyPtr;

};

#endif // ChatSessionKey_h__

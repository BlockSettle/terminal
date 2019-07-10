#include "ContactPublicKey.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <enable_warnings.h>

namespace Chat {

   ContactPublicKey::ContactPublicKey(const std::shared_ptr<spdlog::logger>& logger) {
      logger_ = logger;
   }

   void ContactPublicKey::loadKeys(const std::map<std::string, BinaryData>& newKeys)
   {
      contactPublicKeys_.clear();

      for (auto userKey : newKeys) {
         if (userKey.first.empty() || userKey.second.isNull()) {
            logger_->warn("[ContactPublicKey::{}] cannot set key for user {} or user name is empty.", __func__, userKey.first);
            continue;
         }

         contactPublicKeys_.emplace_hint(contactPublicKeys_.end(), userKey.first, userKey.second);
      }
   }

   bool ContactPublicKey::findPublicKeyForUser(const std::string& userId, BinaryData& publicKey)
   {
      auto contactPublicKeyIterator = contactPublicKeys_.find(userId);

      if (contactPublicKeyIterator == contactPublicKeys_.end()) {
         return false;
      }

      if (contactPublicKeyIterator->second.isNull()) {
         return false;
      }

      publicKey.copyFrom(contactPublicKeyIterator->second);

      return true;
   }

   void ContactPublicKey::setPublicKey(const std::string& userId, const BinaryData& publicKey)
   {
      contactPublicKeys_[userId] = publicKey;
   }

}

#ifndef ContactPublicKey_h__
#define ContactPublicKey_h__

#include <map>

#include <disable_warnings.h>
#include <BinaryData.h>
#include <enable_warnings.h>

namespace spdlog
{
   class logger;
};

namespace Chat {

   class ContactPublicKey
   {
   public:
      ContactPublicKey(const std::shared_ptr<spdlog::logger>& logger);

      ContactPublicKey(const ContactPublicKey& c) = delete;
      ContactPublicKey& operator=(const ContactPublicKey& c) = delete;

      ContactPublicKey(ContactPublicKey&& c) = delete;
      ContactPublicKey& operator=(ContactPublicKey&& c) = delete;

      void loadKeys(const std::map<std::string, BinaryData>&);
      bool findPublicKeyForUser(const std::string& userId, BinaryData& publicKey);
      void setPublicKey(const std::string& userId, const BinaryData& publicKey);

   private:
      std::shared_ptr<spdlog::logger> logger_;
      std::map<std::string, BinaryData> contactPublicKeys_;
   };

   typedef std::shared_ptr<ContactPublicKey> ContactPublicKeyPtr;

}

#endif // ContactPublicKey_h__

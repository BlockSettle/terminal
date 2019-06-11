#ifndef ChatSessionKeyData_h__
#define ChatSessionKeyData_h__

#include <disable_warnings.h>
#include <SecureBinaryData.h>
#include <BinaryData.h>
#include <enable_warnings.h>

#include <memory>

namespace Chat {

   class ChatSessionKeyData
   {
   public:

      std::string receiverId() const;
      void setReceiverId(const std::string receiverId);

      SecureBinaryData localPrivateKey() const;
      void setLocalPrivateKey(const SecureBinaryData localPrivateKey);

      BinaryData localPublicKey() const;
      void setLocalPublicKey(const BinaryData localPublicKey);

      BinaryData remotePublicKey() const;
      void setRemotePublicKey(const BinaryData remotePublicKey);

   private:
      std::string _receiverId;
      SecureBinaryData _localPrivateKey;
      BinaryData _localPublicKey;
      BinaryData _remotePublicKey;
   };

   typedef std::shared_ptr<ChatSessionKeyData> ChatSessionKeyDataPtr;

}

#endif // ChatSessionKeyData_h__

#ifndef SESSION_PUBLIC_KEY_REQUEST
#define SESSION_PUBLIC_KEY_REQUEST

#include <disable_warnings.h>
#include "Request.h"
#include <BinaryData.h>
#include <enable_warnings.h>

namespace Chat {

   class SessionPublicKeyRequest : public Request
   {
   public:
      SessionPublicKeyRequest(
         const std::string& clientId,
         const std::string& senderId,
         const std::string& receiverId,
         const BinaryData& senderSessionPublicKey
      ) : Request(RequestType::RequestSessionPublicKey, clientId),
         senderId_(senderId),
         receiverId_(receiverId),
         senderSessionPublicKey_(senderSessionPublicKey.toHexStr())
      {}

      SessionPublicKeyRequest(
         const std::string& clientId,
         const std::string& senderId,
         const std::string& receiverId,
         const std::string& senderSessionPublicKeyHex
      ) : Request(RequestType::RequestSessionPublicKey, clientId),
         senderId_(senderId),
         receiverId_(receiverId),
         senderSessionPublicKey_(senderSessionPublicKeyHex)
      {}

      QJsonObject toJson() const override;

      static std::shared_ptr<Request> fromJSON(
         const std::string& clientId,
         const std::string& jsonData);

      void handle(RequestHandler&) override;

      std::string senderId() const { return senderId_; }
      std::string receiverId() const { return receiverId_; }
      BinaryData senderSessionPublicKey() const { return BinaryData::CreateFromHex(senderSessionPublicKey_); }

   private:
      std::string senderId_;
      std::string receiverId_;
      std::string senderSessionPublicKey_;
   };

}

#endif //SESSION_PUBLIC_KEY_REQUEST

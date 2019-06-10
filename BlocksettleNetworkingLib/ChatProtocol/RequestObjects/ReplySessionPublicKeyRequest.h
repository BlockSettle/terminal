#ifndef ReplySessionPublicKeyRequest_h__
#define ReplySessionPublicKeyRequest_h__

#include <ChatProtocol/RequestObjects/Request.h>

namespace Chat {

   class ReplySessionPublicKeyRequest : public Request
   {
   public:
      ReplySessionPublicKeyRequest(
         const std::string& clientId,
         const std::string& senderId,
         const std::string& receiverId,
         const BinaryData& senderSessionPublicKey
      ) : Request(RequestType::RequestReplySessionPublicKey, clientId),
         senderId_(senderId),
         receiverId_(receiverId),
         senderSessionPublicKey_(senderSessionPublicKey.toHexStr())
      {}

      ReplySessionPublicKeyRequest(
         const std::string& clientId,
         const std::string& senderId,
         const std::string& receiverId,
         const std::string& senderSessionPublicKeyHex
      ) : Request(RequestType::RequestReplySessionPublicKey, clientId),
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
      std::string senderSessionPublicKey() const { return senderSessionPublicKey_; }

   private:
      std::string senderId_;
      std::string receiverId_;
      std::string senderSessionPublicKey_;
   };

}

#endif // ReplySessionPublicKeyRequest_h__

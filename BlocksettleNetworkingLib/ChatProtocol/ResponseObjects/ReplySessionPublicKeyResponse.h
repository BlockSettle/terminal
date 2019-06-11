#ifndef ReplySessionPublicKeyResponse_h__
#define ReplySessionPublicKeyResponse_h__

#include <disable_warnings.h>
#include <ChatProtocol/ResponseObjects/Response.h>
#include <enable_warnings.h>

namespace Chat {

   class ReplySessionPublicKeyResponse : public Response
   {
   public:
      ReplySessionPublicKeyResponse(
         const std::string& senderId,
         const std::string& receiverId,
         const std::string& senderSessionPublicKey);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(
         const std::string& jsonData);

      void handle(ResponseHandler&) override;

      const std::string& senderId() const { return senderId_; }
      const std::string& receiverId() const { return receiverId_; }
      const std::string& senderSessionPublicKey() const { return senderSessionPublicKey_; }

   private:
      std::string senderId_;
      std::string receiverId_;
      std::string senderSessionPublicKey_;
   };

}

#endif // ReplySessionPublicKeyResponse_h__

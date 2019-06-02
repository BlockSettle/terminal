#ifndef SendOwnPublicKeyResponse_h__
#define SendOwnPublicKeyResponse_h__

#include "Response.h"

#include <disable_warnings.h>
#include <BinaryData.h>
#include <enable_warnings.h>

namespace Chat {
   
   // Response to sending our own public key to the peer who asked for it.
   // Strangely, Response is sent to the peer to whom the key is being sent, not the 
   // node which made the call.
   class SendOwnPublicKeyResponse : public Response
   {
   public:
      SendOwnPublicKeyResponse(
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const BinaryData& sendingNodePublicKey);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(
         const std::string& jsonData);

      void handle(ResponseHandler &) override;

      const std::string& getReceivingNodeId() const;
      const std::string& getSendingNodeId() const;
      const BinaryData& getSendingNodePublicKey() const;

   private:
      std::string receivingNodeId_;
      std::string sendingNodeId_;
      BinaryData sendingNodePublicKey_;
   };
   
}

#endif // SendOwnPublicKeyResponse_h__

#pragma once

#include "Request.h"


#include "SecureBinaryData.h"
#include "autheid_utils.h"

namespace Chat {
   // Request for sending our key to the peer, who previously asked for it.
   class SendOwnPublicKeyRequest : public Request
   {
   public:
      SendOwnPublicKeyRequest(
         const std::string& clientId,
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const autheid::PublicKey& sendingNodePublicKey);

      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(
         const std::string& clientId,
         const std::string& jsonData);

      void handle(RequestHandler &) override;

      const std::string& getReceivingNodeId() const;
      const std::string& getSendingNodeId() const;
      const autheid::PublicKey& getSendingNodePublicKey() const;
      
   private:
      std::string receivingNodeId_;
      std::string sendingNodeId_;
      autheid::PublicKey sendingNodePublicKey_;
   };
}

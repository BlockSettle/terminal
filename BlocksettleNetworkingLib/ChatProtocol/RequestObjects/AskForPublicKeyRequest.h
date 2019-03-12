#pragma once

#include "Request.h"

namespace Chat {
   // Request for asking the peer to send us their public key.
   class AskForPublicKeyRequest : public Request
   {
   public:
      AskForPublicKeyRequest(
         const std::string& clientId,
         const std::string& askingNodeId,
         const std::string& peerId);

      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(
         const std::string& clientId,
         const std::string& jsonData);

      void handle(RequestHandler &) override;

      const std::string& getAskingNodeId() const;
      const std::string& getPeerId() const;

   private:
      std::string askingNodeId_;
      std::string peerId_;
   };
}

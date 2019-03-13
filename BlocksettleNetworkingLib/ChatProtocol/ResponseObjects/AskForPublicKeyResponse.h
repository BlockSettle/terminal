#pragma once

#include "Response.h"

namespace Chat {
   
   // Response to ask a peer to send us his own public key.
   // Strangely, this response is sent to the peer itself, not the one who sent the request.
   class AskForPublicKeyResponse : public Response
   {
   public:
      AskForPublicKeyResponse(
         const std::string& askingNodeId,
         const std::string& peerId);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(
         const std::string& jsonData);

      void handle(ResponseHandler &) override;

      const std::string& getAskingNodeId() const;
      const std::string& getPeerId() const;

   private:
      std::string askingNodeId_;
      std::string peerId_;
   };
   
}

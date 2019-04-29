#pragma once

#include "Request.h"

namespace Chat {
   class ContactActionRequestDirect : public Request
   {
   public:
      ContactActionRequestDirect(const std::string& clientId, const std::string& senderId, const std::string& receiverId, ContactsAction action, autheid::PublicKey publicKey);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId, const std::string& jsonData);
      void handle(RequestHandler &) override;
      std::string senderId() const {return senderId_;}
      std::string receiverId() const {return receiverId_;}
      ContactsAction getAction() const {return action_;}
      autheid::PublicKey getSenderPublicKey() const {return senderPublicKey_;}
   private:
      std::string senderId_;
      std::string receiverId_;
      ContactsAction action_;
      autheid::PublicKey senderPublicKey_;
   };
}

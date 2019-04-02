#pragma once

#include "Request.h"

namespace Chat {
   class ContactActionRequestServer : public Request
   {
   public:
      ContactActionRequestServer(const std::string& clientId, const std::string& senderId, const std::string& contactId, ContactsActionServer action, ContactStatus status, autheid::PublicKey publicKey);
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId, const std::string& jsonData);
      void handle(RequestHandler &) override;
      std::string senderId() const {return senderId_;}
      std::string contactId() const {return contactUserId_;}
      ContactsActionServer getAction() const {return action_;}
      ContactStatus getContactStatus() const {return status_;}
      autheid::PublicKey getContactPublicKey() const {return contactPublicKey_;}
   private:
      std::string senderId_;
      std::string contactUserId_;
      ContactsActionServer action_;
      ContactStatus status_;
      autheid::PublicKey contactPublicKey_;
   };
}

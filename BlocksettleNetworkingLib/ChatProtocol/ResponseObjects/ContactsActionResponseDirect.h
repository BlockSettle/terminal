#pragma once

#include "Response.h"

namespace Chat {
   
   class ContactsActionResponseDirect : public PendingResponse
   {
   public:
      
      ContactsActionResponseDirect(const std::string& senderId, const std::string& receiverId, ContactsAction action, autheid::PublicKey publicKey);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
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

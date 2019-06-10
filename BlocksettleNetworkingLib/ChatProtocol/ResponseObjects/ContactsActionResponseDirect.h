#ifndef ContactsActionResponseDirect_h__
#define ContactsActionResponseDirect_h__

#include "Response.h"

namespace Chat {
   
   class ContactsActionResponseDirect : public PendingResponse
   {
   public:
      
      ContactsActionResponseDirect(const std::string& senderId, const std::string& receiverId, ContactsAction action, BinaryData publicKey);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler&) override;
      std::string senderId() const {return senderId_;} 
      std::string receiverId() const {return receiverId_;} 
      ContactsAction getAction() const {return action_;}
      BinaryData getSenderPublicKey() const {return senderPublicKey_;}
   private:
      std::string senderId_;
      std::string receiverId_;
      ContactsAction action_;
      BinaryData senderPublicKey_;
      
   };
   
}

#endif // ContactsActionResponseDirect_h__

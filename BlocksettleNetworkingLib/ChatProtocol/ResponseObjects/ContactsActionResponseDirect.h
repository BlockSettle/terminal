#ifndef ContactsActionResponseDirect_h__
#define ContactsActionResponseDirect_h__

#include "Response.h"

#include <QDateTime>

namespace Chat {
   
   class ContactsActionResponseDirect : public PendingResponse
   {
   public:
      
      ContactsActionResponseDirect(const std::string& senderId, const std::string& receiverId, ContactsAction action, QString publicKey, const QDateTime &dt);

      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);

      void handle(ResponseHandler&) override;
      
      std::string senderId() const { return senderId_; }
      std::string receiverId() const { return receiverId_; } 
      ContactsAction getAction() const { return action_; }
      QString getSenderPublicKey() const { return senderPublicKey_; }
      BinaryData getSenderPublicKeyBinaryData() const { return BinaryData::CreateFromHex(senderPublicKey_.toStdString()); }
      QDateTime getSenderPublicKeyTimestamp() const { return senderPublicKeyTime_; }

   private:
      std::string senderId_;
      std::string receiverId_;
      ContactsAction action_;
      QString senderPublicKey_;
      QDateTime senderPublicKeyTime_;
   };
   
}

#endif // ContactsActionResponseDirect_h__

#include "ContactsActionResponseDirect.h"

namespace Chat {
   ContactsActionResponseDirect::ContactsActionResponseDirect(
      const std::string& senderId, 
      const std::string& receiverId, 
      ContactsAction action, 
      QString publicKey,
      const QDateTime &dt)
      : PendingResponse (ResponseType::ResponseContactsActionDirect), 
      senderId_(senderId),
      receiverId_(receiverId),
      action_(action),
      senderPublicKey_(publicKey),
      senderPublicKeyTime_(dt)
   {
      
   }
   
   QJsonObject ContactsActionResponseDirect::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      data[PublicKeyKey] = senderPublicKey_;
      data[PublicKeyTimeKey] = senderPublicKeyTime_.toMSecsSinceEpoch();

      return data;
   }
   
   std::shared_ptr<Response> ContactsActionResponseDirect::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      QString publicKey = data[PublicKeyKey].toString();
      QDateTime publicKeyTime = QDateTime::fromMSecsSinceEpoch(data[PublicKeyTimeKey].toDouble());
      return std::make_shared<ContactsActionResponseDirect>(senderId.toStdString(), receiverId.toStdString(), action, publicKey, publicKeyTime);
   }
   
   void ContactsActionResponseDirect::handle(ResponseHandler& handler)
   {
      return handler.OnContactsActionResponseDirect(*this);
   }
}

#include "ContactsActionResponseDirect.h"

namespace Chat {
   ContactsActionResponseDirect::ContactsActionResponseDirect(const std::string& senderId, const std::string& receiverId, ContactsAction action, autheid::PublicKey publicKey)
      : PendingResponse (ResponseType::ResponseContactsActionDirect)
      , senderId_(senderId)
      , receiverId_(receiverId)
      , action_(action)
      , senderPublicKey_(publicKey)
   {
      
   }
   
   QJsonObject ContactsActionResponseDirect::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      data[PublicKeyKey] = QString::fromStdString(Chat::publicKeyToString(senderPublicKey_));

      return data;
   }
   
   std::shared_ptr<Response> ContactsActionResponseDirect::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      autheid::PublicKey publicKey = Chat::publicKeyFromString(data[PublicKeyKey].toString().toStdString());
      return std::make_shared<ContactsActionResponseDirect>(senderId.toStdString(), receiverId.toStdString(), action, publicKey);
   }
   
   void ContactsActionResponseDirect::handle(ResponseHandler& handler)
   {
      return handler.OnContactsActionResponseDirect(*this);
   }
}

#include "ContactsActionResponse.h"

namespace Chat {
   ContactsActionResponse::ContactsActionResponse(const std::string& senderId, const std::string& receiverId, ContactsAction action)
      : PendingResponse (ResponseType::ResponseContactsAction)
      , senderId_(senderId)
      , receiverId_(receiverId)
      , action_(action)
   {
      
   }
   
   QJsonObject ContactsActionResponse::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      return data;
   }
   
   std::shared_ptr<Response> ContactsActionResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString senderId = data[SenderIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      return std::make_shared<ContactsActionResponse>(senderId.toStdString(), receiverId.toStdString(), action);
   }
   
   void ContactsActionResponse::handle(ResponseHandler& handler)
   {
      return handler.OnContactsActionResponse(*this);
   }
}

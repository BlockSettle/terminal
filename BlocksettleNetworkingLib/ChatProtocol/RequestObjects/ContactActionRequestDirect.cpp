#include "ContactActionRequestDirect.h"

namespace Chat {
   ContactActionRequestDirect::ContactActionRequestDirect(const std::string& clientId, const std::string& senderId, const std::string& receiverId, ContactsAction action)
      : Request (RequestType::RequestContactsActionDirect, clientId)
      , senderId_(senderId)
      , receiverId_(receiverId)
      , action_(action)
   {
      
   }
   
   QJsonObject ContactActionRequestDirect::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      return data;
   }
   
   std::shared_ptr<Request> ContactActionRequestDirect::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::string senderId = data[SenderIdKey].toString().toStdString();
      std::string receiverId = data[ReceiverIdKey].toString().toStdString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      return std::make_shared<ContactActionRequestDirect>(clientId, senderId, receiverId, action);
   }
   
   void ContactActionRequestDirect::handle(RequestHandler& handler)
   {
      return handler.OnRequestContactsActionDirect(*this);
   }
}

#include "ContactActionRequest.h"

namespace Chat {
   ContactActionRequest::ContactActionRequest(const std::string& clientId, const std::string& senderId, const std::string& receiverId, ContactsAction action)
      : Request (RequestType::RequestContactsAction, clientId)
      , senderId_(senderId)
      , receiverId_(receiverId)
      , action_(action)
   {
      
   }
   
   QJsonObject ContactActionRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      return data;
   }
   
   std::shared_ptr<Request> ContactActionRequest::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::string senderId = data[SenderIdKey].toString().toStdString();
      std::string receiverId = data[ReceiverIdKey].toString().toStdString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      return std::make_shared<ContactActionRequest>(clientId, senderId, receiverId, action);
   }
   
   void ContactActionRequest::handle(RequestHandler& handler)
   {
      return handler.OnRequestContactsAction(*this);
   }
}

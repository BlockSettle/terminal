#include "ContactActionRequestServer.h"

namespace Chat {
   ContactActionRequestServer::ContactActionRequestServer(const std::string& clientId, const std::string& senderId, 
      const std::string& contactId, ContactsActionServer action, ContactStatus status, BinaryData publicKey)
      : Request (RequestType::RequestContactsActionServer, clientId)
      , senderId_(senderId)
      , contactUserId_(contactId)
      , action_(action)
      , status_(status)
      , contactPublicKey_(publicKey)
   {
      
   }
   
   QJsonObject ContactActionRequestServer::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ContactIdKey] = QString::fromStdString(contactUserId_);
      data[ContactActionKey] = static_cast<int>(action_);
      data[ContactStatusKey] = static_cast<int>(status_);
      data[PublicKeyKey] = QString::fromStdString(contactPublicKey_.toHexStr());
      return data;
   }
   
   std::shared_ptr<Request> ContactActionRequestServer::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::string senderId = data[SenderIdKey].toString().toStdString();
      std::string receiverId = data[ContactIdKey].toString().toStdString();
      ContactsActionServer action = static_cast<ContactsActionServer>(data[ContactActionKey].toInt());
      ContactStatus status = static_cast<ContactStatus>(data[ContactStatusKey].toInt());
      BinaryData publicKey = BinaryData::CreateFromHex(data[PublicKeyKey].toString().toStdString());
      return std::make_shared<ContactActionRequestServer>(clientId, senderId, receiverId, action, status, publicKey);
   }
   
   void ContactActionRequestServer::handle(RequestHandler& handler)
   {
      return handler.OnRequestContactsActionServer(*this);
   }
}

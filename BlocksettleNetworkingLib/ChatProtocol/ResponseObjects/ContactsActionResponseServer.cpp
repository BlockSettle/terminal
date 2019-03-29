#include "ContactsActionResponseServer.h"

namespace Chat {
   ContactsActionResponseServer::ContactsActionResponseServer(const std::string& userId, const std::string& contactId, ContactsActionServer requestedAction, ContactsActionServerResult actionResult, const std::string& message)
      : PendingResponse (ResponseType::ResponseContactsActionServer)
      , userId_(userId)
      , contactId_(contactId)
      , message_(message)
      , requestedAction_(requestedAction)
      , actionResult_(actionResult)
   {
      
   }
   
   QJsonObject ContactsActionResponseServer::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[UserIdKey] = QString::fromStdString(userId_);
      data[ContactIdKey] = QString::fromStdString(contactId_);
      data[ContactActionKey] = static_cast<int>(requestedAction_);
      data[ContactActionResultKey] = static_cast<int>(actionResult_);
      data[ContactActionResultMessageKey] = QString::fromStdString(message_);

      return data;
   }
   
   std::shared_ptr<Response> ContactsActionResponseServer::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString userId = data[UserIdKey].toString();
      QString contactId = data[ContactIdKey].toString();
      ContactsActionServer action = static_cast<ContactsActionServer>(data[ContactActionKey].toInt());
      ContactsActionServerResult result = static_cast<ContactsActionServerResult>(data[ContactActionResultKey].toInt());
      QString message = data[ContactActionResultMessageKey].toString();
      return std::make_shared<ContactsActionResponseServer>(userId.toStdString(), contactId.toStdString(), action, result, message.toStdString());
   }
   
   void ContactsActionResponseServer::handle(ResponseHandler& handler)
   {
      return handler.OnContactsActionResponseServer(*this);
   }

   std::string ContactsActionResponseServer::userId() const
   {
      return userId_;
   }

   std::string ContactsActionResponseServer::contactId() const {return contactId_;}

   std::string ContactsActionResponseServer::message() const {return message_;}

   ContactsActionServer ContactsActionResponseServer::getRequestedAction() const {return requestedAction_;}

   ContactsActionServerResult ContactsActionResponseServer::getActionResult() const {return actionResult_;}
   }

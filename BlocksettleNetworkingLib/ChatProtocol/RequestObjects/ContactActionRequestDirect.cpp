#include "ContactActionRequestDirect.h"

namespace Chat {
   ContactActionRequestDirect::ContactActionRequestDirect(const std::string& clientId, const std::string& senderId, const std::string& receiverId, ContactsAction action, BinaryData publicKey)
      : Request (RequestType::RequestContactsActionDirect, clientId)
      , senderId_(senderId)
      , receiverId_(receiverId)
      , action_(action)
      , senderPublicKey_(publicKey)
      , message_(nullptr)
   {
      
   }
   
   QJsonObject ContactActionRequestDirect::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[ContactActionKey] = static_cast<int>(action_);
      data[PublicKeyKey] = QString::fromStdString(senderPublicKey_.toHexStr());

      if (message_ != nullptr){
         data[MessageKey] = message_->toJson();
      }
      return data;
   }
   
   std::shared_ptr<Request> ContactActionRequestDirect::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::string senderId = data[SenderIdKey].toString().toStdString();
      std::string receiverId = data[ReceiverIdKey].toString().toStdString();
      ContactsAction action = static_cast<ContactsAction>(data[ContactActionKey].toInt());
      BinaryData publicKey = BinaryData::CreateFromHex(data[PublicKeyKey].toString().toStdString());
      auto request = std::make_shared<ContactActionRequestDirect>(clientId, senderId, receiverId, action, publicKey);
      if (data.contains(MessageKey)) {
         request->setMessage(Chat::MessageData::fromJSON(data[MessageKey].toString().toStdString()));
      }
      return std::move(request);
   }
   
   void ContactActionRequestDirect::handle(RequestHandler& handler)
   {
      return handler.OnRequestContactsActionDirect(*this);
   }
   
   std::shared_ptr<Chat::MessageData> ContactActionRequestDirect::getMessage() const
   {
       return message_;
   }
   
   void ContactActionRequestDirect::setMessage(const std::shared_ptr<Chat::MessageData> &message)
   {
       message_ = message;
   }
    }

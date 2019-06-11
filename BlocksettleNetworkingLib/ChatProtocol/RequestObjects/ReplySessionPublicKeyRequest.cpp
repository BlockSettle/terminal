#include "ReplySessionPublicKeyRequest.h"

namespace Chat {

   QJsonObject ReplySessionPublicKeyRequest::toJson() const
   {
      QJsonObject data = Request::toJson();

      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[SenderSessionPublicKeyKey] = QString::fromStdString(senderSessionPublicKey_);

      return data;
   }

   std::shared_ptr<Request> ReplySessionPublicKeyRequest::fromJSON(
      const std::string& clientId,
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();

      return std::make_shared<ReplySessionPublicKeyRequest>(
         clientId,
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString(),
         data[SenderSessionPublicKeyKey].toString().toStdString());
   }

   void ReplySessionPublicKeyRequest::handle(RequestHandler& handler)
   {
      handler.OnReplySessionPublicKeyRequest(*this);
   }

}
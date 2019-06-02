#include "SessionPublicKeyRequest.h"

namespace Chat {

   QJsonObject SessionPublicKeyRequest::toJson() const
   {
      QJsonObject data = Request::toJson();

      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[SenderSessionPublicKeyKey] = QString::fromStdString(senderSessionPublicKey_);

      return data;
   }

   std::shared_ptr<Request> SessionPublicKeyRequest::fromJSON(
      const std::string& clientId,
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();

      return std::make_shared<SessionPublicKeyRequest>(
         clientId,
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString(),
         data[SenderSessionPublicKeyKey].toString().toStdString());
   }

   void SessionPublicKeyRequest::handle(RequestHandler& handler)
   {
      handler.OnSessionPublicKeyRequest(*this);
   }

}

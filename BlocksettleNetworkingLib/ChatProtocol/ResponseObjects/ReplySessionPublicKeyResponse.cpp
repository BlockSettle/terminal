#include "ReplySessionPublicKeyResponse.h"

namespace Chat {

   ReplySessionPublicKeyResponse::ReplySessionPublicKeyResponse(
      const std::string& senderId,
      const std::string& receiverId,
      const std::string& senderSessionPublicKey)
      : Response(ResponseType::ResponseReplySessionPublicKey),
      senderId_(senderId),
      receiverId_(receiverId),
      senderSessionPublicKey_(senderSessionPublicKey)
   {

   }

   QJsonObject ReplySessionPublicKeyResponse::toJson() const
   {
      QJsonObject data = Response::toJson();

      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[SenderSessionPublicKeyKey] = QString::fromStdString(senderSessionPublicKey_);

      return data;
   }

   std::shared_ptr<Response> ReplySessionPublicKeyResponse::fromJSON(
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();

      return std::make_shared<ReplySessionPublicKeyResponse>(
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString(),
         data[SenderSessionPublicKeyKey].toString().toStdString());
   }

   void ReplySessionPublicKeyResponse::handle(ResponseHandler& handler)
   {
      handler.OnReplySessionPublicKeyResponse(*this);
   }

}
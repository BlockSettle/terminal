#include "SendOwnPublicKeyResponse.h"

namespace Chat {
   SendOwnPublicKeyResponse::SendOwnPublicKeyResponse(
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const autheid::PublicKey& sendingNodePublicKey)
      : Response(ResponseType::ResponseSendOwnPublicKey)
      , receivingNodeId_(receivingNodeId)
      , sendingNodeId_(sendingNodeId)
      , sendingNodePublicKey_(sendingNodePublicKey)
   {
   }
   
   QJsonObject SendOwnPublicKeyResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
   
      data[SenderIdKey] = QString::fromStdString(sendingNodeId_);
      data[ReceiverIdKey] = QString::fromStdString(receivingNodeId_);
      data[PublicKeyKey] = QString::fromStdString(
         publicKeyToString(sendingNodePublicKey_));
      return data;
   }
   
   std::shared_ptr<Response> SendOwnPublicKeyResponse::fromJSON(
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<SendOwnPublicKeyResponse>(
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString(), 
         publicKeyFromString(data[PublicKeyKey].toString().toStdString()));
   }
   
   void SendOwnPublicKeyResponse::handle(ResponseHandler& handler)
   {
      handler.OnSendOwnPublicKey(*this);
   }
   
   const std::string& SendOwnPublicKeyResponse::getReceivingNodeId() const {
      return receivingNodeId_;
   }
   
   const std::string& SendOwnPublicKeyResponse::getSendingNodeId() const {
      return sendingNodeId_;
   }
   
   const autheid::PublicKey& SendOwnPublicKeyResponse::getSendingNodePublicKey() const {
      return sendingNodePublicKey_;
   }
}

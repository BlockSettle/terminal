#include "SendOwnPublicKeyResponse.h"

namespace Chat {
   SendOwnPublicKeyResponse::SendOwnPublicKeyResponse(
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const BinaryData& sendingNodePublicKey)
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
      data[PublicKeyKey] = QString::fromStdString(sendingNodePublicKey_.toHexStr());
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
         BinaryData::CreateFromHex(data[PublicKeyKey].toString().toStdString()));
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
   
   const BinaryData& SendOwnPublicKeyResponse::getSendingNodePublicKey() const {
      return sendingNodePublicKey_;
   }
}

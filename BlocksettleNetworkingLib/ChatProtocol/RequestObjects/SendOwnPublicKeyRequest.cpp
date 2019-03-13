#include "SendOwnPublicKeyRequest.h"

namespace Chat {
   SendOwnPublicKeyRequest::SendOwnPublicKeyRequest(
         const std::string& clientId,
         const std::string& receivingNodeId,
         const std::string& sendingNodeId,
         const autheid::PublicKey& sendingNodePublicKey)
      : Request(RequestType::RequestSendOwnPublicKey, clientId)
      , receivingNodeId_(receivingNodeId)
      , sendingNodeId_(sendingNodeId)
      , sendingNodePublicKey_(sendingNodePublicKey)
   {
   }
   
   QJsonObject SendOwnPublicKeyRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[SenderIdKey] = QString::fromStdString(sendingNodeId_);
      data[ReceiverIdKey] = QString::fromStdString(receivingNodeId_);
      data[PublicKeyKey] = QString::fromStdString(
         publicKeyToString(sendingNodePublicKey_));
      return data;
   }
   
   std::shared_ptr<Request> SendOwnPublicKeyRequest::fromJSON(
      const std::string& clientId,
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<SendOwnPublicKeyRequest>(
         clientId,
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString(), 
         publicKeyFromString(data[PublicKeyKey].toString().toStdString()));
   }
   
   void SendOwnPublicKeyRequest::handle(RequestHandler& handler)
   {
      handler.OnSendOwnPublicKey(*this);
   }
   
   const std::string& SendOwnPublicKeyRequest::getReceivingNodeId() const {
      return receivingNodeId_;
   }
   
   const std::string& SendOwnPublicKeyRequest::getSendingNodeId() const {
      return sendingNodeId_;
   }
   
   const autheid::PublicKey& SendOwnPublicKeyRequest::getSendingNodePublicKey() const {
      return sendingNodePublicKey_;
   }
}

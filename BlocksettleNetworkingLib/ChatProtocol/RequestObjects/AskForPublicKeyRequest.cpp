#include "AskForPublicKeyRequest.h"

#include "../ProtocolDefinitions.h"

namespace Chat {
   AskForPublicKeyRequest::AskForPublicKeyRequest(
         const std::string& clientId,
         const std::string& askingNodeId,
         const std::string& peerId)
      : Request(RequestType::RequestAskForPublicKey, clientId)
      , askingNodeId_(askingNodeId)
      , peerId_(peerId)
   {
   }
   
   QJsonObject AskForPublicKeyRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[SenderIdKey] = QString::fromStdString(askingNodeId_);
      data[ReceiverIdKey] = QString::fromStdString(peerId_);
      
      return data;
   }
   
   std::shared_ptr<Request> AskForPublicKeyRequest::fromJSON(
      const std::string& clientId,
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<AskForPublicKeyRequest>(
         clientId,
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString());
   }
   
   void AskForPublicKeyRequest::handle(RequestHandler& handler)
   {
      handler.OnAskForPublicKey(*this);
   }
   
   const std::string& AskForPublicKeyRequest::getAskingNodeId() const
   {
      return askingNodeId_;
   }
   
   const std::string& AskForPublicKeyRequest::getPeerId() const {
      return peerId_;
   }
}

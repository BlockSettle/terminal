#include "AskForPublicKeyResponse.h"

#include "../ProtocolDefinitions.h"

namespace Chat {
   AskForPublicKeyResponse::AskForPublicKeyResponse(
         const std::string& askingNodeId,
         const std::string& peerId)
      : Response(ResponseType::ResponseAskForPublicKey)
      , askingNodeId_(askingNodeId)
      , peerId_(peerId)
   {
   }
   
   QJsonObject AskForPublicKeyResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
   
      data[SenderIdKey] = QString::fromStdString(askingNodeId_);
      data[ReceiverIdKey] = QString::fromStdString(peerId_);
      
      return data;
   }
   
   std::shared_ptr<Response> AskForPublicKeyResponse::fromJSON(
      const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(
         QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<AskForPublicKeyResponse>(
         data[SenderIdKey].toString().toStdString(),
         data[ReceiverIdKey].toString().toStdString());
   }
   
   void AskForPublicKeyResponse::handle(ResponseHandler& handler)
   {
      handler.OnAskForPublicKey(*this);
   }
   
   const std::string& AskForPublicKeyResponse::getAskingNodeId() const
   {
      return askingNodeId_;
   }
   
   const std::string& AskForPublicKeyResponse::getPeerId() const {
      return peerId_;
   }
}

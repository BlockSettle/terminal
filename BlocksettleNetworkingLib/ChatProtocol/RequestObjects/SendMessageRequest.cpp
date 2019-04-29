#include "SendMessageRequest.h"

namespace Chat {
   SendMessageRequest::SendMessageRequest(const std::string& clientId
                                 , const std::string& messageData)
      : Request(RequestType::RequestSendMessage, clientId)
      , messageData_(messageData)
   {
   }
   
   QJsonObject SendMessageRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[MessageKey] = QString::fromStdString(messageData_);
   
      return data;
   }
   
   
   std::shared_ptr<Request> SendMessageRequest::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<SendMessageRequest>(
                        clientId
                       , data[MessageKey].toString().toStdString());
   }
   
   void SendMessageRequest::handle(RequestHandler& handler)
   {
      handler.OnSendMessage(*this);
   }
   
   const std::string& SendMessageRequest::getMessageData() const
   {
      return messageData_;
   }
}

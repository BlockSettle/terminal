#include "MessageChangeStatusRequest.h"

namespace Chat {
   MessageChangeStatusRequest::MessageChangeStatusRequest(const std::string& clientId, const std::string& messageId, int state)
      : Request(RequestType::RequestChangeMessageStatus, clientId)
      , messageId_(messageId)
      , messageState_(state)
   {
   }
   
   
   QJsonObject MessageChangeStatusRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[MessageIdKey] = QString::fromStdString(messageId_);
      data[MessageStateKey] = static_cast<int>(messageState_);
   
      return data;
   }
   
   std::shared_ptr<Request> MessageChangeStatusRequest::fromJSON(const std::string& clientId, const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<MessageChangeStatusRequest>(
                        clientId
                       , data[MessageIdKey].toString().toStdString()
                       , data[MessageStateKey].toInt());
   }
   
   void MessageChangeStatusRequest::handle(RequestHandler& handler)
   {
      handler.OnRequestChangeMessageStatus(*this);
   }
}

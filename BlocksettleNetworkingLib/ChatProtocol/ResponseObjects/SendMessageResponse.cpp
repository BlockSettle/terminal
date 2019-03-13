#include "SendMessageResponse.h"

namespace Chat {
   Chat::SendMessageResponse::SendMessageResponse(const std::string& clientId, const std::string& serverId, const std::string& receiverId, SendMessageResponse::Result result)
      : PendingResponse(ResponseType::ResponseSendMessage)
      , clientMessageId_(clientId), serverMessageId_(serverId), receiverId_(receiverId), result_(result)
   {
      
   }
   
   QJsonObject Chat::SendMessageResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
      data[ClientMessageIdKey] = QString::fromStdString(clientMessageId_);
      data[MessageIdKey] = QString::fromStdString(serverMessageId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
      data[MessageResultKey] = static_cast<int>(result_);
      return data;
   }
   
   std::shared_ptr<Response> Chat::SendMessageResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString clientId = data[ClientMessageIdKey].toString();
      QString serverId = data[MessageIdKey].toString();
      QString receiverId = data[ReceiverIdKey].toString();
      Result result    = static_cast<Result>(data[MessageResultKey].toInt());
      
      return std::make_shared<SendMessageResponse>(clientId.toStdString(), serverId.toStdString(), receiverId.toStdString(), result);
   }
   
   void SendMessageResponse::handle(ResponseHandler& handler)
   {
      handler.OnSendMessageResponse(*this);
   }
}

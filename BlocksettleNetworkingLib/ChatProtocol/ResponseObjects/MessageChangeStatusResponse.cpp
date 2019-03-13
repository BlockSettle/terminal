#include "MessageChangeStatusResponse.h"

namespace Chat {
   MessageChangeStatusResponse::MessageChangeStatusResponse(const std::string& messageId, const std::string& senderId, const std::string& receiverId, int status)
      :PendingResponse(ResponseType::ResponseChangeMessageStatus)
      , messageId_(messageId)
      , messageSenderId_(senderId)
      , messageReceiverId_(receiverId)
      , status_(status)
   {
      
   }
   
   QJsonObject MessageChangeStatusResponse::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[MessageIdKey] = QString::fromStdString(messageId_);
      data[SenderIdKey] = QString::fromStdString(messageSenderId_);
      data[ReceiverIdKey] = QString::fromStdString(messageReceiverId_);
      data[MessageStateKey] = static_cast<int>(status_);
      return data;
   }
   
   std::shared_ptr<Response> MessageChangeStatusResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString messageId = data[MessageIdKey].toString();
      QString messageSenderId = data[SenderIdKey].toString();
      QString messageReceiverId = data[ReceiverIdKey].toString();
      int status = data[MessageStateKey].toInt();
      
      return std::make_shared<MessageChangeStatusResponse>(
                 messageId.toStdString()
               , messageSenderId.toStdString()
               , messageReceiverId.toStdString()
               , status);
   }
   
   void MessageChangeStatusResponse::handle(ResponseHandler& handler)
   {
      handler.OnMessageChangeStatusResponse(*this);
   }
}

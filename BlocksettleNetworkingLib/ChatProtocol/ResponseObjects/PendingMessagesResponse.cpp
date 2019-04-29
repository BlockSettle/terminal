#include "PendingMessagesResponse.h"

namespace Chat {
   PendingMessagesResponse::PendingMessagesResponse(const QString & message_id, const QString &id)
      : PendingResponse(ResponseType::ResponsePendingMessage, id), messageId_(message_id)
   {
   
   }
   
   QString PendingMessagesResponse::getMessageId()
   {
      return messageId_;
   }
   
   QJsonObject PendingMessagesResponse::toJson() const
   {
      QJsonObject data = PendingResponse::toJson();
      data[MessageIdKey] = messageId_;
      return data;
   }
   
   std::shared_ptr<Response> PendingMessagesResponse::fromJSON(const std::string & jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QString messageId = data[MessageIdKey].toString();
      return std::make_shared<PendingMessagesResponse>(messageId);
   }
}

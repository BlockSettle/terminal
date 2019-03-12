#pragma once

#include "Response.h"

namespace Chat {
   
   class PendingMessagesResponse : public PendingResponse
   {
   public: 
      PendingMessagesResponse(const QString& message_id, const QString& id = QString());
      QString getMessageId();
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
   protected:
      QString messageId_;
   };
}

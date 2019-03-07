#pragma once

#include "Request.h"

namespace Chat {
   class MessageChangeStatusRequest : public Request
   {
   public:
      MessageChangeStatusRequest(const std::string& clientId, const std::string& messageId, int state);
      
      const std::string getMessageId() const {return messageId_; }
      int getMessageState() const {return messageState_; }
      
      QJsonObject toJson() const override;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler &) override;
   private:
      const std::string messageId_;
      int messageState_;
   };
}

#pragma once

#include "Response.h"

namespace Chat {
   
   class MessageChangeStatusResponse : public PendingResponse
   {
   public:
      MessageChangeStatusResponse(const std::string& messageId, const std::string& senderId,const std::string& receiverId, int status);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      std::string messageId() const {return messageId_;} 
      std::string messageSenderId() const {return messageSenderId_;}
      std::string messageReceiverId() const {return messageReceiverId_;}
      int getUpdatedStatus() const {return status_; }
      void handle(ResponseHandler&) override;
   private:
      std::string messageId_;
      std::string messageSenderId_;
      std::string messageReceiverId_;
      int status_;
   };
   
}

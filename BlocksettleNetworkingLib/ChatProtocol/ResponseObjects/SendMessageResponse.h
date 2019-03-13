#pragma once

#include "Response.h"

namespace Chat {
   
   class SendMessageResponse : public PendingResponse
   {
   public:
      
      enum class Result {
           Accepted
         , Rejected
      };
      SendMessageResponse(const std::string& clientMessageId, const std::string& serverMessageId, const std::string& receiverId, Result result);
      QJsonObject toJson() const override;
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      
      std::string clientMessageId() const { return clientMessageId_;}
      std::string serverMessageId() const { return serverMessageId_;}
      std::string receiverId() const { return receiverId_;}
      Result getResult() const {return result_;}
      void handle(ResponseHandler&) override;
      
   private:
      std::string clientMessageId_;
      std::string serverMessageId_;
      std::string receiverId_;
      Result result_;
   };
   
}

#pragma once

#include "Request.h"

namespace Chat {
   class ChatroomsListRequest : public Request
   {
   public:
      ChatroomsListRequest(const std::string& clientId, const std::string& senderId);
      std::string getSenderId() const { return senderId_; } 
      QJsonObject toJson() const override;
      void handle(RequestHandler &) override;
   private:
      std::string senderId_;
   };
}

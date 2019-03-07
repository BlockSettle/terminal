#pragma once

#include "Request.h"

namespace Chat {
   class OnlineUsersRequest : public Request
   {
   public:
      OnlineUsersRequest(const std::string& clientId
                     , const std::string& authId);
      QJsonObject toJson() const override;
      void handle(RequestHandler &) override;
      std::string getAuthId() const { return authId_; }

   private:
      std::string authId_;
   };
}

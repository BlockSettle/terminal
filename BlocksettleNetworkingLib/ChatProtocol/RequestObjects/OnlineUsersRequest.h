#ifndef OnlineUsersRequest_h__
#define OnlineUsersRequest_h__

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

#endif // OnlineUsersRequest_h__

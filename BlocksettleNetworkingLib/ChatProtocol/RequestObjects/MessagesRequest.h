#ifndef MessagesRequest_h__
#define MessagesRequest_h__

#include "Request.h"

namespace Chat {
   class MessagesRequest : public Request
   {
   public:
      MessagesRequest(const std::string& clientId
                  , const std::string& senderId
                  , const std::string& receiverId);
      QJsonObject toJson() const override;
      void handle(RequestHandler &) override;
      std::string getSenderId() const { return senderId_; }
      std::string getReceiverId() const { return receiverId_; }

   private:
      std::string senderId_;
      std::string receiverId_;
   };
}

#endif // MessagesRequest_h__

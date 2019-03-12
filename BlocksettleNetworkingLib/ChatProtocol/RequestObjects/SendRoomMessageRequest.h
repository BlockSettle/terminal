#ifndef SENDROOMMESSAGEREQUEST_H
#define SENDROOMMESSAGEREQUEST_H

#include "Request.h"

namespace Chat {
   class SendRoomMessageRequest : public Request
   {
   public:
      SendRoomMessageRequest(const std::string& clientId
      , const std::string& roomId
      , const std::string& messageData);
      
      QJsonObject toJson() const;
      static std::shared_ptr<Request> fromJSON(const std::string& clientId
                                     , const std::string& jsonData);
      void handle(RequestHandler&);
      const std::string& getMessageData() const;
      const std::string& getRoomId() const;
      
   private:
      std::string roomId_;
      std::string messageData_;
   };
}

#endif // SENDROOMMESSAGEREQUEST_H

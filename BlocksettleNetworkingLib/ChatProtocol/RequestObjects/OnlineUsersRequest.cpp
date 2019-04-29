#include "OnlineUsersRequest.h"

namespace Chat {
   OnlineUsersRequest::OnlineUsersRequest(const std::string& clientId
                  , const std::string& authId)
      : Request(RequestType::RequestOnlineUsers, clientId)
      , authId_(authId)
   {
   }
   
   QJsonObject OnlineUsersRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[AuthIdKey] = QString::fromStdString(authId_);
   
      return data;
   }
   
   void OnlineUsersRequest::handle(RequestHandler& handler)
   {
      handler.OnOnlineUsers(*this);
   }
}

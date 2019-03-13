#include "ChatroomsListRequest.h"

namespace Chat {
   ChatroomsListRequest::ChatroomsListRequest(const std::string& clientId, const std::string& senderId)
      : Request (RequestType::RequestChatroomsList, clientId)
      , senderId_(senderId)
   {
      
   }
   
   QJsonObject ChatroomsListRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[SenderIdKey] = QString::fromStdString(senderId_);
   
      return data;
   }
   
   void ChatroomsListRequest::handle(RequestHandler& handler)
   {
      return handler.OnRequestChatroomsList(*this);
   }
}

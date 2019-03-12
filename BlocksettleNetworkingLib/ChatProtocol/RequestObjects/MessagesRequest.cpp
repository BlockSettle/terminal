#include "MessagesRequest.h"

namespace Chat {
   MessagesRequest::MessagesRequest(const std::string& clientId
                            , const std::string& senderId
                            , const std::string& receiverId)
      : Request(RequestType::RequestMessages, clientId)
      , senderId_(senderId)
      , receiverId_(receiverId)
   {
   }
   
   QJsonObject MessagesRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[SenderIdKey] = QString::fromStdString(senderId_);
      data[ReceiverIdKey] = QString::fromStdString(receiverId_);
   
      return data;
   }
   
   void MessagesRequest::handle(RequestHandler& handler)
   {
      handler.OnRequestMessages(*this);
   }
}

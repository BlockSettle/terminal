#include "ChatroomsListResponse.h"

namespace Chat {
   ChatroomsListResponse::ChatroomsListResponse(std::vector<std::string> dataList)
      : ListResponse (ResponseType::ResponseChatroomsList, dataList)
   {
      
   }
   
   std::shared_ptr<Response> ChatroomsListResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<ChatroomsListResponse>(ListResponse::fromJSON(jsonData));
   }
   
   void ChatroomsListResponse::handle(ResponseHandler& handler)
   {
      handler.OnChatroomsList(*this);
   }
}

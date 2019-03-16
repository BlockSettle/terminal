#include "RoomMessagesResponse.h"

namespace Chat {
   RoomMessagesResponse::RoomMessagesResponse(std::vector<std::string> dataList)
      : ListResponse (ResponseType::ResponseRoomMessages, dataList)
   {

   }

   std::shared_ptr<Response> RoomMessagesResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<RoomMessagesResponse>(ListResponse::fromJSON(jsonData));
   }

   void RoomMessagesResponse::handle(ResponseHandler& handler)
   {
      handler.OnRoomMessages(*this);
   }
}


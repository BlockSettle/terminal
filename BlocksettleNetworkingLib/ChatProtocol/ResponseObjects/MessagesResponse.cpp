#include "MessagesResponse.h"

namespace Chat {
   MessagesResponse::MessagesResponse(std::vector<std::string> dataList)
      : ListResponse (ResponseType::ResponseMessages, dataList)
   {
   }

   std::shared_ptr<Response> MessagesResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<MessagesResponse>(ListResponse::fromJSON(jsonData));
   }

   void MessagesResponse::handle(ResponseHandler& handler)
   {
      handler.OnMessages(*this);
   }
}


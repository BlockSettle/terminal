#include "UsersListResponse.h"

namespace Chat {
   UsersListResponse::UsersListResponse(std::vector<std::string> dataList, Command cmd)
      : ListResponse(ResponseType::ResponseUsersList, dataList), cmd_(cmd)
   {
   }

   void UsersListResponse::handle(ResponseHandler& handler)
   {
      handler.OnUsersList(*this);
   }

   std::shared_ptr<Response> UsersListResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      const auto cmd = static_cast<Command>(data[CommandKey].toInt());
      return std::make_shared<UsersListResponse>(ListResponse::fromJSON(jsonData), cmd);
   }

   QJsonObject UsersListResponse::toJson() const
   {
      auto data = ListResponse::toJson();
      data[CommandKey] = static_cast<int>(cmd_);

      return data;
   }
}

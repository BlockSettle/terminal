#include "LoginResponse.h"

namespace Chat {
   LoginResponse::LoginResponse(const std::string& userId, Status status)
      : Response (ResponseType::ResponseLogin)
      , userId_(userId)
      , status_(status)
   {
   }
   
   QJsonObject LoginResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
   
      data[SenderIdKey] = QString::fromStdString(userId_);
      data[StatusKey] = static_cast<int>(status_);
   
      return data;
   }
   
   std::shared_ptr<Response> LoginResponse::fromJSON(const std::string& jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      return std::make_shared<LoginResponse>(
                      data[SenderIdKey].toString().toStdString()
                    , static_cast<Status>(data[StatusKey].toInt()));
   }
   
   void LoginResponse::handle(ResponseHandler& handler)
   {
      handler.OnLoginReturned(*this);
   }
}

#include "LoginRequest.h"

namespace Chat {
   BaseLoginRequest::BaseLoginRequest(RequestType requestType
                        , const std::string& clientId
                        , const std::string& authId
                        , const std::string& jwt)
      : Request (requestType, clientId)
      , authId_(authId)
      , jwt_(jwt)
   {
   
   }
   
   
   QJsonObject BaseLoginRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
   
      data[AuthIdKey] = QString::fromStdString(authId_);
      data[JwtKey] = QString::fromStdString(jwt_);
   
      return data;
   }
   
   
   void LoginRequest::handle(RequestHandler& handler)
   {
      handler.OnLogin(*this);
   }
   
   
   void LogoutRequest::handle(RequestHandler& handler)
   {
      handler.OnLogout(*this);
   }
}

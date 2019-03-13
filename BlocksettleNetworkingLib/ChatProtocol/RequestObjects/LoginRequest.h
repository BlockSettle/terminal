#pragma once

#include "Request.h"

namespace Chat {
   class BaseLoginRequest : public Request
   {
   public:
      BaseLoginRequest(RequestType requestType
                , const std::string& clientId
                , const std::string& authId
                , const std::string& jwt);
      QJsonObject toJson() const override;
      std::string getAuthId() const { return authId_; }
      std::string getJWT() const { return jwt_; }

   protected:
      std::string authId_;
      std::string jwt_;
   };


   class LoginRequest : public BaseLoginRequest
   {
   public:
      LoginRequest(const std::string& clientId
                  , const std::string& authId
                  , const std::string& jwt)
         : BaseLoginRequest (RequestType::RequestLogin, clientId, authId, jwt)
      {
      }
      void handle(RequestHandler &) override;
   };
   
   class LogoutRequest : public BaseLoginRequest
   {
   public:
      LogoutRequest(const std::string& clientId
                  , const std::string& authId
                  , const std::string& jwt)
         : BaseLoginRequest (RequestType::RequestLogout, clientId, authId, jwt)
      {
      }
      void handle(RequestHandler &) override;
   };
}

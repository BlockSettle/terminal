#ifndef LoginRequest_h__
#define LoginRequest_h__

#include "Request.h"

namespace Chat {
   class BaseLoginRequest : public Request
   {
   public:
      BaseLoginRequest(RequestType requestType,
         const std::string& clientId,
         const std::string& authId,
         const std::string& jwt,
         const std::string& publicKey
      );

      QJsonObject toJson() const override;

      std::string authId() const { return authId_; }
      std::string jwt() const { return jwt_; }
      std::string publicKey() const { return publicKey_; }

   protected:
      std::string authId_;
      std::string jwt_;
      std::string publicKey_;
   };


   class LoginRequest : public BaseLoginRequest
   {
   public:
      LoginRequest(const std::string& clientId,
         const std::string& authId,
         const std::string& jwt,
         const std::string& publicKey
      )
         : BaseLoginRequest (RequestType::RequestLogin, clientId, authId, jwt, publicKey)
      {
      }
      void handle(RequestHandler &) override;
   };
   
   class LogoutRequest : public BaseLoginRequest
   {
   public:
      LogoutRequest(const std::string& clientId, 
         const std::string& authId,
         const std::string& jwt,
         const std::string& publicKey
      )
         : BaseLoginRequest (RequestType::RequestLogout, clientId, authId, jwt, publicKey)
      {
      }
      void handle(RequestHandler &) override;
   };
}

#endif // LoginRequest_h__

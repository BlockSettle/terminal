#ifndef LoginResponse_h__
#define LoginResponse_h__

#include "Response.h"

namespace Chat {
   
   class LoginResponse : public Response
   {
   public:
      enum class Status {
           LoginOk
         , LoginFailed
      };

      LoginResponse(const std::string& userId, Status status);
      static std::shared_ptr<Response> fromJSON(const std::string& jsonData);
      void handle(ResponseHandler &) override;
      QJsonObject toJson() const override;

      std::string getUserId() const { return userId_; }
      Status getStatus() const { return status_; }

   private:
      std::string userId_;
      Status status_;
   };
   
}

#endif // LoginResponse_h__

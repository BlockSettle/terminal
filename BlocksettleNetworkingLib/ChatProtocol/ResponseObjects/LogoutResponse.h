#ifndef LogoutResponse_h__
#define LogoutResponse_h__

#include "Response.h"

namespace Chat {

   class LogoutResponse : public Response
   {
   public:
      LogoutResponse();
      void handle(ResponseHandler &) override;
   };
}

#endif // LogoutResponse_h__

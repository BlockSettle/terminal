#ifndef LOGOUTRESPONSE_H
#define LOGOUTRESPONSE_H

#include "Response.h"

namespace Chat {

   class LogoutResponse : public Response
   {
   public:
      LogoutResponse();
      void handle(ResponseHandler &) override;
   };
}

#endif // LOGOUTRESPONSE_H

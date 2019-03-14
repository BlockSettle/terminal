#pragma once

#include "Response.h"

namespace Chat {
   
   class HeartbeatPongResponse : public Response
   {
   public:
      HeartbeatPongResponse();
      void handle(ResponseHandler &) override;
   };
   
}

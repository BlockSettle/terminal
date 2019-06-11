#ifndef HeartbeatPongResponse_h__
#define HeartbeatPongResponse_h__

#include "Response.h"

namespace Chat {
   
   class HeartbeatPongResponse : public Response
   {
   public:
      HeartbeatPongResponse();
      void handle(ResponseHandler &) override;
   };
   
}

#endif // HeartbeatPongResponse_h__

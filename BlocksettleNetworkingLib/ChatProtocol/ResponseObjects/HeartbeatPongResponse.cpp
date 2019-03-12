#include "HeartbeatPongResponse.h"

namespace Chat {
   HeartbeatPongResponse::HeartbeatPongResponse()
      : Response(ResponseType::ResponseHeartbeatPong)
   {
   }
    
   void HeartbeatPongResponse::handle(ResponseHandler& handler)
   {
      handler.OnHeartbeatPong(*this);
   }
}

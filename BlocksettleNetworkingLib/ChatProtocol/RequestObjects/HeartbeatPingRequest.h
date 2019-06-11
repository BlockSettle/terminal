#ifndef HeartbeatPingRequest_h__
#define HeartbeatPingRequest_h__

#include "Request.h"

namespace Chat {
   class HeartbeatPingRequest : public Request
      {
      public:
         HeartbeatPingRequest(const std::string& clientId);
         void handle(RequestHandler &) override;
      };
}

#endif // HeartbeatPingRequest_h__

#pragma once

#include "Request.h"

namespace Chat {
   class HeartbeatPingRequest : public Request
      {
      public:
         HeartbeatPingRequest(const std::string& clientId);
         void handle(RequestHandler &) override;
      };
}

#include "HeartbeatPingRequest.h"

using namespace Chat;

HeartbeatPingRequest::HeartbeatPingRequest(const std::string& clientId)
   : Request (RequestType::RequestHeartbeatPing, clientId)
{
}

void HeartbeatPingRequest::handle(RequestHandler& handler)
{
   handler.OnHeartbeatPing(*this);
}

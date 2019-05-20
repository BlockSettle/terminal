#include "SendOTCDataResponse.h"

namespace Chat {
   SendOTCDataResponse::SendOTCDataResponse(std::shared_ptr<OTCResponseData> otcResponseData)
      : Response (ResponseType::ResponseSendOTCData)
      , otcData_(otcResponseData)
   {

   }

   std::shared_ptr<Response> SendOTCDataResponse::fromJSON(const std::string &jsonData)
   {
      return nullptr;
   }

   void SendOTCDataResponse::handle(ResponseHandler & handler)
   {
      handler.OnSendOTCDataResponse(*this);
   }

   QJsonObject SendOTCDataResponse::toJson() const
   {
      return Response::toJson();
   }

}

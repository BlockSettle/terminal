#include "SendOTCDataRequest.h"

namespace Chat {

   SendOTCDataRequest::SendOTCDataRequest(const std::string &clientId, std::shared_ptr<Chat::OTCRequestData> otcData)
   : Request (RequestType::RequestSendOTCData, clientId)
   , otcData_(otcData)
   {

   }

   QJsonObject SendOTCDataRequest::toJson() const
   {
      return Request::toJson();
   }

   std::shared_ptr<Request> SendOTCDataRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
   {
      return nullptr;
   }

   void Chat::SendOTCDataRequest::handle(RequestHandler & handler)
   {
      handler.OnSendOTCDataRequest(*this);
   }

   const std::shared_ptr<OTCRequestData> SendOTCDataRequest::getOtcRequestData() const
   {
      return otcData_;
   }
}

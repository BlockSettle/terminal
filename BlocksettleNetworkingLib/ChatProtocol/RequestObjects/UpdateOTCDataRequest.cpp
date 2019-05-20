#include "UpdateOTCDataRequest.h"

namespace Chat {

   UpdateOTCDataRequest::UpdateOTCDataRequest(const std::string &clientId, std::shared_ptr<OTCUpdateData> otcUpdateData)
      :Request (RequestType::RequestUpdateOTCData, clientId)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateOTCDataRequest::toJson() const
   {
      return Request::toJson();
   }

   std::shared_ptr<Request> UpdateOTCDataRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
   {
      return nullptr;
   }

   void UpdateOTCDataRequest::handle(RequestHandler & handler)
   {
      handler.OnUpdateOTCDataRequest(*this);
   }

   std::shared_ptr<OTCUpdateData> UpdateOTCDataRequest::getOtcUpdateData() const
   {
      return otcUpdateData_;
   }

}

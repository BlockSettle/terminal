#include "UpdateOTCDataResponse.h"
namespace Chat {

   UpdateOTCDataResponse::UpdateOTCDataResponse(std::shared_ptr<OTCUpdateData> otcUpdateData)
      : Response (ResponseType::ResponseUpdateOTCData)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateOTCDataResponse::toJson() const
   {
      return Response::toJson();
   }

   std::shared_ptr<Response> UpdateOTCDataResponse::fromJSON(const std::string &jsonData)
   {
      return nullptr;
   }

   void UpdateOTCDataResponse::handle(ResponseHandler & handler)
   {
      handler.OnUpdateOTCDataResponse(*this);
   }

   std::shared_ptr<OTCUpdateData> UpdateOTCDataResponse::getOtcUpdateData() const
   {
      return otcUpdateData_;
   }
}

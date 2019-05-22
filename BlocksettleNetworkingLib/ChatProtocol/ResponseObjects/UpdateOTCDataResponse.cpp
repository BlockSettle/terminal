#include "UpdateOTCDataResponse.h"
namespace Chat {

   UpdateOTCDataResponse::UpdateOTCDataResponse(std::shared_ptr<OTCUpdateData> otcUpdateData)
      : Response (ResponseType::ResponseUpdateOTCData)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateOTCDataResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
      data[OTCDataObjectKey] = otcUpdateData_->toJson();
      return data;
   }

   std::shared_ptr<Response> UpdateOTCDataResponse::fromJSON(const std::string &jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::shared_ptr<OTCUpdateData> otcUpdateData = OTCUpdateData::fromJSON(jsonData);

      return std::make_shared<UpdateOTCDataResponse>(otcUpdateData);
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

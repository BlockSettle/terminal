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
      QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

      std::shared_ptr<OTCUpdateData> otcUpdateData =
            OTCUpdateData::fromJSON(
               QString::fromUtf8(innerDataDocument.toJson()).toStdString());

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

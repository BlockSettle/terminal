#include "UpdateCommonOTCResponse.h"
namespace Chat {

   UpdateCommonOTCResponse::UpdateCommonOTCResponse(std::shared_ptr<OTCUpdateData> otcUpdateData)
      : Response (ResponseType::ResponseUpdateCommonOTC)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateCommonOTCResponse::toJson() const
   {
      QJsonObject data = Response::toJson();
      data[OTCDataObjectKey] = otcUpdateData_->toJson();
      return data;
   }

   std::shared_ptr<Response> UpdateCommonOTCResponse::fromJSON(const std::string &jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

      std::shared_ptr<OTCUpdateData> otcUpdateData =
            OTCUpdateData::fromJSON(
               QString::fromUtf8(innerDataDocument.toJson()).toStdString());

      return std::make_shared<UpdateCommonOTCResponse>(otcUpdateData);
   }

   void UpdateCommonOTCResponse::handle(ResponseHandler & handler)
   {
      handler.OnUpdateCommonOTCResponse(*this);
   }

   std::shared_ptr<OTCUpdateData> UpdateCommonOTCResponse::getOtcUpdateData() const
   {
      return otcUpdateData_;
   }
}

#include "UpdateCommonOTCRequest.h"

namespace Chat {

   UpdateCommonOTCRequest::UpdateCommonOTCRequest(const std::string &clientId, std::shared_ptr<OTCUpdateData> otcUpdateData)
      :Request (RequestType::RequestUpdateCommonOTC, clientId)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateCommonOTCRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[OTCDataObjectKey] = otcUpdateData_->toJson();
      return data;
   }

   std::shared_ptr<Request> UpdateCommonOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

      QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

      std::shared_ptr<OTCUpdateData> otcUpdateData =
            OTCUpdateData::fromJSON(
               QString::fromUtf8(innerDataDocument.toJson()).toStdString());


      return std::make_shared<UpdateCommonOTCRequest>(clientId, otcUpdateData);
   }

   void UpdateCommonOTCRequest::handle(RequestHandler & handler)
   {
      handler.OnUpdateCommonOTCRequest(*this);
   }

   std::shared_ptr<OTCUpdateData> UpdateCommonOTCRequest::getOtcUpdateData() const
   {
      return otcUpdateData_;
   }

}

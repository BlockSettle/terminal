#include "UpdateOTCDataRequest.h"

namespace Chat {

   UpdateOTCDataRequest::UpdateOTCDataRequest(const std::string &clientId, std::shared_ptr<OTCUpdateData> otcUpdateData)
      :Request (RequestType::RequestUpdateOTCData, clientId)
      , otcUpdateData_(otcUpdateData)
   {

   }

   QJsonObject UpdateOTCDataRequest::toJson() const
   {
      QJsonObject data = Request::toJson();
      data[OTCDataObjectKey] = otcUpdateData_->toJson();
      return data;
   }

   std::shared_ptr<Request> UpdateOTCDataRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
   {
      QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
      std::shared_ptr<OTCUpdateData> otcUpdateData =  OTCUpdateData::fromJSON(data[OTCDataObjectKey].toString().toStdString());
      return std::make_shared<UpdateOTCDataRequest>(clientId, otcUpdateData);
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

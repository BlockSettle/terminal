#include "SendOTCDataResponse.h"

using namespace Chat;

SendOTCDataResponse::SendOTCDataResponse(std::shared_ptr<OTCResponseData> otcResponseData)
   : Response (ResponseType::ResponseSendOTCData)
   , otcResponseData_(otcResponseData)
{

}

QJsonObject SendOTCDataResponse::toJson() const
{
   QJsonObject data = Response::toJson();
   data[OTCDataObjectKey] = otcResponseData_->toJson();
   return data;
}

std::shared_ptr<Response> SendOTCDataResponse::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCResponseData> otcResponseData =
         OTCResponseData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<SendOTCDataResponse>(otcResponseData);
}

void SendOTCDataResponse::handle(ResponseHandler & handler)
{
   handler.OnSendOTCDataResponse(*this);
}

std::shared_ptr<OTCResponseData> SendOTCDataResponse::otcResponseData() const
{
   return otcResponseData_;
}

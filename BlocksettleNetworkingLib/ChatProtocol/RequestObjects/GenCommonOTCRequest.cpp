#include "GenCommonOTCRequest.h"

using namespace Chat;

GenCommonOTCRequest::GenCommonOTCRequest(const std::string &clientId, std::shared_ptr<Chat::OTCRequestData> otcRequestData)
   : Request (RequestType::RequestGenCommonOTC, clientId)
   , otcRequestData_(otcRequestData)
{

}

QJsonObject GenCommonOTCRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCDataObjectKey] = otcRequestData_->toJson();
   return data;
}

std::shared_ptr<Request> GenCommonOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCRequestData> otcRequestData =
         OTCRequestData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<GenCommonOTCRequest>(clientId, otcRequestData);
}

void Chat::GenCommonOTCRequest::handle(RequestHandler & handler)
{
   handler.OnGenCommonOTCRequest(*this);
}

std::shared_ptr<OTCRequestData> GenCommonOTCRequest::getOtcRequestData() const
{
   return otcRequestData_;
}

std::string GenCommonOTCRequest::getSenderId() const
{
   return otcRequestData_->requestorId();
}

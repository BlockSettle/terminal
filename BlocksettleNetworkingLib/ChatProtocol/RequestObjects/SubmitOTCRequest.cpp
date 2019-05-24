#include "SubmitOTCRequest.h"

using namespace Chat;

SubmitOTCRequest::SubmitOTCRequest(const std::string &clientId, std::shared_ptr<Chat::OTCRequestData> otcRequestData)
   : Request (RequestType::RequestSubmitOTC, clientId)
   , otcRequestData_(otcRequestData)
{

}

QJsonObject SubmitOTCRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCDataObjectKey] = otcRequestData_->toJson();
   return data;
}

std::shared_ptr<Request> SubmitOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCRequestData> otcRequestData =
         OTCRequestData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<SubmitOTCRequest>(clientId, otcRequestData);
}

void Chat::SubmitOTCRequest::handle(RequestHandler & handler)
{
   handler.OnSubmitOTCRequest(*this);
}

const std::shared_ptr<OTCRequestData> SubmitOTCRequest::getOtcRequestData() const
{
   return otcRequestData_;
}

QString SubmitOTCRequest::getSenderId() const
{
   return otcRequestData_->requestorId();
}

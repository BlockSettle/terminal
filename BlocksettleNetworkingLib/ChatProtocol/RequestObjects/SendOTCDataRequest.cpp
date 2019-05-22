#include "SendOTCDataRequest.h"

using namespace Chat;

SendOTCDataRequest::SendOTCDataRequest(const std::string &clientId, std::shared_ptr<Chat::OTCRequestData> otcRequestData)
   : Request (RequestType::RequestSendOTCData, clientId)
   , otcRequestData_(otcRequestData)
{

}

QJsonObject SendOTCDataRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCDataObjectKey] = otcRequestData_->toJson();
   return data;
}

std::shared_ptr<Request> SendOTCDataRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   std::shared_ptr<OTCRequestData> otcRequestData =  OTCRequestData::fromJSON(data[OTCDataObjectKey].toString().toStdString());
   return std::make_shared<SendOTCDataRequest>(clientId, otcRequestData);
}

void Chat::SendOTCDataRequest::handle(RequestHandler & handler)
{
   handler.OnSendOTCDataRequest(*this);
}

const std::shared_ptr<OTCRequestData> SendOTCDataRequest::getOtcRequestData() const
{
   return otcRequestData_;
}

QString SendOTCDataRequest::getSenderId() const
{
   return otcRequestData_->requestorId();
}

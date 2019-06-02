#include "AnswerCommonOTCRequest.h"

using namespace Chat;

AnswerCommonOTCRequest::AnswerCommonOTCRequest(const std::string &clientId,
                                               std::shared_ptr<OTCResponseData> otcResponseData)
   : Request(RequestType::RequestAnswerCommonOTC, clientId),
     otcResponseData_(otcResponseData)
{

}

QJsonObject AnswerCommonOTCRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCDataObjectKey] = otcResponseData_->toJson();
   return data;
}

std::shared_ptr<Request> AnswerCommonOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCResponseData> otcRequestData =
         OTCResponseData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<AnswerCommonOTCRequest>(clientId, otcRequestData);
}

void AnswerCommonOTCRequest::handle(RequestHandler & handler)
{
   handler.OnAnswerCommonOTCRequest(*this);
}

const std::shared_ptr<OTCResponseData> AnswerCommonOTCRequest::getOtcResponseData() const
{
   return otcResponseData_;
}

std::string AnswerCommonOTCRequest::getResponsedId() const
{
   return otcResponseData_->responderId();
}

std::string AnswerCommonOTCRequest::getRequestorId() const
{
   return otcResponseData_->requestorId();
}

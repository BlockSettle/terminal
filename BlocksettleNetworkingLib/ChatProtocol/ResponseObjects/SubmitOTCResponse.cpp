#include "SubmitOTCResponse.h"

using namespace Chat;

SubmitOTCResponse::SubmitOTCResponse(std::shared_ptr<OTCRequestData> otcRequestData,
                                     OTCRequestResult result,
                                     const QString& message)
   : Response (ResponseType::ResponseSubmitOTC)
   , otcRequestData_(otcRequestData)
   , result_(result)
   , message_(message)
{

}

QJsonObject SubmitOTCResponse::toJson() const
{
   QJsonObject data = Response::toJson();
   data[OTCDataObjectKey] = otcRequestData_->toJson();
   data[OTCActionResultKey] = static_cast<int>(result_);
   data[OTCMessageKey] = message_;
   return data;
}

std::shared_ptr<Response> SubmitOTCResponse::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   OTCRequestResult result = static_cast<OTCRequestResult>(data[OTCActionResultKey].toInt());
   QString message = data[OTCMessageKey].toString();
   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCRequestData> otcResponseData =
         OTCRequestData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<SubmitOTCResponse>(otcResponseData, result, message);
}

void SubmitOTCResponse::handle(ResponseHandler & handler)
{
   handler.OnSubmitOTCResponse(*this);
}

std::shared_ptr<OTCRequestData> SubmitOTCResponse::otcRequestData() const
{
   return otcRequestData_;
}

OTCRequestResult SubmitOTCResponse::getResult() const
{
   return result_;
}

QString SubmitOTCResponse::getMessage() const
{
   return message_;
}

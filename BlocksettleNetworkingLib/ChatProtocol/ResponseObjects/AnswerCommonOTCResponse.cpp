#include "AnswerCommonOTCResponse.h"

using namespace Chat;

AnswerCommonOTCResponse::AnswerCommonOTCResponse(std::shared_ptr<OTCResponseData> otcResponseData,
                                                  OTCResult result,
                                                 const QString& message)
   : Response (ResponseType::ResponseAnswerCommonOTC)
   , otcResponseData_(otcResponseData)
   , result_(result)
   , message_(message)

{

}

QJsonObject AnswerCommonOTCResponse::toJson() const
{
   QJsonObject data = Response::toJson();
   data[OTCDataObjectKey] = otcResponseData_->toJson();
   data[OTCResultKey] = static_cast<int>(result_);
   data[OTCMessageKey] = message_;
   return data;
}

std::shared_ptr<Response> AnswerCommonOTCResponse::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   OTCResult result = static_cast<OTCResult>(data[OTCResultKey].toInt());
   QString message = data[OTCMessageKey].toString();
   QJsonDocument innerDataDocument = QJsonDocument(data[OTCDataObjectKey].toObject());

   std::shared_ptr<OTCResponseData> otcResponseData =
         OTCResponseData::fromJSON(
            QString::fromUtf8(innerDataDocument.toJson()).toStdString());

   return std::make_shared<AnswerCommonOTCResponse>(otcResponseData, result, message);
}

void AnswerCommonOTCResponse::handle(ResponseHandler & handler)
{
   handler.OnAnswerCommonOTCResponse(*this);
}

std::shared_ptr<OTCResponseData> AnswerCommonOTCResponse::otcResponseData() const
{
   return otcResponseData_;
}

OTCResult AnswerCommonOTCResponse::getResult() const
{
   return result_;
}

QString AnswerCommonOTCResponse::getMessage() const
{
   return message_;
}

void AnswerCommonOTCResponse::setMessage(const QString &message)
{
   message_ = message;
}

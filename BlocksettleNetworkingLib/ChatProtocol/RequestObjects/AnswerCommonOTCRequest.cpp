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
   return Request::toJson();
}

std::shared_ptr<Request> AnswerCommonOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   return nullptr;
}

void AnswerCommonOTCRequest::handle(RequestHandler & handler)
{
   handler.OnAnswerCommonOTCRequest(*this);
}

const std::shared_ptr<OTCResponseData> AnswerCommonOTCRequest::getOtcResponseData() const
{
   return otcResponseData_;
}

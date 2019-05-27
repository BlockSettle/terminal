#include "AnswerCommonOTCResponse.h"

using namespace Chat;

AnswerCommonOTCResponse::AnswerCommonOTCResponse(std::shared_ptr<OTCResponseData> otcResponseData,
                                                  OTCRequestResult result)
   : Response (ResponseType::ResponseAnswerCommonOTC)
   , otcResponseData_(otcResponseData)
   , result_(result)
{

}

QJsonObject AnswerCommonOTCResponse::toJson() const
{
   return Response::toJson();
}

std::shared_ptr<Response> AnswerCommonOTCResponse::fromJSON(const std::string &jsonData)
{
   return nullptr;
}

void AnswerCommonOTCResponse::handle(ResponseHandler & handler)
{
   handler.OnAnswerCommonOTCResponse(*this);
}

std::shared_ptr<OTCResponseData> AnswerCommonOTCResponse::otcResponseData() const
{
   return otcResponseData_;
}

OTCRequestResult AnswerCommonOTCResponse::getResult() const
{
   return result_;
}

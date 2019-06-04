#include "PullOwnOTCRequest.h"

using namespace Chat;

PullOwnOTCRequest::PullOwnOTCRequest(const std::string &clientId, const QString& targetId, const QString& serverOTCId)
   : Request (RequestType::RequestPullOTC, clientId)
   , targetId_(targetId)
   , serverOTCId_(serverOTCId)
{

}

QJsonObject PullOwnOTCRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCTargetIdKey] = targetId_;
   data[OTCRequestIdServerKey] = serverOTCId_;
   return data;
}

std::shared_ptr<Request> PullOwnOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   QString target = data[OTCTargetIdKey].toString();
   QString requestId = data[OTCRequestIdServerKey].toString();

   return std::make_shared<PullOwnOTCRequest>(clientId, target, requestId);
}

void Chat::PullOwnOTCRequest::handle(RequestHandler & handler)
{
   handler.OnPullOTCRequest(*this);
}

QString PullOwnOTCRequest::targetId() const
{
   return targetId_;
}

QString PullOwnOTCRequest::serverOTCId() const
{
   return serverOTCId_;
}

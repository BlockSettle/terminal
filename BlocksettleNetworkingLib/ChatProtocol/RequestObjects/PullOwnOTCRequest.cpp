#include "PullOwnOTCRequest.h"

using namespace Chat;

PullOwnOTCRequest::PullOwnOTCRequest(const std::string &clientId, const std::string& requesterId, const std::string& serverOTCId)
   : Request (RequestType::RequestPullOTC, clientId)
   , requesterId_(requesterId)
   , serverOTCId_(serverOTCId)
{

}

QJsonObject PullOwnOTCRequest::toJson() const
{
   QJsonObject data = Request::toJson();
   data[OTCRequestorIdKey] = QString::fromStdString(requesterId_);
   data[OTCRequestIdServerKey] = QString::fromStdString(serverOTCId_);
   return data;
}

std::shared_ptr<Request> PullOwnOTCRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

   std::string target = data[OTCRequestorIdKey].toString().toStdString();
   std::string requestId = data[OTCRequestIdServerKey].toString().toStdString();

   return std::make_shared<PullOwnOTCRequest>(clientId, target, requestId);
}

void Chat::PullOwnOTCRequest::handle(RequestHandler & handler)
{
   handler.OnPullOTCRequest(*this);
}

std::string PullOwnOTCRequest::requesterId() const
{
   return requesterId_;
}

std::string PullOwnOTCRequest::serverOTCId() const
{
   return serverOTCId_;
}

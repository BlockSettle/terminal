#include "SearchUsersRequest.h"

using namespace Chat;

SearchUsersRequest::SearchUsersRequest(const std::string clientId, const std::string senderId, const std::string &searchIdPattern)
   : Request (RequestType::RequestSearchUsers, clientId)
   , senderId_(senderId)
   , searchIdPattern_(searchIdPattern)
{

}

QJsonObject SearchUsersRequest::toJson() const
{
   QJsonObject data = Request::toJson();

   data[SenderIdKey] = QString::fromStdString(senderId_);
   data[SearchIdPatternKey] = QString::fromStdString(searchIdPattern_);

   return data;
}

std::shared_ptr<Request> SearchUsersRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<SearchUsersRequest>(
                     clientId
                    , data[SenderIdKey].toString().toStdString()
                    , data[SearchIdPatternKey].toString().toStdString());
}

void SearchUsersRequest::handle(RequestHandler  & handler)
{
   handler.OnSearchUsersRequest(*this);
}

std::string SearchUsersRequest::getSenderId() const
{
   return senderId_;
}

std::string SearchUsersRequest::getSearchIdPattern() const
{
   return searchIdPattern_;
}

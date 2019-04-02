#include "ContactsListRequest.h"

using namespace Chat;

ContactsListRequest::ContactsListRequest(const std::string &clientId, const std::string &authId)
    :Request(RequestType::RequestContactsList, clientId)
    , authId_(authId)
{

}

QJsonObject ContactsListRequest::toJson() const
{
    QJsonObject data = Request::toJson();

    data[AuthIdKey] = QString::fromStdString(authId_);

    return data;
}

std::shared_ptr<Request> ContactsListRequest::fromJSON(const std::string &clientId, const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<ContactsListRequest>(
                     clientId
                    , data[AuthIdKey].toString().toStdString());
}

void ContactsListRequest::handle(RequestHandler & handler)
{
   handler.OnRequestContactsList(*this);
}

std::string ContactsListRequest::getAuthId() const { return authId_; }

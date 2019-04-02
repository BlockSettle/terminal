#include "SearchUsersResponse.h"
using namespace Chat;

SearchUsersResponse::SearchUsersResponse(std::vector<std::string> dataList)
    : ListResponse(ResponseType::ResponseSearchUsers, dataList)
{
   usersList_.reserve(dataList_.size());
   for (const auto& userData : dataList_){
      usersList_.push_back(UserData::fromJSON(userData));
   }
}

SearchUsersResponse::SearchUsersResponse(std::vector<std::shared_ptr<UserData> > usersList)
   : ListResponse(ResponseType::ResponseSearchUsers, {})
   , usersList_(usersList)
{
   std::vector<std::string> users;
   for (const auto& user : usersList_){
      users.push_back(user->toJsonString());
   }
   dataList_ = std::move(users);
}

std::shared_ptr<Response> SearchUsersResponse::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<SearchUsersResponse>(ListResponse::fromJSON(jsonData));
}

void SearchUsersResponse::handle(ResponseHandler & handler)
{
   handler.OnSearchUsersResponse(*this);
}

const std::vector<std::shared_ptr<UserData> > &SearchUsersResponse::getUsersList() const
{
   return usersList_;
}

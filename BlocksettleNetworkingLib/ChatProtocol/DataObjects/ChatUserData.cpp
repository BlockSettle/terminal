#include "ChatUserData.h"

using namespace Chat;

ChatUserData::ChatUserData(const QString &userId, UserStatus status)
   :DataObject (DataObject::Type::ChatUserData)
   , userId_(userId)
   , userStatus_(status)
{

}

QString ChatUserData::getUserId()
{
    return userId_;
}

UserStatus ChatUserData::getUserStatus()
{
    return userStatus_;
}

QJsonObject ChatUserData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   data[UserIdKey] = userId_;
   data[UserStatusKey] = static_cast<int>(userStatus_);

   return data;
}

std::shared_ptr<ChatUserData> ChatUserData::fromJSON(const std::string &jsonData)
{
    QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

    QString userId = data[UserIdKey].toString();
    UserStatus status = static_cast<UserStatus>(data[UserStatusKey].toInt());

    return std::make_shared<ChatUserData>(userId, status);
}

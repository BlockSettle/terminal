#include "UserData.h"

using namespace Chat;

UserData::UserData(const QString &userId, UserStatus status)
   :DataObject (DataObject::Type::UserData)
   , userId_(userId)
   , userStatus_(status)
{

}

QString UserData::getUserId()
{
    return userId_;
}

UserStatus UserData::getUserStatus()
{
    return userStatus_;
}

QJsonObject UserData::toJson() const
{
   QJsonObject data = DataObject::toJson();

   data[UserIdKey] = userId_;
   data[UserStatusKey] = static_cast<int>(userStatus_);

   return data;
}

std::shared_ptr<UserData> UserData::fromJSON(const std::string &jsonData)
{
    QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();

    QString userId = data[UserIdKey].toString();
    UserStatus status = static_cast<UserStatus>(data[UserStatusKey].toInt());

    return std::make_shared<UserData>(userId, status);
}

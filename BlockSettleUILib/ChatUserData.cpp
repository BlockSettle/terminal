#include "ChatUserData.h"

ChatUserData::ChatUserData(QObject *parent)  : QObject(parent)
{
   setUserStatus(ChatUserData::Offline);
   setUserState(ChatUserData::Unknown);
}

QString ChatUserData::userName() const
{
   return _userName;
}

void ChatUserData::setUserName(const QString &userName)
{
   _userName = userName;
   emit userNameChanged();
}

ChatUserData::UserConnectionStatus ChatUserData::userConnectionStatus() const
{
   return _userStatus;
}

void ChatUserData::setUserStatus(const ChatUserData::UserConnectionStatus &userStatus)
{
   _userStatus = userStatus;
   emit userStatusChanged();
}

ChatUserData::UserState ChatUserData::userState() const
{
   return _userState;
}

void ChatUserData::setUserState(const ChatUserData::UserState &userState)
{
   _userState = userState;
   emit userStateChanged();
}

QString ChatUserData::userEmail() const
{
   return _userEmail;
}

void ChatUserData::setUserEmail(const QString &userEmail)
{
   _userEmail = userEmail;
   emit userEmailChanged();
}

QString ChatUserData::userId() const
{
    return _userId;
}

void ChatUserData::setUserId(const QString &userId)
{
    _userId = userId;
    emit userIdChanged();
}

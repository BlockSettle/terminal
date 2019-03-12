#include "ChatUserData.h"

ChatUserData::ChatUserData(QObject *parent)  : QObject(parent)
{
   setUserConnectionStatus(ConnectionStatus::Offline);
   setUserState(State::Unknown);
   setHaveNewMessage(false);
}

QString ChatUserData::userName() const
{
   return userName_;
}

void ChatUserData::setUserName(const QString &userName)
{
   userName_ = userName;
   emit userNameChanged();
}

ChatUserData::ConnectionStatus ChatUserData::userConnectionStatus() const
{
   return userConnectionStatus_;
}

void ChatUserData::setUserConnectionStatus(const ChatUserData::ConnectionStatus &userStatus)
{
   userConnectionStatus_ = userStatus;
   emit userStatusChanged();
}

ChatUserData::State ChatUserData::userState() const
{
   return userState_;
}

void ChatUserData::setUserState(const ChatUserData::State &userState)
{
   userState_ = userState;
   emit userStateChanged();
}

QString ChatUserData::userEmail() const
{
   return userEmail_;
}

void ChatUserData::setUserEmail(const QString &userEmail)
{
   userEmail_ = userEmail;
   emit userEmailChanged();
}

QString ChatUserData::userId() const
{
    return userId_;
}

void ChatUserData::setUserId(const QString &userId)
{
    userId_ = userId;
    emit userIdChanged();
}

bool ChatUserData::haveNewMessage() const
{
   return haveNewMessage_;
}

void ChatUserData::setHaveNewMessage(bool haveNewMessage)
{
   haveNewMessage_ = haveNewMessage;
}

#include "ChatProtocol/ChatUser.h"

using namespace Chat;

ChatUser::ChatUser(QObject* parent) : QObject(parent)
{
}

std::string ChatUser::userHash() const
{
   return userHash_;
}

void ChatUser::setUserHash(const std::string& userName)
{
   userHash_ = userName;

   emit userHashChanged(userHash_);
}

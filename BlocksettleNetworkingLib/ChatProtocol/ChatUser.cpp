#include "ChatProtocol/ChatUser.h"

using namespace Chat;

ChatUser::ChatUser(QObject* parent) : QObject(parent)
{
}

std::string ChatUser::userName() const
{
   return userName_;
}

void ChatUser::setUserName(const std::string& userName)
{
   userName_ = userName;

   emit userNameChanged(userName_);
}

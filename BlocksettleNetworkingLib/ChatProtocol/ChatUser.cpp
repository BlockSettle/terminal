/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

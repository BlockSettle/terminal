#include "ChatUserModel.h"

ChatUserModel::ChatUserModel(QObject *parent) : QObject(parent)
{

}

void ChatUserModel::addUser(const TChatUserDataPtr &chatUserDataPtr)
{
   if (isChatUserExist(chatUserDataPtr->userId()))
      return;

   _chatUserDataListPtr.push_back(chatUserDataPtr);

   emit chatUserAdded(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

void ChatUserModel::removeUser(const TChatUserDataPtr &chatUserDataPtr)
{
   removeByUserId(chatUserDataPtr->userId());
}

void ChatUserModel::removeByUserId(const QString &userId)
{
   TChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);
   if (!chatUserDataPtr)
   {
      return;
   }

   _chatUserDataListPtr.erase(
      std::remove_if(std::begin(_chatUserDataListPtr), std::end(_chatUserDataListPtr),
      [chatUserDataPtr](const TChatUserDataPtr cudPtr)
   {
      return cudPtr && (cudPtr == chatUserDataPtr);
   }));

   emit chatUserRemoved(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

bool ChatUserModel::isChatUserExist(const QString &userId) const
{
   TChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (chatUserDataPtr)
   {
      return true;
   }

   return false;
}

void ChatUserModel::setUserStatus(const QString &userId, const ChatUserData::UserConnectionStatus &userStatus)
{
   TChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return;
   }

   chatUserDataPtr->setUserStatus(userStatus);

   emit chatUserStatusChanged(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

void ChatUserModel::setUserState(const QString &userId, const ChatUserData::UserState &userState)
{
   TChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return;
   }

   chatUserDataPtr->setUserState(userState);

   emit chatUserStateChanged(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

TChatUserDataPtr ChatUserModel::getUserByUserId(const QString &userId) const
{
   auto chatUserIt = std::find_if(std::begin(_chatUserDataListPtr), std::end(_chatUserDataListPtr), [userId](const TChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (0 == chatUserDataPtr->userId().compare(userId));
   });

   if (chatUserIt == std::end(_chatUserDataListPtr))
   {
      return TChatUserDataPtr();
   }

   TChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

void ChatUserModel::resetModel()
{
   while(!_chatUserDataListPtr.empty())
   {
      TChatUserDataPtr chatUserDataPtr = _chatUserDataListPtr.back();
      _chatUserDataListPtr.pop_back();
      emit chatUserRemoved(chatUserDataPtr);
   }

   emit chatUserDataListChanged(_chatUserDataListPtr);
}

TChatUserDataListPtr ChatUserModel::chatUserDataList() const
{
   return _chatUserDataListPtr;
}

bool ChatUserModel::isChatUserInContacts(const QString &userId) const
{
   TChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return false;
   }

   // any state exept unknown belongs to contacts
   return (ChatUserData::Unknown != chatUserDataPtr->userState());
}

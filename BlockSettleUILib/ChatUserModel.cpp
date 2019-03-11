#include "ChatUserModel.h"

ChatUserModel::ChatUserModel(QObject *parent) : QObject(parent)
{

}

void ChatUserModel::addUser(const ChatUserDataPtr &chatUserDataPtr)
{
   if (isChatUserExist(chatUserDataPtr->userId()))
      return;

   _chatUserDataListPtr.push_back(chatUserDataPtr);

   emit chatUserAdded(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

void ChatUserModel::removeUser(const ChatUserDataPtr &chatUserDataPtr)
{
   removeByUserId(chatUserDataPtr->userId());
}

void ChatUserModel::removeByUserId(const QString &userId)
{
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);
   if (!chatUserDataPtr)
   {
      return;
   }

   _chatUserDataListPtr.erase(
      std::remove_if(std::begin(_chatUserDataListPtr), std::end(_chatUserDataListPtr),
      [chatUserDataPtr](const ChatUserDataPtr cudPtr)
   {
      return cudPtr && (cudPtr == chatUserDataPtr);
   }));

   emit chatUserRemoved(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

bool ChatUserModel::isChatUserExist(const QString &userId) const
{
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (chatUserDataPtr)
   {
      return true;
   }

   return false;
}

void ChatUserModel::setUserStatus(const QString &userId, const ChatUserData::ConnectionStatus &userStatus)
{
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return;
   }

   chatUserDataPtr->setUserConnectionStatus(userStatus);

   emit chatUserStatusChanged(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

void ChatUserModel::setUserState(const QString &userId, const ChatUserData::State &userState)
{
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return;
   }

   chatUserDataPtr->setUserState(userState);

   emit chatUserStateChanged(chatUserDataPtr);
   emit chatUserDataListChanged(_chatUserDataListPtr);
}

ChatUserDataPtr ChatUserModel::getUserByUserId(const QString &userId) const
{
   auto chatUserIt = std::find_if (std::begin(_chatUserDataListPtr), std::end(_chatUserDataListPtr), [userId](const ChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (0 == chatUserDataPtr->userId().compare(userId));
   });

   if (chatUserIt == std::end(_chatUserDataListPtr))
   {
      return ChatUserDataPtr();
   }

   ChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

void ChatUserModel::resetModel()
{
   while(!_chatUserDataListPtr.empty())
   {
      ChatUserDataPtr chatUserDataPtr = _chatUserDataListPtr.back();
      _chatUserDataListPtr.pop_back();
      emit chatUserRemoved(chatUserDataPtr);
   }

   emit chatUserDataListChanged(_chatUserDataListPtr);
}

ChatUserDataListPtr ChatUserModel::chatUserDataList() const
{
   return _chatUserDataListPtr;
}

bool ChatUserModel::isChatUserInContacts(const QString &userId) const
{
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return false;
   }

   // any state exept unknown belongs to contacts
   return (ChatUserData::State::Unknown != chatUserDataPtr->userState());
}

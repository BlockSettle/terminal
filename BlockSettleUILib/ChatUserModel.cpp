#include "ChatUserModel.h"
#include "UserHasher.h"

ChatUserModel::ChatUserModel(QObject *parent) : QObject(parent)
{
   hasher_ = std::make_shared<UserHasher>();
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

bool ChatUserModel::hasUnreadMessages() const
{
   ChatUserDataPtr chatUserDataPtr;
   foreach( chatUserDataPtr, _chatUserDataListPtr )
   {
      if (chatUserDataPtr->haveNewMessage()) {
         return true;
      }
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

void ChatUserModel::setUserHaveNewMessage(const QString &userId, const bool &haveNewMessage) {
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return;
   }

   chatUserDataPtr->setHaveNewMessage(haveNewMessage);

   emit chatUserHaveNewMessageChanged(chatUserDataPtr);
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

ChatUserDataPtr ChatUserModel::getUserByUserIdPrefix(const QString &userIdPrefix) const
{
   auto chatUserIt = std::find_if (std::begin(_chatUserDataListPtr), std::end(_chatUserDataListPtr), [userIdPrefix](const ChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (true == chatUserDataPtr->userId().startsWith(userIdPrefix));
   });

   if (chatUserIt == std::end(_chatUserDataListPtr))
   {
      return ChatUserDataPtr();
   }

   ChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

ChatUserDataPtr ChatUserModel::getUserByEmail(const QString &email) const
{
   QString userId = QString::fromStdString(hasher_->deriveKey(email.toStdString()));
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

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

   chatUserDataListPtr_.push_back(chatUserDataPtr);

   emit chatUserAdded(chatUserDataPtr);
   emit chatUserDataListChanged(chatUserDataListPtr_);
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

   chatUserDataListPtr_.erase(
      std::remove_if(std::begin(chatUserDataListPtr_), std::end(chatUserDataListPtr_),
      [chatUserDataPtr](const ChatUserDataPtr cudPtr)
   {
      return cudPtr && (cudPtr == chatUserDataPtr);
   }));

   emit chatUserRemoved(chatUserDataPtr);
   emit chatUserDataListChanged(chatUserDataListPtr_);
}

void ChatUserModel::removeByRoomId(const QString& roomId)
{
   std::shared_ptr<Chat::RoomData> chatRoomDataPtr = getRoomByRoomId(roomId);
   if (!chatRoomDataPtr)
   {
      return;
   }

   chatRoomDataListPtr_.erase(
      std::remove_if(std::begin(chatRoomDataListPtr_), std::end(chatRoomDataListPtr_),
      [chatRoomDataPtr](const std::shared_ptr<Chat::RoomData> cudPtr)
   {
      return cudPtr && (cudPtr == chatRoomDataPtr);
   }));

   emit chatRoomRemoved(chatRoomDataPtr);
   emit chatRoomDataListChanged(chatRoomDataListPtr_);
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
   for (const auto &chatUserDataPtr : chatUserDataListPtr_) {
      if (chatUserDataPtr->haveNewMessage()) {
         return true;
      }
   }

   for (const auto &chatRoomDataPtr : chatRoomDataListPtr_) {
      if (chatRoomDataPtr->haveNewMessage()) {
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
   emit chatUserDataListChanged(chatUserDataListPtr_);
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
   emit chatUserDataListChanged(chatUserDataListPtr_);
}

bool ChatUserModel::setUserHaveNewMessage(const QString &userId, const bool &haveNewMessage) {
   ChatUserDataPtr chatUserDataPtr = getUserByUserId(userId);

   if (!chatUserDataPtr)
   {
      return false;
   }

   chatUserDataPtr->setHaveNewMessage(haveNewMessage);

   emit chatUserHaveNewMessageChanged(chatUserDataPtr);
   emit chatUserDataChanged(chatUserDataPtr);

   return true;
}

bool ChatUserModel::setRoomHaveNewMessage(const QString &roomId, const bool &haveNewMessage)
{
   Chat::RoomDataPtr chatRoomDataPtr = getRoomByRoomId(roomId);

   if (!chatRoomDataPtr) {
      return false;
   }

   chatRoomDataPtr->setHaveNewMessage(haveNewMessage);

   emit chatRoomDataChanged(chatRoomDataPtr);

   return true;

}

ChatUserDataPtr ChatUserModel::getUserByUserId(const QString &userId) const
{
   auto chatUserIt = std::find_if (std::begin(chatUserDataListPtr_), std::end(chatUserDataListPtr_), [userId](const ChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (0 == chatUserDataPtr->userId().compare(userId));
   });

   if (chatUserIt == std::end(chatUserDataListPtr_))
   {
      return ChatUserDataPtr();
   }

   ChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

ChatUserDataPtr ChatUserModel::getUserByUserIdPrefix(const QString &userIdPrefix) const
{
   auto chatUserIt = std::find_if (std::begin(chatUserDataListPtr_), std::end(chatUserDataListPtr_), [userIdPrefix](const ChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (true == chatUserDataPtr->userId().startsWith(userIdPrefix));
   });

   if (chatUserIt == std::end(chatUserDataListPtr_))
   {
      return ChatUserDataPtr();
   }

   ChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

ChatUserDataPtr ChatUserModel::getUserByEmail(const QString &email) const
{
   QString userId = QString::fromStdString(hasher_->deriveKey(email.toStdString()));
   auto chatUserIt = std::find_if (std::begin(chatUserDataListPtr_), std::end(chatUserDataListPtr_), [userId](const ChatUserDataPtr &chatUserDataPtr)->bool
   {
      return (0 == chatUserDataPtr->userId().compare(userId));
   });

   if (chatUserIt == std::end(chatUserDataListPtr_))
   {
      return ChatUserDataPtr();
   }

   ChatUserDataPtr chatUserDataPtr((*chatUserIt));

   return chatUserDataPtr;
}

std::shared_ptr<Chat::RoomData> ChatUserModel::getRoomByRoomId(const QString& roomId) const
{
   auto chatRoomIt = std::find_if(std::begin(chatRoomDataListPtr_), std::end(chatRoomDataListPtr_), [roomId](const std::shared_ptr<Chat::RoomData> &chatRoomDataPtr)->bool
   {
      return (0 == chatRoomDataPtr->getId().compare(roomId));
   });

   if (chatRoomIt == std::end(chatRoomDataListPtr_))
   {
      return std::shared_ptr<Chat::RoomData>();
   }

   std::shared_ptr<Chat::RoomData> chatRoomDataPtr((*chatRoomIt));

   return chatRoomDataPtr;
}

void ChatUserModel::addRoom(const std::shared_ptr<Chat::RoomData> roomData)
{
   if (isChatRoomExist(roomData->getId()))
      return;

   chatRoomDataListPtr_.push_back(roomData);

   emit chatRoomAdded(roomData); 
   emit chatRoomDataListChanged(chatRoomDataListPtr_);
}

bool ChatUserModel::isChatRoomExist(const QString& roomId) const
{
   std::shared_ptr<Chat::RoomData> chatRoomDataPtr = getRoomByRoomId(roomId);

   if (chatRoomDataPtr)
   {
      return true;
   }

   return false;
}

QList<std::shared_ptr<Chat::RoomData> > ChatUserModel::chatRoomDataList() const
{
   return chatRoomDataListPtr_;
}

void ChatUserModel::resetModel()
{
   while(!chatUserDataListPtr_.empty())
   {
      ChatUserDataPtr chatUserDataPtr = chatUserDataListPtr_.back();
      chatUserDataListPtr_.pop_back();
      emit chatUserRemoved(chatUserDataPtr);
   }

   emit chatUserDataListChanged(chatUserDataListPtr_);

   
   while(!chatRoomDataListPtr_.empty())
   {
      std::shared_ptr<Chat::RoomData> roomDataPtr = chatRoomDataListPtr_.back();
      chatRoomDataListPtr_.pop_back();
      emit chatRoomRemoved(roomDataPtr);
   }

   emit chatRoomDataListChanged(chatRoomDataListPtr_);
}

ChatUserDataListPtr ChatUserModel::chatUserDataList() const
{
   return chatUserDataListPtr_;
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

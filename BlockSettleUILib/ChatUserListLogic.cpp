#include <QtDebug>

#include "ChatUserListLogic.h"

#include "disable_warnings.h"
#include <spdlog/spdlog.h>
#include "enable_warnings.h"

#include "ApplicationSettings.h"
#include "ChatClient.h"
#include "NotificationCenter.h"

void ChatUserListLogic::init(const std::shared_ptr<ChatClient> &client,
   const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   client_ = client;
}

ChatUserListLogic::ChatUserListLogic(QObject *parent) : QObject(parent)
{
   chatUserModelPtr_ = std::make_shared<ChatUserModel>(this);
}

void ChatUserListLogic::onAddChatUsers(const UserIdList &userIdList)
{
   for (const std::string &userId : userIdList)
   {
      QString newUserId = QString::fromStdString(userId);

      ChatUserDataPtr chatUserDataPtr = chatUserModelPtr_->getUserByUserId(newUserId);
      // If not found, set online status and add new user
      if (!chatUserDataPtr)
      {
         ChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
         newChatUserData->setUserConnectionStatus(ChatUserData::ConnectionStatus::Online);
         newChatUserData->setUserId(newUserId);
         newChatUserData->setUserName(newUserId);
         chatUserModelPtr_->addUser(newChatUserData);
      }
      else
      // If found then set status to online
      {
         chatUserModelPtr_->setUserStatus(chatUserDataPtr->userId(), ChatUserData::ConnectionStatus::Online);
      }
   }
}

void ChatUserListLogic::onRemoveChatUsers(const UserIdList &userIdList)
{
   for (const std::string &userId : userIdList)
   {
      QString newUserId = QString::fromStdString(userId);
      ChatUserDataPtr chatUserDataPtr = chatUserModelPtr_->getUserByUserId(newUserId);

      if (chatUserDataPtr)
      {
         if (chatUserDataPtr->userState() == ChatUserData::State::Unknown)
         {
            chatUserModelPtr_->removeByUserId(chatUserDataPtr->userId());
         }
         else
         {
            chatUserModelPtr_->setUserStatus(chatUserDataPtr->userId(), ChatUserData::ConnectionStatus::Offline);
         }
      }
   }
}

void ChatUserListLogic::onReplaceChatUsers(const UserIdList &userIdList)
{
   onRemoveChatUsers(userIdList);
   onAddChatUsers(userIdList);
}

void ChatUserListLogic::onIcomingFriendRequest(const UserIdList &userIdList)
{
   for (const std::string &userId : userIdList)
   {
      QString searchUserId = QString::fromStdString(userId);
      ChatUserDataPtr chatUserDataPtr = chatUserModelPtr_->getUserByUserId(searchUserId);

      if (chatUserDataPtr)
      {
         chatUserModelPtr_->setUserState(searchUserId, ChatUserData::State::IncomingFriendRequest);
      }
   }
}

void ChatUserListLogic::onUserHaveNewMessageChanged(const QString &userId, const bool &haveNewMessage, const bool &isInCurrentChat) 
{
   const auto &changed = chatUserModelPtr_->setUserHaveNewMessage(userId, haveNewMessage);
   if (!changed) {
      chatUserModelPtr_->setRoomHaveNewMessage(userId, haveNewMessage);
   }

   bool hasUnreadMessages = chatUserModelPtr_->hasUnreadMessages();
   NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, { tr("New message"), QVariant(isInCurrentChat), QVariant(hasUnreadMessages) });
}

void ChatUserListLogic::addChatRooms(const std::vector<std::shared_ptr<Chat::ChatRoomData> >& roomList)
{
   for (const std::shared_ptr<Chat::ChatRoomData>  &room : roomList)
   {
      std::shared_ptr<Chat::ChatRoomData> chatRoomDataPtr = chatUserModelPtr_->getRoomByRoomId(room->getId());
      
      // If not found, set online status and add new user
      if (!chatRoomDataPtr)
      {
         chatUserModelPtr_->addRoom(room);
      }
//      else
//      // If found then set status to online
//      {
//         _chatUserModelPtr->setUserStatus(chatUserDataPtr->userId(), ChatUserData::ConnectionStatus::Online);
//      }
   }
}

ChatUserModelPtr ChatUserListLogic::chatUserModelPtr() const
{
   return chatUserModelPtr_;
}

void ChatUserListLogic::readUsersFromDB()
{
   ContactUserDataList contactUserDataList;

   if (!client_->getContacts(contactUserDataList))
   {
      logger_->debug("[ChatUserListLogic] failed to get contact list from DB.");
      return;
   }

   for (const ContactUserData &contactUserData : contactUserDataList)
   {
      ChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
      if (contactUserData.incomingFriendRequest())
      {
         newChatUserData->setUserState(ChatUserData::State::IncomingFriendRequest);
      }
      else
      {
         newChatUserData->setUserState(ChatUserData::State::Friend);
      }
      newChatUserData->setUserName(contactUserData.userName());
      newChatUserData->setUserId(contactUserData.userId());
      chatUserModelPtr_->addUser(newChatUserData);
   }
}

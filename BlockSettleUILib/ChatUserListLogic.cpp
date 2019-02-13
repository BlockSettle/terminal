#include <QtDebug>

#include "ChatUserListLogic.h"

#include "disable_warnings.h"
#include <spdlog/spdlog.h>
#include "enable_warnings.h"

#include "ApplicationSettings.h"
#include "ChatClient.h"

void ChatUserListLogic::init(const std::shared_ptr<ChatClient> &client,
   const std::shared_ptr<spdlog::logger>& logger)
{
   _logger = logger;
   _client = client;
}

ChatUserListLogic::ChatUserListLogic(QObject *parent) : QObject(parent)
{
   _chatUserModelPtr = std::make_shared<ChatUserModel>(this);
}

void ChatUserListLogic::onAddChatUsers(const UserIdList &userIdList)
{
   for (const std::string &userId : userIdList)
   {
      QString newUserId = QString::fromStdString(userId);

      ChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(newUserId);
      // If not found, set online status and add new user
      if (!chatUserDataPtr)
      {
         ChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
         newChatUserData->setUserConnectionStatus(ChatUserData::ConnectionStatus::Online);
         newChatUserData->setUserId(newUserId);
         newChatUserData->setUserName(newUserId);
         _chatUserModelPtr->addUser(newChatUserData);
      }
      else
      // If found then set status to online
      {
         _chatUserModelPtr->setUserStatus(chatUserDataPtr->userId(), ChatUserData::ConnectionStatus::Online);
      }
   }
}

void ChatUserListLogic::onRemoveChatUsers(const UserIdList &userIdList)
{
   for (const std::string &userId : userIdList)
   {
      QString newUserId = QString::fromStdString(userId);
      ChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(newUserId);

      if (chatUserDataPtr)
      {
         if (chatUserDataPtr->userState() == ChatUserData::State::Unknown)
         {
            _chatUserModelPtr->removeByUserId(chatUserDataPtr->userId());
         }
         else
         {
            _chatUserModelPtr->setUserStatus(chatUserDataPtr->userId(), ChatUserData::ConnectionStatus::Offline);
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
      ChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(searchUserId);

      if (chatUserDataPtr)
      {
         _chatUserModelPtr->setUserState(searchUserId, ChatUserData::State::IncomingFriendRequest);
      }
   }
}

ChatUserModelPtr ChatUserListLogic::chatUserModelPtr() const
{
   return _chatUserModelPtr;
}

void ChatUserListLogic::readUsersFromDB()
{
   ContactUserDataList contactUserDataList;

   if (!_client->getContacts(contactUserDataList))
   {
      _logger->debug("[ChatUserListLogic] failed to get contact list from DB.");
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
      _chatUserModelPtr->addUser(newChatUserData);
   }
}

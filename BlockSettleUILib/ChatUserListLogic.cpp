#include <QtDebug>

#include "ChatUserListLogic.h"

#include <../disable_warnings.h>
#include <spdlog/spdlog.h>
#include <../enable_warnings.h>

#include "ApplicationSettings.h"
#include "../BlocksettleNetworkingLib/ChatClient.h"

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

void ChatUserListLogic::onAddChatUsers(const TUserIdList &userIdList)
{
   std::for_each(std::begin(userIdList), std::end(userIdList), [this](const std::string &userId)
   {
      QString newUserId = QString::fromStdString(userId);

      TChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(newUserId);
      // If not found, set online status and add new user
      if (!chatUserDataPtr)
      {
         TChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
         newChatUserData->setUserStatus(ChatUserData::Online);
         newChatUserData->setUserId(newUserId);
         newChatUserData->setUserName(newUserId);
         _chatUserModelPtr->addUser(newChatUserData);
      }
      else
      // If found then set status to online
      {
         _chatUserModelPtr->setUserStatus(chatUserDataPtr->userId(), ChatUserData::Online);
      }
   });
}

void ChatUserListLogic::onRemoveChatUsers(const TUserIdList &userIdList)
{
   std::for_each(std::begin(userIdList), std::end(userIdList), [this](const std::string &userId)
   {
      QString newUserId = QString::fromStdString(userId);
      TChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(newUserId);

      if (!chatUserDataPtr)
      {
         if (chatUserDataPtr->userState() == ChatUserData::Unknown)
         {
            _chatUserModelPtr->removeByUserId(chatUserDataPtr->userId());
         }
         else
         {
            _chatUserModelPtr->setUserStatus(chatUserDataPtr->userId(), ChatUserData::Offline);
         }
      }
   });
}

void ChatUserListLogic::onReplaceChatUsers(const TUserIdList &userIdList)
{
   onRemoveChatUsers(userIdList);
   onAddChatUsers(userIdList);
}

void ChatUserListLogic::onIcomingFriendRequest(const TUserIdList &userIdList)
{
   std::for_each(std::begin(userIdList), std::end(userIdList), [this](const std::string &userId)
   {
      QString searchUserId = QString::fromStdString(userId);
      TChatUserDataPtr chatUserDataPtr = _chatUserModelPtr->getUserByUserId(searchUserId);

      if (!chatUserDataPtr)
      {
         _chatUserModelPtr->setUserState(searchUserId, ChatUserData::IncomingFriendRequest);
      }
   });
}

TChatUserModelPtr ChatUserListLogic::chatUserModelPtr() const
{
   return _chatUserModelPtr;
}

void ChatUserListLogic::readUsersFromDB()
{
   TContactUserDataList contactUserDataList;

   if(!_client->getContacts(contactUserDataList))
   {
      _logger->debug("[ChatUserListLogic] failed to get contact list from DB.");
      return;
   }

   std::for_each(std::begin(contactUserDataList), std::end(contactUserDataList), [=](const ContactUserData &contactUserData)
   {
      TChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
      if(contactUserData.incomingFriendRequest())
      {
         newChatUserData->setUserState(ChatUserData::IncomingFriendRequest);
      }
      else
      {
         newChatUserData->setUserState(ChatUserData::Friend);
      }
      newChatUserData->setUserName(contactUserData.userName());
      newChatUserData->setUserId(contactUserData.userId());
      _chatUserModelPtr->addUser(newChatUserData);
   });
}

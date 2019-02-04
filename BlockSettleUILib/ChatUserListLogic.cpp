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

void ChatUserListLogic::addChatUsers(const TUserIdList &userIdList)
{
   std::for_each(std::begin(userIdList), std::end(userIdList), [this](const std::string &userId)
   {
      TChatUserDataListPtr chatUserDataListPtr = _chatUserModelPtr->getChatUserList();
      QString newUserId = QString::fromStdString(userId);
      auto chatUserDataPtrIt = std::find_if(std::begin(chatUserDataListPtr), std::end(chatUserDataListPtr), [newUserId](const TChatUserDataPtr &chatUserDataPtr)->bool
      {
         return (0 == chatUserDataPtr->userId().compare(newUserId));
      });

      // If not found, set online status and add new user
      if (chatUserDataPtrIt == std::end(chatUserDataListPtr))
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
         _chatUserModelPtr->setUserStatus(chatUserDataPtrIt->get()->userId(), ChatUserData::Online);
      }
   });
}

void ChatUserListLogic::removeChatUsers(const TUserIdList &userIdList)
{
   std::for_each(std::begin(userIdList), std::end(userIdList), [this](const std::string &userId)
   {
      TChatUserDataListPtr chatUserDataListPtr = _chatUserModelPtr->getChatUserList();
      QString newUserId = QString::fromStdString(userId);
      auto chatUserDataPtrIt = std::find_if(std::begin(chatUserDataListPtr), std::end(chatUserDataListPtr), [newUserId](const TChatUserDataPtr &chatUserDataPtr)->bool
      {
         return (0 == chatUserDataPtr->userName().compare(newUserId));
      });

      if (chatUserDataPtrIt != std::end(chatUserDataListPtr))
      {
         if (chatUserDataPtrIt->get()->userState() == ChatUserData::Unknown)
         {
            _chatUserModelPtr->removeByUserId(chatUserDataPtrIt->get()->userId());
         }
         else
         {
            _chatUserModelPtr->setUserStatus(chatUserDataPtrIt->get()->userId(), ChatUserData::Offline);
         }
      }
   });
}

void ChatUserListLogic::replaceChatUsers(const TUserIdList &userIdList)
{
   removeChatUsers(userIdList);
   addChatUsers(userIdList);
}

TChatUserModelPtr ChatUserListLogic::chatUserModelPtr() const
{
   return _chatUserModelPtr;
}

void ChatUserListLogic::readUsersFromDB()
{
   TChatUserDataListPtr chatUserDataListPtr = _chatUserModelPtr->getChatUserList();
   TContactUserDataList contactUserDataList;

   if(!_client->getContacts(contactUserDataList))
   {
      _logger->debug("[ChatUserListLogic] failed to get contact list from DB.");
      return;
   }

   std::for_each(std::begin(contactUserDataList), std::end(contactUserDataList), [=](const ContactUserData &contactUserData)
   {
      TChatUserDataPtr newChatUserData = std::make_shared<ChatUserData>();
      if(contactUserData.friendRequestState())
      {
         newChatUserData->setUserState(ChatUserData::FriendRequest);
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

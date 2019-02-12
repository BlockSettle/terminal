#ifndef CHATUSERLISTLOGIC_H
#define CHATUSERLISTLOGIC_H

#include <QObject>

#include "ChatUserModel.h"

namespace spdlog {
   class logger;
}

class ChatClient;

class ChatUserListLogic : public QObject
{
   Q_OBJECT
public:
   typedef std::vector<std::string> TUserIdList;

   explicit ChatUserListLogic(QObject *parent = nullptr);

   void init(const std::shared_ptr<ChatClient> &client,
           const std::shared_ptr<spdlog::logger>& logger);

   void readUsersFromDB();

   ChatUserModelPtr chatUserModelPtr() const;

signals:

public slots:
   void onAddChatUsers(const TUserIdList &userIdList);
   void onRemoveChatUsers(const TUserIdList &userIdList);
   void onReplaceChatUsers(const TUserIdList &userIdList);
   void onIcomingFriendRequest(const TUserIdList &userIdList);

private:
   ChatUserModelPtr _chatUserModelPtr;
   std::shared_ptr<ChatClient>      _client;
   std::shared_ptr<spdlog::logger>  _logger;
};

using ChatUserListLogicPtr = std::shared_ptr<ChatUserListLogic>;

#endif // CHATUSERLISTLOGIC_H

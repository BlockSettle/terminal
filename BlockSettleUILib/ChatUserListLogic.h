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
   void addChatUsers(const TUserIdList &userIdList);
   void removeChatUsers(const TUserIdList &userIdList);
   void replaceChatUsers(const TUserIdList &userIdList);

   TChatUserModelPtr chatUserModelPtr() const;

signals:

public slots:

private:
   TChatUserModelPtr _chatUserModelPtr;
   std::shared_ptr<ChatClient>      _client;
   std::shared_ptr<spdlog::logger>  _logger;
};

typedef std::shared_ptr<ChatUserListLogic> TChatUserListLogicPtr;

#endif // CHATUSERLISTLOGIC_H

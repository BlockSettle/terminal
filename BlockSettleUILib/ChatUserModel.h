#ifndef CHATUSERMODEL_H
#define CHATUSERMODEL_H

#include <QObject>

#include "ChatUserData.h"

class UserHasher;

class ChatUserModel : public QObject
{
   Q_OBJECT
public:
   explicit ChatUserModel(QObject *parent = nullptr);

   void addUser(const ChatUserDataPtr &chatUserDataPtr);
   void removeUser(const ChatUserDataPtr &chatUserDataPtr);
   void removeByUserId(const QString &userId);
   void setUserStatus(const QString &userId, const ChatUserData::ConnectionStatus &userStatus);
   void setUserState(const QString &userId, const ChatUserData::State &userState);
   void setUserHaveNewMessage(const QString &userId, const bool &haveNewMessage);
   void resetModel();

   bool isChatUserExist(const QString &userId) const;
   bool isChatUserInContacts(const QString &userId) const;
   bool hasUnreadMessages() const;

   ChatUserDataListPtr chatUserDataList() const;

   ChatUserDataPtr getUserByUserId(const QString &userId) const;
   ChatUserDataPtr getUserByUserIdPrefix(const QString &userIdPrefix) const;
   ChatUserDataPtr getUserByEmail(const QString &email) const;

signals:
   void chatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList);

   void chatUserAdded(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserRemoved(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStatusChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStateChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserHaveNewMessageChanged(const ChatUserDataPtr &chatUserDataPtr);

public slots:

private:
   ChatUserDataListPtr _chatUserDataListPtr;
   std::shared_ptr<UserHasher> hasher_;
};

using ChatUserModelPtr = std::shared_ptr<ChatUserModel>;

#endif // CHATUSERMODEL_H

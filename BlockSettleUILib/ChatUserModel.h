#ifndef CHATUSERMODEL_H
#define CHATUSERMODEL_H

#include <QObject>

#include "ChatUserData.h"

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
   void resetModel();

   bool isChatUserExist(const QString &userId) const;
   bool isChatUserInContacts(const QString &userId) const;

   ChatUserDataListPtr chatUserDataList() const;

   ChatUserDataPtr getUserByUserId(const QString &userId) const;

signals:
   void chatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList);

   void chatUserAdded(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserRemoved(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStatusChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStateChanged(const ChatUserDataPtr &chatUserDataPtr);

public slots:

private:
   ChatUserDataListPtr _chatUserDataListPtr;
};

using ChatUserModelPtr = std::shared_ptr<ChatUserModel>;

#endif // CHATUSERMODEL_H

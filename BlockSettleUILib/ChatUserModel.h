#ifndef CHATUSERMODEL_H
#define CHATUSERMODEL_H

#include <QObject>

#include "ChatUserData.h"

class ChatUserModel : public QObject
{
   Q_OBJECT
public:
   explicit ChatUserModel(QObject *parent = nullptr);

   void addUser(const TChatUserDataPtr &chatUserDataPtr);
   void removeUser(const TChatUserDataPtr &chatUserDataPtr);
   void removeByUserId(const QString &userId);
   void setUserStatus(const QString &userId, const ChatUserData::UserConnectionStatus &userStatus);
   void setUserState(const QString &userId, const ChatUserData::UserState &userState);
   void resetModel();

   bool isChatUserExist(const QString &userId) const;

   TChatUserDataListPtr getChatUserList() const;

signals:
   void chatUserDataListChanged(const TChatUserDataListPtr &chatUserDataList);

   void chatUserAdded(const TChatUserDataPtr &chatUserDataPtr);
   void chatUserRemoved(const TChatUserDataPtr &chatUserDataPtr);
   void chatUserStatusChanged(const TChatUserDataPtr &chatUserDataPtr);
   void chatUserStateChanged(const TChatUserDataPtr &chatUserDataPtr);

public slots:

private:
   TChatUserDataListPtr::const_iterator getUserByUserId(const QString &userId) const;

   TChatUserDataListPtr _chatUserDataListPtr;
};

typedef std::shared_ptr<ChatUserModel> TChatUserModelPtr;

#endif // CHATUSERMODEL_H

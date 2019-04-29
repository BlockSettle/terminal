#ifndef CHATUSERMODEL_H
#define CHATUSERMODEL_H

#include <QObject>

#include "ChatUserData.h"
#include "ChatProtocol/DataObjects.h"

class UserHasher;

class ChatUserModel : public QObject
{
   Q_OBJECT
public:
   explicit ChatUserModel(QObject *parent = nullptr);

   void addUser(const ChatUserDataPtr &chatUserDataPtr);
   void removeUser(const ChatUserDataPtr &chatUserDataPtr);
   void removeByUserId(const QString &userId);
   void removeByRoomId(const QString &roomId);
   void setUserStatus(const QString &userId, const ChatUserData::ConnectionStatus &userStatus);
   void setUserState(const QString &userId, const ChatUserData::State &userState);
   bool setUserHaveNewMessage(const QString &userId, const bool &haveNewMessage);
   bool setRoomHaveNewMessage(const QString &roomId, const bool &haveNewMessage);
   void resetModel();

   bool isChatUserExist(const QString &userId) const;
   bool isChatUserInContacts(const QString &userId) const;
   bool hasUnreadMessages() const;

   ChatUserDataListPtr chatUserDataList() const;

   ChatUserDataPtr getUserByUserId(const QString &userId) const;
   ChatUserDataPtr getUserByUserIdPrefix(const QString &userIdPrefix) const;
   ChatUserDataPtr getUserByEmail(const QString &email) const;
   std::shared_ptr<Chat::RoomData> getRoomByRoomId(const QString &roomId) const;
   
public:
   void addRoom(const std::shared_ptr<Chat::RoomData> roomData);
   
   bool isChatRoomExist(const QString &roomId) const;
   QList<std::shared_ptr<Chat::RoomData>> chatRoomDataList() const;

signals:
   void chatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList);
   void chatRoomDataListChanged(const QList<std::shared_ptr<Chat::RoomData>> &chatRoomDataList);

   void chatUserAdded(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserRemoved(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStatusChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserStateChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserHaveNewMessageChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatUserDataChanged(const ChatUserDataPtr &chatUserDataPtr);
   void chatRoomDataChanged(const Chat::RoomDataPtr &chatRoomDataPtr);
   void chatRoomAdded(const std::shared_ptr<Chat::RoomData> &chatRoomDataPtr);
   void chatRoomRemoved(const std::shared_ptr<Chat::RoomData>& chatRoomDataPtr);

public slots:

private:
   ChatUserDataListPtr chatUserDataListPtr_;
   std::shared_ptr<UserHasher> hasher_;
   QList<std::shared_ptr<Chat::RoomData>> chatRoomDataListPtr_;
};

using ChatUserModelPtr = std::shared_ptr<ChatUserModel>;

#endif // CHATUSERMODEL_H

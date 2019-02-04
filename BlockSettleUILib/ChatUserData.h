#ifndef CHATUSER_H
#define CHATUSER_H

#include <QString>
#include <QObject>

class ChatUserData : public QObject
{
   Q_OBJECT
public:

   typedef enum
   {
      Offline = 0,
      Online = 1
   } UserConnectionStatus;

   typedef enum
   {
      Unknown = 0,
      Friend = 1,
      FriendRequest = 2
   } UserState;

   ChatUserData(QObject *parent = nullptr);

   QString userName() const;
   void setUserName(const QString &userName);

   ChatUserData::UserConnectionStatus userConnectionStatus() const;
   void setUserStatus(const ChatUserData::UserConnectionStatus &userConnectionStatus);

   ChatUserData::UserState userState() const;
   void setUserState(const ChatUserData::UserState &userState);

   QString userEmail() const;
   void setUserEmail(const QString &userEmail);

   QString userId() const;
   void setUserId(const QString &userId);

signals:
   void userNameChanged();
   void userStatusChanged();
   void userStateChanged();
   void userEmailChanged();
   void userIdChanged();

private:
   ChatUserData::UserConnectionStatus _userStatus;
   QString _userName;
   QString _userEmail;
   QString _userId;
   ChatUserData::UserState _userState;
};

typedef std::shared_ptr<ChatUserData> TChatUserDataPtr;
typedef std::vector<TChatUserDataPtr> TChatUserDataListPtr;

#endif // CHATUSER_H

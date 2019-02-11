#ifndef CHATUSER_H
#define CHATUSER_H

#include <QString>
#include <QObject>

namespace ChatUserColor {
   static const QString COLOR_USER_ONLINE = QStringLiteral("#00c8f8");
   static const QString COLOR_INCOMING_FRIEND_REQUEST = QStringLiteral("#ffa834");
   static const QString COLOR_USER_DEFAULT = QStringLiteral("#c0c0c0");
}

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
      IncomingFriendRequest = 2
   } UserState;

   ChatUserData(QObject *parent = nullptr);

   QString userName() const;
   void setUserName(const QString &userName);

   ChatUserData::UserConnectionStatus userConnectionStatus() const;
   void setUserConnectionStatus(const ChatUserData::UserConnectionStatus &userConnectionStatus);

   ChatUserData::UserState userState() const;
   void setUserState(const ChatUserData::UserState &userState);

   QString userEmail() const;
   void setUserEmail(const QString &userEmail);

   QString userId() const;
   void setUserId(const QString &userId);

   bool haveNewMessage() const;
   void setHaveNewMessage(bool haveNewMessage);

signals:
   void userNameChanged();
   void userStatusChanged();
   void userStateChanged();
   void userEmailChanged();
   void userIdChanged();

private:
   ChatUserData::UserConnectionStatus _userConnectionStatus;
   QString _userName;
   QString _userEmail;
   QString _userId;
   ChatUserData::UserState _userState;
   bool _haveNewMessage;
};

typedef std::shared_ptr<ChatUserData> TChatUserDataPtr;
typedef QList<TChatUserDataPtr> TChatUserDataListPtr;

Q_DECLARE_METATYPE(ChatUserData::UserConnectionStatus)
Q_DECLARE_METATYPE(ChatUserData::UserState);

#endif // CHATUSER_H

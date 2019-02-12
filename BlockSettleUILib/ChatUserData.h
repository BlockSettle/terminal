#ifndef CHATUSER_H
#define CHATUSER_H

#include <QString>
#include <QObject>

#include <memory>

namespace ChatUserColor {
   static const QString COLOR_USER_ONLINE = QStringLiteral("#00c8f8");
   static const QString COLOR_INCOMING_FRIEND_REQUEST = QStringLiteral("#ffa834");
   static const QString COLOR_USER_DEFAULT = QStringLiteral("#c0c0c0");
}

class ChatUserData : public QObject
{
   Q_OBJECT
public:

   enum class ConnectionStatus
   {
      Offline = 0,
      Online = 1
   };

   enum class State
   {
      Unknown = 0,
      Friend = 1,
      IncomingFriendRequest = 2
   };

   ChatUserData(QObject *parent = nullptr);

   QString userName() const;
   void setUserName(const QString &userName);

   ConnectionStatus userConnectionStatus() const;
   void setUserConnectionStatus(const ConnectionStatus &userConnectionStatus);

   State userState() const;
   void setUserState(const State &userState);

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
   ConnectionStatus _userConnectionStatus;
   QString _userName;
   QString _userEmail;
   QString _userId;
   State _userState;
   bool _haveNewMessage;
};

using ChatUserDataPtr = std::shared_ptr<ChatUserData>;
using ChatUserDataListPtr = QList<ChatUserDataPtr>;

Q_DECLARE_METATYPE(ChatUserData::ConnectionStatus)
Q_DECLARE_METATYPE(ChatUserData::State);

#endif // CHATUSER_H

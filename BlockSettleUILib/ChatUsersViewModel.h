#ifndef CHAT_USERS_VIEW_MODEL
#define CHAT_USERS_VIEW_MODEL

#include <QAbstractItemModel>
#include <QMap>
#include <QVector>

#include <memory>

#include "ChatUserData.h"

class ChatUsersViewModel : public QAbstractTableModel
{
   Q_OBJECT

public:

   enum Role
   {
      UserConnectionStatusRole = Qt::UserRole,
      UserStateRole,
      UserNameRole,
      HaveNewMessageRole
   };

   ChatUsersViewModel(QObject* parent = nullptr);
   ~ChatUsersViewModel() noexcept override = default;

   ChatUsersViewModel(const ChatUsersViewModel&) = delete;
   ChatUsersViewModel& operator = (const ChatUsersViewModel&) = delete;

   ChatUsersViewModel(ChatUsersViewModel&&) = delete;
   ChatUsersViewModel& operator = (ChatUsersViewModel&&) = delete;

   QString resolveUser(const QModelIndex &) const;

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void onUserDataListChanged(const ChatUserDataListPtr &chatUserDataListPtr);

private:
   ChatUserDataListPtr _users;
};


#endif // CHAT_USERS_VIEW_MODEL

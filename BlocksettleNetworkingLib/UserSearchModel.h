#ifndef USERSEARCHMODEL_H
#define USERSEARCHMODEL_H

#include <QAbstractListModel>

#include <memory>

namespace Chat {
   class UserData;
}

class UserSearchModel : public QAbstractListModel
{
   Q_OBJECT
public:
   enum CustomRoles {
      UserStatusRole = Qt::UserRole + 1
   };
   enum class UserStatus {
      ContactUnknown,
      ContactAdded,
      ContactPending,
      ContactRejected
   };
   Q_ENUM(UserStatus)

   typedef std::pair<QString,UserStatus> UserInfo;
   explicit UserSearchModel(QObject *parent = nullptr);

   void setUsers(const std::vector<UserInfo> &users);

   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
   std::vector<UserInfo> users_;
};

#endif // USERSEARCHMODEL_H

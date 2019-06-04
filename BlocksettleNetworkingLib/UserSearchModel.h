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
      IsInContacts = Qt::UserRole + 1
   };
   explicit UserSearchModel(QObject *parent = nullptr);

   void setUsers(const std::vector<std::pair<QString,bool>> &users);

   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
   std::vector<std::pair<QString,bool>> users_;
};

#endif // USERSEARCHMODEL_H

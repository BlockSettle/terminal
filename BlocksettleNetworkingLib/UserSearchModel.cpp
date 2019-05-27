#include "UserSearchModel.h"

#include "ChatProtocol/DataObjects/UserData.h"

#include <QSize>

constexpr int kRowHeigth = 25;

UserSearchModel::UserSearchModel(QObject *parent) : QAbstractListModel(parent)
{
}

void UserSearchModel::setUsers(const std::vector<std::pair<QString, bool> > &users)
{
   beginResetModel();
   users_.clear();
   for (const auto &user : users) {
      users_.push_back(user);
   }
   endResetModel();
}

int UserSearchModel::rowCount(const QModelIndex &parent) const
{
   Q_UNUSED(parent)
   return static_cast<int>(users_.size());
}

QVariant UserSearchModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid()) {
      return QVariant();
   }
   if (index.row() < 0 || index.row() >= static_cast<int>(users_.size())) {
      return QVariant();
   }
   switch (role) {
   case Qt::DisplayRole:
      return QVariant::fromValue(users_.at(static_cast<size_t>(index.row())).first);
   case UserSearchModel::IsInContacts:
      return QVariant::fromValue(users_.at(static_cast<size_t>(index.row())).second);
   case Qt::SizeHintRole:
      return QVariant::fromValue(QSize(20, kRowHeigth));
   default:
      return QVariant();
   }
}

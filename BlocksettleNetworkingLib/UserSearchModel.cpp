#include "UserSearchModel.h"

#include "ChatProtocol/DataObjects/UserData.h"

#include <QSize>
#include <QtGui/QColor>

constexpr int kRowHeigth = 20;

const QColor kContactUnknown           = QColor(0xc0c0c0);
const QColor kContactAdded             = QColor(0x00c8f8);
const QColor kContactPendingIncoming   = QColor(0xffa834);
const QColor kContactPendingOutgoing   = QColor(0xa0bc5d);
const QColor kContactRejected          = QColor(0xc4362f);

UserSearchModel::UserSearchModel(QObject *parent) : QAbstractListModel(parent)
{
}

void UserSearchModel::setUsers(const std::vector<UserInfo> &users)
{
   beginResetModel();
   users_.clear();
   users_.reserve(users.size());
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
   case UserSearchModel::UserStatusRole: {
      return QVariant::fromValue(users_.at(static_cast<size_t>(index.row())).second);
   }
   case Qt::SizeHintRole:
      return QVariant::fromValue(QSize(20, kRowHeigth));
   case Qt::ForegroundRole: {
      auto status = users_.at(static_cast<size_t>(index.row())).second;
      switch (status) {
      case UserSearchModel::UserStatus::ContactUnknown:
         return QVariant::fromValue(kContactUnknown);
      case UserSearchModel::UserStatus::ContactAccepted:
         return QVariant::fromValue(kContactAdded);
      case UserSearchModel::UserStatus::ContactPendingIncoming:
         return QVariant::fromValue(kContactPendingIncoming);
      case UserSearchModel::UserStatus::ContactPendingOutgoing:
         return QVariant::fromValue(kContactPendingOutgoing);
      default:
         return QVariant();
      }
   }
   default:
      return QVariant();
   }
}

Qt::ItemFlags UserSearchModel::flags(const QModelIndex &index) const
{
   Q_UNUSED(index)
   return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

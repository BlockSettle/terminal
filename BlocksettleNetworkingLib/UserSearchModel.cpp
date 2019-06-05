#include "UserSearchModel.h"

#include "ChatProtocol/DataObjects/UserData.h"

#include <QSize>
#include <QtGui/QColor>

constexpr int kRowHeigth = 20;
const QColor kContactUnknown = Qt::gray;
const QColor kContactAdded = QColor(0x00c8f8);
const QColor kContactPendingIncoming = Qt::darkYellow;
const QColor kContactPendingOutgoing = Qt::darkGreen;
const QColor kContactRejected = Qt::darkRed;

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
      auto status = users_.at(static_cast<size_t>(index.row())).second;
      return QVariant::fromValue(status == UserStatus::ContactAdded);
   }
   case Qt::SizeHintRole:
      return QVariant::fromValue(QSize(20, kRowHeigth));
   case Qt::ForegroundRole: {
      auto status = users_.at(static_cast<size_t>(index.row())).second;
      switch (status) {
      case UserSearchModel::UserStatus::ContactUnknown:
         return QVariant::fromValue(kContactUnknown);
      case UserSearchModel::UserStatus::ContactAdded:
         return QVariant::fromValue(kContactAdded);
      case UserSearchModel::UserStatus::ContactPending:
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

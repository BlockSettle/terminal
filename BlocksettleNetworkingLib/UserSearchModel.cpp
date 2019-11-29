/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UserSearchModel.h"

#include <QSize>
#include <QtGui/QColor>

constexpr int kRowHeigth = 20;

constexpr char kContactUnknown[]          = "color_contact_unknown";
constexpr char kContactAdded[]            = "color_contact_accepted";
constexpr char kContactPendingIncoming[]  = "color_contact_incoming";
constexpr char kContactPendingOutgoing[]  = "color_contact_outgoing";
//constexpr char kContactRejected[]         = "color_contact_rejected";

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

void UserSearchModel::setItemStyle(std::shared_ptr<QObject> itemStyle)
{
   itemStyle_ = itemStyle;
}

int UserSearchModel::rowCount(const QModelIndex &) const
{
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
   
   switch (role)
   {
   case Qt::DisplayRole:
      return QVariant::fromValue(users_.at(static_cast<size_t>(index.row())).first);

   case UserSearchModel::UserStatusRole:
      return QVariant::fromValue(users_.at(static_cast<size_t>(index.row())).second);

   case Qt::SizeHintRole:
      return QVariant::fromValue(QSize(20, kRowHeigth));

   case Qt::ForegroundRole:
   {
      if (!itemStyle_) {
         return QVariant();
      }

      auto status = users_.at(static_cast<size_t>(index.row())).second;
      switch (status)
      {
      case UserStatus::ContactUnknown:
         return itemStyle_->property(kContactUnknown);
      case UserStatus::ContactAccepted:
         return itemStyle_->property(kContactAdded);
      case UserStatus::ContactPendingIncoming:
         return itemStyle_->property(kContactPendingIncoming);
      case UserStatus::ContactPendingOutgoing:
         return itemStyle_->property(kContactPendingOutgoing);
      default:
         return QVariant();
      }
   }

   default:
     return QVariant();
   }
}

Qt::ItemFlags UserSearchModel::flags(const QModelIndex &) const
{
   return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef USERSEARCHMODEL_H
#define USERSEARCHMODEL_H

#include <memory>

#include <QAbstractListModel>
#include "ChatProtocol/ClientPartyModel.h"

class ChatSearchListViewItemStyle;

class UserSearchModel : public QAbstractListModel
{
   Q_OBJECT
public:
   enum CustomRoles 
   {
      UserStatusRole = Qt::UserRole + 1
   };

   enum class UserStatus
   {
      ContactUnknown,
      ContactAccepted,
      ContactPendingIncoming,
      ContactPendingOutgoing,
      ContactRejected
   };
   Q_ENUM(UserStatus)

   typedef std::pair<QString, UserStatus> UserInfo;
   explicit UserSearchModel(QObject *parent = nullptr);

   void setUsers(const std::vector<UserInfo> &users);
   void setItemStyle(std::shared_ptr<QObject> itemStyle);

   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
   std::vector<UserInfo> users_;
   std::shared_ptr<QObject> itemStyle_;
};

#endif // USERSEARCHMODEL_H

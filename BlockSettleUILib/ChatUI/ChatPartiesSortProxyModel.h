/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHATPARTYSORTPROXYMODEL_H
#define CHATPARTYSORTPROXYMODEL_H

#include "ChatPartiesTreeModel.h"
#include <QSortFilterProxyModel>

class ChatPartiesSortProxyModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit ChatPartiesSortProxyModel(ChatPartiesTreeModelPtr sourceModel, QObject *parent = nullptr);

   PartyTreeItem* getInternalData(const QModelIndex& index) const;

   const std::string& currentUser() const;
   Qt::ItemFlags flags(const QModelIndex& index) const override;

   QModelIndex getProxyIndexById(const std::string& partyId) const;
   QModelIndex getOTCGlobalRoot() const;

protected:

   bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
   bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
   ChatPartiesTreeModelPtr sourceModel_;
};

using ChatPartiesSortProxyModelPtr = std::shared_ptr<ChatPartiesSortProxyModel>;

#endif // CHATPARTYSORTPROXYMODEL_H

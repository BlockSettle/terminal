/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHATCLIENTUSERSVIEWITEMDELEGATE_H
#define CHATCLIENTUSERSVIEWITEMDELEGATE_H

#include <QStyledItemDelegate>
#include "ChatUsersViewItemStyle.h"

#include "ChatPartiesSortProxyModel.h"

class ChatClientUsersViewItemDelegate : public QStyledItemDelegate
{
   Q_OBJECT
public:
   explicit ChatClientUsersViewItemDelegate(ChatPartiesSortProxyModelPtr proxyModel, QObject *parent = nullptr);

public:
   void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
   void paintPartyContainer(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
   void paintParty(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

   void paintInitParty(PartyTreeItem* partyTreeItem, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;
   void paintRequestParty(Chat::ClientPartyPtr& clientPartyPtr, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;

private:
   ChatUsersViewItemStyle itemStyle_;
   ChatPartiesSortProxyModelPtr proxyModel_;
};
#endif // CHATCLIENTUSERSVIEWITEMDELEGATE_H

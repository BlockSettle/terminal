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

   void paintInitParty(Chat::ClientPartyPtr& clientPartyPtr, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;
   void paintRequestParty(Chat::ClientPartyPtr& clientPartyPtr, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;

private:
   ChatUsersViewItemStyle itemStyle_;
   ChatPartiesSortProxyModelPtr proxyModel_;
};
#endif // CHATCLIENTUSERSVIEWITEMDELEGATE_H

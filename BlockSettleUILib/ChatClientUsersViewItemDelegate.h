#ifndef CHATCLIENTUSERSVIEWITEMDELEGATE_H
#define CHATCLIENTUSERSVIEWITEMDELEGATE_H

#include <QStyledItemDelegate>
#include "ChatUsersViewItemStyle.h"

#include "ChatPartiesTreeModel.h"

class ChatClientUsersViewItemDelegate : public QStyledItemDelegate
{
   Q_OBJECT
public:
   explicit ChatClientUsersViewItemDelegate(ChatPartiesSortProxyModelPtr proxyModel, QObject *parent = nullptr);

signals:

public slots:

   // QAbstractItemDelegate interface
public:
   void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
protected:

   // #new_logic
   void paintPartyContainer(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
   void paintParty(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

   void paintInitParty(Chat::ClientPartyPtr clientPartyPtr, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;
   void paintRequestParty(Chat::ClientPartyPtr clientPartyPtr, QPainter* painter,
      QStyleOptionViewItem& itemOption) const;

   // #old_logic
   void paintCategoryNode(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
   void paintRoomsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const ;
   void paintContactsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
   void paintUserElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
   ChatUsersViewItemStyle itemStyle_;

   // QAbstractItemDelegate interface
public:
   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   ChatPartiesSortProxyModelPtr proxyModel_;
};
#endif // CHATCLIENTUSERSVIEWITEMDELEGATE_H

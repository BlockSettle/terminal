#ifndef CHATCLIENTUSERSVIEWITEMDELEGATE_H
#define CHATCLIENTUSERSVIEWITEMDELEGATE_H

#include <QStyledItemDelegate>
#include "ChatUsersViewItemStyle.h"

class ChatClientUsersViewItemDelegate : public QStyledItemDelegate
{
   Q_OBJECT
public:
   explicit ChatClientUsersViewItemDelegate(QObject *parent = nullptr);

signals:

public slots:

   // QAbstractItemDelegate interface
public:
   void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
protected:

   void paintPartyContainer(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
   void paintParty(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;


   template <typename AbstractPartySubClass>
   AbstractPartySubClass *checkAndGetInternalPointer(const QModelIndex &index) const;
   void paintCategoryNode(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
   void paintRoomsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const ;
   void paintContactsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
   void paintUserElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
   ChatUsersViewItemStyle itemStyle_;

   // QAbstractItemDelegate interface
public:
   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
#endif // CHATCLIENTUSERSVIEWITEMDELEGATE_H

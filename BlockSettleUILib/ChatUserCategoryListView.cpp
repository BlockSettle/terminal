#include "ChatUserCategoryListView.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QApplication>
#include <QFont>

ChatUserCategoryListViewDelegate::ChatUserCategoryListViewDelegate(QObject *parent)
   : QStyledItemDelegate (parent)
{

}

void ChatUserCategoryListViewDelegate::paint(QPainter *painter,
   const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   QStyledItemDelegate::paint(painter,option,index);

   painter->save();

   QFont font = QApplication::font();

   painter->restore();
}

ChatUserCategoryListView::ChatUserCategoryListView(QWidget *parent) : QListView(parent)
{
   setItemDelegate(new ChatUserCategoryListViewDelegate(this));
}

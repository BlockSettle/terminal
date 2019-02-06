#ifndef CHATUSERCATEGORYLISTVIEW_H
#define CHATUSERCATEGORYLISTVIEW_H

#include <QListView>
#include <QStyledItemDelegate>

class ChatUserCategoryListViewDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ChatUserCategoryListViewDelegate(QObject *parent = nullptr);

   void paint(QPainter *painter,
              const QStyleOptionViewItem &option,
              const QModelIndex &index) const override;
};

class ChatUserCategoryListView : public QListView
{
   Q_OBJECT
public:
   ChatUserCategoryListView(QWidget *parent = nullptr);

   using QListView::contentsSize;
};

#endif // CHATUSERCATEGORYLISTVIEW_H

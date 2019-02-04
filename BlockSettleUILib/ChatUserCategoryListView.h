#ifndef CHATUSERCATEGORYLISTVIEW_H
#define CHATUSERCATEGORYLISTVIEW_H

#include <QListView>

class ChatUserCategoryListView : public QListView
{
   Q_OBJECT
public:
   ChatUserCategoryListView(QWidget *parent = nullptr);

   using QListView::contentsSize;
};

#endif // CHATUSERCATEGORYLISTVIEW_H

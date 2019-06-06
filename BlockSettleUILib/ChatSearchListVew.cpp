#include "ChatSearchListVew.h"

#include <QKeyEvent>

ChatSearchListVew::ChatSearchListVew(QWidget *parent) : QTreeView(parent)
{
   setHeaderHidden(true);
   setRootIsDecorated(false);
   setSelectionMode(QAbstractItemView::SingleSelection);
   setContextMenuPolicy(Qt::CustomContextMenu);
}

void ChatSearchListVew::keyPressEvent(QKeyEvent *event)
{
   switch (event->key()) {
   case Qt::Key_Escape:
      emit leaveWithCloseRequired();
      break;
   case Qt::Key_Up:
      if (currentIndex().row() == 0) {
         emit leaveRequired();
      }
      break;
   default:
      break;
   }
   return QTreeView::keyPressEvent(event);
}

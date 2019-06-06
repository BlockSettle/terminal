#include "ChatSearchListVew.h"

ChatSearchListVew::ChatSearchListVew(QWidget *parent) : QTreeView(parent)
{
   setHeaderHidden(true);
   setRootIsDecorated(false);
   setSelectionMode(QAbstractItemView::SingleSelection);
   setContextMenuPolicy(Qt::CustomContextMenu);
}

#include "ChatTreeModelWrapper.h"

#include "ChatClientTree/TreeItem.h"

ChatTreeModelWrapper::ChatTreeModelWrapper(QObject *parent)
   : QSortFilterProxyModel(parent)
   , filteringRole_(-1)
{
}

bool ChatTreeModelWrapper::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
   if (!source_parent.isValid()) {
      return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
   }
   TreeItem *item = static_cast<TreeItem*>(source_parent.internalPointer());
   if (!item) {
      return false;
   }
   switch (item->getType()) {
   case ChatUIDefinitions::ChatTreeNodeType::RootNode:
   case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode: {
      return true;
   }
   default:
      return false;
   }
}

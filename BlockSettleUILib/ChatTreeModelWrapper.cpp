#include "ChatTreeModelWrapper.h"

#include "ChatClientTree/TreeItem.h"

ChatTreeModelWrapper::ChatTreeModelWrapper(QObject *parent) : QIdentityProxyModel(parent)
{
}

bool ChatTreeModelWrapper::hasChildren(const QModelIndex &parent) const
{
   return childrenVisible(parent) ? QIdentityProxyModel::hasChildren(parent) : false;
}

bool ChatTreeModelWrapper::childrenVisible(const QModelIndex &parent) const
{
   if (!parent.isValid()) {
      return true;
   }
   TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
   if (!item) {
      return false;
   }
   switch (item->getType()) {
   case ChatUIDefinitions::ChatTreeNodeType::RootNode:
   case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode:
      return true;
   default:
      return false;
   }
}

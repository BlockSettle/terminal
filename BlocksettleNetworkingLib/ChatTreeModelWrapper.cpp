#include "ChatTreeModelWrapper.h"

#include "ChatClientTree/TreeItem.h"

ChatTreeModelWrapper::ChatTreeModelWrapper(QObject *parent)
   : QSortFilterProxyModel(parent)
   , filteringRole_(-1)
{
   setDynamicSortFilter(true);
}

void ChatTreeModelWrapper::setSourceModel(QAbstractItemModel *sourceModel)
{
   if (this->sourceModel()) {
      disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged,
                 this, &ChatTreeModelWrapper::resetTree);
   }
   QSortFilterProxyModel::setSourceModel(sourceModel);
   if (sourceModel) {
      connect(this->sourceModel(), &QAbstractItemModel::dataChanged,
                       this, &ChatTreeModelWrapper::resetTree);
   }
}

bool ChatTreeModelWrapper::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
   auto index = sourceModel()->index(source_row, 0, source_parent);
   if (!index.isValid()) {
      return false;
   }
   TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
   if (!item) {
      return false;
   }
   switch (item->getType()) {
   case ChatUIDefinitions::ChatTreeNodeType::RootNode: {
      return true;
   }
   case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode:
      return item->getChildren().size() > 0;
   case ChatUIDefinitions::ChatTreeNodeType::SearchElement:
      return true;
   case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
   case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
   case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:
   case ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement:
   case ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement:
      return true;
   default:
      return false;
   }
}

void ChatTreeModelWrapper::resetTree()
{
   invalidateFilter();
   emit treeUpdated();
}

#include "ChatTreeModelWrapper.h"

#include "ChatClientTree/TreeItem.h"

ChatTreeModelWrapper::ChatTreeModelWrapper(QObject *parent)
   : QSortFilterProxyModel(parent)
{
   setDynamicSortFilter(true);
}

void ChatTreeModelWrapper::setFilterKey(const QString &pattern, int role, bool caseSensitive)
{
   setFilterFixedString(pattern);
   setFilterRole(role);
   setFilterCaseSensitivity(caseSensitive ? Qt:: CaseSensitive : Qt::CaseInsensitive);
   resetTree();
}

void ChatTreeModelWrapper::setSourceModel(QAbstractItemModel *sourceModel)
{
   if (this->sourceModel()) {
      disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged,
                 this, &ChatTreeModelWrapper::resetTree);
      disconnect(this->sourceModel(), &QAbstractItemModel::rowsInserted,
                 this, &ChatTreeModelWrapper::resetTree);
      disconnect(this->sourceModel(), &QAbstractItemModel::rowsRemoved,
                 this, &ChatTreeModelWrapper::resetTree);
      disconnect(this->sourceModel(), &QAbstractItemModel::modelReset,
                 this, &ChatTreeModelWrapper::resetTree);
   }
   QSortFilterProxyModel::setSourceModel(sourceModel);
   if (sourceModel) {
      connect(this->sourceModel(), &QAbstractItemModel::dataChanged,
                       this, &ChatTreeModelWrapper::resetTree);
      connect(this->sourceModel(), &QAbstractItemModel::rowsInserted,
                       this, &ChatTreeModelWrapper::resetTree);
      connect(this->sourceModel(), &QAbstractItemModel::rowsRemoved,
                       this, &ChatTreeModelWrapper::resetTree);
      connect(this->sourceModel(), &QAbstractItemModel::modelReset,
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
   case ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement: {
      return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
   }
   default:
      return false;
   }
}

void ChatTreeModelWrapper::resetTree()
{
   invalidateFilter();
   emit treeUpdated();
}

#include "ChatTreeModelWrapper.h"
#include "ChatClientDataModel.h"

#include "ChatProtocol/ChatUtils.h"

using NodeType = ChatUIDefinitions::ChatTreeNodeType;
using Role = ChatClientDataModel::Role;
using OnlineStatus = ChatContactElement::OnlineStatus;
using ContactStatus = Chat::ContactStatus;

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
   case NodeType::RootNode:
      return true;
   case NodeType::CategoryGroupNode:
      return item->getChildren().size() > 0;
   case NodeType::SearchElement:
      return true;
   case NodeType::RoomsElement:
   case NodeType::ContactsElement:
   case NodeType::ContactsRequestElement:
   case NodeType::AllUsersElement:
      return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
   default:
      return false;
   }
}

bool ChatTreeModelWrapper::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   auto leftNodeType = left.data(Role::ItemTypeRole).value<NodeType>();
   auto rightNodeType = right.data(Role::ItemTypeRole).value<NodeType>();

   // sort contact list by online status
   if (leftNodeType == NodeType::ContactsElement && rightNodeType == NodeType::ContactsElement) {

      auto leftContactStatus = left.data(Role::ContactStatusRole).value<ContactStatus>();
      auto leftOnlineStatus = left.data(Role::ContactOnlineStatusRole).value<OnlineStatus>();

      auto rightContactStatus = right.data(Role::ContactStatusRole).value<ContactStatus>();
      auto rightOnlineStatus = right.data(Role::ContactOnlineStatusRole).value<OnlineStatus>();

      if (leftContactStatus != rightContactStatus) {
         return leftContactStatus < rightContactStatus;
      }

      if (leftOnlineStatus != rightOnlineStatus) {
         return leftOnlineStatus < rightOnlineStatus;
      }
   }

   // sort category nodes alphabetically
   else if (leftNodeType == NodeType::CategoryGroupNode && rightNodeType == NodeType::CategoryGroupNode) {
      auto leftString = left.data(Role::CategoryGroupDisplayName).toString();
      auto rightString = right.data(Role::CategoryGroupDisplayName).toString();

      return QString::localeAwareCompare(leftString, rightString) < 0;
   }

   // sort room nodes alphabetically
   else if (leftNodeType == NodeType::RoomsElement && rightNodeType == NodeType::RoomsElement) {
      auto leftString = left.data(Role::RoomIdRole).toString();
      auto rightString = right.data(Role::RoomIdRole).toString();

      if (left.data(Role::RoomTitleRole).isValid()) {
         leftString = left.data(Role::RoomTitleRole).toString();
      }

      if (right.data(Role::RoomTitleRole).isValid()) {
         rightString = right.data(Role::RoomTitleRole).toString();
      }

      return QString::localeAwareCompare(leftString, rightString) < 0;
   }

   return QSortFilterProxyModel::lessThan(left, right);
}

void ChatTreeModelWrapper::resetTree()
{
   invalidateFilter();
   emit treeUpdated();
}
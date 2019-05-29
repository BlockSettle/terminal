#include "ChatTreeModelWrapper.h"
#include "ChatClientDataModel.h"

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
   case NodeType::RootNode: {
      return true;
   }
   case NodeType::CategoryGroupNode:
      return item->getChildren().size() > 0;
   case NodeType::SearchElement:
      return true;
   case NodeType::RoomsElement:
   case NodeType::ContactsElement:
   case NodeType::AllUsersElement:
   case NodeType::OTCReceivedResponsesElement:
   case NodeType::OTCSentResponsesElement: {
      return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
   }
   default:
      return false;
   }
}

bool ChatTreeModelWrapper::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   // sort contact list
   if (left.data(Role::ItemTypeRole).value<NodeType>() == NodeType::ContactsElement &&
       right.data(Role::ItemTypeRole).value<NodeType>() == NodeType::ContactsElement) {

      auto leftContactStatus = left.data(Role::ContactStatusRole).value<ContactStatus>();
      auto leftOnlineStatus = left.data(Role::ContactOnlineStatusRole).value<OnlineStatus>();

      auto rightContactStatus = right.data(Role::ContactStatusRole).value<ContactStatus>();
      auto rightOnlineStatus = right.data(Role::ContactOnlineStatusRole).value<OnlineStatus>();

      // contacts with online status are placed at the top of the list
      if (leftOnlineStatus == OnlineStatus::Online && leftContactStatus == ContactStatus::Accepted) {
         return false;
      }
      if (rightOnlineStatus == OnlineStatus::Online && rightContactStatus == ContactStatus::Accepted) {
         return true;
      }

      // contacts with offline status are placed at the middle of the list
      if (leftOnlineStatus == OnlineStatus::Offline && leftContactStatus == ContactStatus::Accepted) {
         return false;
      }
      if (rightOnlineStatus == OnlineStatus::Offline && rightContactStatus == ContactStatus::Accepted) {
         return true;
      }
      
      //contacts with incoming status are placed at the bottom of the list, but before outgoing
      if (leftContactStatus == ContactStatus::Incoming) {
         return false;
      }
      if (rightContactStatus == ContactStatus::Incoming) {
         return true;
      }
      
      //contacts with outgoing status are placed at the bottom of the list, but before rejected
      if (leftContactStatus == ContactStatus::Outgoing) {
         return false;
      }
      if (rightContactStatus == ContactStatus::Outgoing) {
         return true;
      }      
      
      //contacts with rejected status are placed at the bottom of the list,
      if (leftContactStatus == ContactStatus::Rejected) {
         return false;
      }
      if (rightContactStatus == ContactStatus::Rejected) {
         return true;
      }
   }

   return true;
}

void ChatTreeModelWrapper::resetTree()
{
   invalidateFilter();
   emit treeUpdated();
}

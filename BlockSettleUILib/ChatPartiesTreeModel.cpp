#include "ChatPartiesTreeModel.h"

namespace {
   const QString ContainerTabGlobal = QObject::tr("Global");
   const QString ContainerTabPrivate = QObject::tr("Private");
   const QString ContainerTabContactRequest = QObject::tr("Contact request");
}

ChatPartiesTreeModel::ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, QObject* parent)
   : QAbstractItemModel(parent),
   chatClientServicePtr_(chatClientServicePtr)
{
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatPartiesTreeModel::partyModelChanged);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatPartiesTreeModel::resetAll);

   rootItem_ = new PartyTreeItem({}, UI::ElementType::Root);
}

ChatPartiesTreeModel::~ChatPartiesTreeModel()
{
}

void ChatPartiesTreeModel::partyModelChanged()
{
   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();

   beginResetModel();

   rootItem_->removeAll();

   PartyTreeItem* globalSection = new PartyTreeItem(ContainerTabGlobal, UI::ElementType::Container, rootItem_);
   PartyTreeItem* privateSection = new PartyTreeItem(ContainerTabPrivate, UI::ElementType::Container, rootItem_);
   PartyTreeItem* requestSection = new PartyTreeItem(ContainerTabContactRequest, UI::ElementType::Container, rootItem_);

   Chat::IdPartyList idPartyList = clientPartyModelPtr->getIdPartyList();

   for (const auto& id : idPartyList)
   {
      Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(id);

      if (clientPartyPtr->partyType() == Chat::PartyType::GLOBAL)
      {
         QVariant stored;
         stored.setValue(clientPartyPtr);
         PartyTreeItem* globalItem = new PartyTreeItem(stored, UI::ElementType::Party, globalSection);
         globalSection->insertChildren(globalItem);
      }

      if (clientPartyPtr->partyType() == Chat::PartyType::PRIVATE_DIRECT_MESSAGE)
      {
         QVariant stored;
         stored.setValue(clientPartyPtr);

         PartyTreeItem* parentSection = clientPartyPtr->partyState() == Chat::PartyState::INITIALIZED ? privateSection : requestSection;

         PartyTreeItem* privateItem = new PartyTreeItem(stored, UI::ElementType::Party, parentSection);
         parentSection->insertChildren(privateItem);
      }
   }

   rootItem_->insertChildren(globalSection);
   rootItem_->insertChildren(privateSection);
   rootItem_->insertChildren(requestSection);

   endResetModel();
}

void ChatPartiesTreeModel::resetAll()
{
   beginResetModel();
   rootItem_->removeAll();
   endResetModel();
}

PartyTreeItem* ChatPartiesTreeModel::getItem(const QModelIndex& index) const
{
   if (index.isValid()) {
      PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());
      if (item)
      {
         return item;
      }
   }

   return rootItem_;
}

QVariant ChatPartiesTreeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid()) {
      return QVariant();
   }

   if (role != Qt::DisplayRole) {
      return QVariant();
   }

   PartyTreeItem* item = getItem(index);

   if (item->modelType() == UI::ElementType::Container) {
      return item->data();
   }
   else if (item->modelType() == UI::ElementType::Party) {
      Q_ASSERT(item->data().canConvert<Chat::ClientPartyPtr>());
      return QString::fromStdString(item->data().value<Chat::ClientPartyPtr>()->displayName());
   }

   return {};
}

QModelIndex ChatPartiesTreeModel::index(int row, int column, const QModelIndex& parent) const
{
   if (parent.isValid() && parent.column() != 0) {
      return QModelIndex();
   }

   if (!hasIndex(row, column, parent)) {
      return QModelIndex();
   }

   PartyTreeItem* parentItem = getItem(parent);
   Q_ASSERT(parentItem);

   PartyTreeItem* childItem = parentItem->child(row);
   if (childItem) {
      return createIndex(row, column, childItem);
   }

   return QModelIndex();
}

QModelIndex ChatPartiesTreeModel::parent(const QModelIndex& index) const
{
   if (!index.isValid())
      return QModelIndex();

   PartyTreeItem* childItem = getItem(index);
   PartyTreeItem* parentItem = childItem->parent();

   if (parentItem == rootItem_) {
      return QModelIndex();
   }

   return createIndex(parentItem->childNumber(), 0, parentItem);
}

int ChatPartiesTreeModel::rowCount(const QModelIndex& parent) const
{
   PartyTreeItem* parentItem = getItem(parent);

   return parentItem->childCount();
}

int ChatPartiesTreeModel::columnCount(const QModelIndex& parent) const
{
   return rootItem_->columnCount();
}

const std::string& ChatPartiesTreeModel::currentUser() const
{
   const auto chatModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   return chatModelPtr->ownUserName();
}

ChatPartiesSortProxyModel::ChatPartiesSortProxyModel(ChatPartiesTreeModelPtr&& sourceModel, QObject *parent /*= nullptr*/)
   : QSortFilterProxyModel(parent)
   , sourceModel_(std::move(sourceModel))
{
   setDynamicSortFilter(true);
   setSourceModel(sourceModel_.get());
}

PartyTreeItem* ChatPartiesSortProxyModel::getInternalData(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return {};
   }

   const auto& sourceIndex = mapToSource(index);
   return static_cast<PartyTreeItem*>(sourceIndex.internalPointer());
}

const std::string& ChatPartiesSortProxyModel::currentUser() const
{
   return sourceModel_->currentUser();
}

bool ChatPartiesSortProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
   Q_ASSERT(sourceModel_);

   auto index = sourceModel_->index(row, 0, parent);
   if (!index.isValid()) {
      return false;
   }

   PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());
   if (!item) {
      return false;
   }

   switch (item->modelType()) {
   case UI::ElementType::Party:
      return true;
   case UI::ElementType::Container:
      return item->childCount() > 0;
   default:
      return false;
   }
}

bool ChatPartiesSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   if (!left.isValid() || !right.isValid()) {
      return QSortFilterProxyModel::lessThan(left, right);
   }

   PartyTreeItem* itemLeft = static_cast<PartyTreeItem*>(left.internalPointer());
   PartyTreeItem* itemRight = static_cast<PartyTreeItem*>(right.internalPointer());

   if (!itemLeft || !itemRight) {
      return QSortFilterProxyModel::lessThan(left, right);
   }

   if (itemLeft->modelType() == itemRight->modelType()) {
      if (itemLeft->modelType() == UI::ElementType::Party) {
         Chat::ClientPartyPtr leftParty = itemLeft->data().value<Chat::ClientPartyPtr>();
         Chat::ClientPartyPtr rightParty = itemRight->data().value<Chat::ClientPartyPtr>();
         return leftParty->displayName() < rightParty->displayName();
      }
      else if (itemLeft->modelType() == UI::ElementType::Container) {
         QString leftContainerName = itemLeft->data().toString();
         QString rightContainerName = itemRight->data().toString();
         return leftContainerName < rightContainerName;
      }
   }

   return QSortFilterProxyModel::lessThan(left, right);
}

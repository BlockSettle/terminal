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

      if (clientPartyPtr->isGlobal()) {
         QVariant stored;
         stored.setValue(clientPartyPtr);
         PartyTreeItem* globalItem = new PartyTreeItem(stored, UI::ElementType::Party, globalSection);
         globalSection->insertChildren(globalItem);
      }

      if (clientPartyPtr->isPrivateStandard()) {
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

void ChatPartiesTreeModel::cleanModel()
{
   beginResetModel();
   rootItem_->removeAll();
   endResetModel();
}

void ChatPartiesTreeModel::partyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   const QModelIndex partyIndex = getPartyIndexById(clientPartyPtr->id());

   if (partyIndex.isValid()) {
      emit dataChanged(partyIndex, partyIndex);
   }
}

void ChatPartiesTreeModel::increaseUnseenCounter(const std::string& partyId, int newMessageCount)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->increaseUnreadedCounter(newMessageCount);

}

void ChatPartiesTreeModel::decreaseUnseenCounter(const std::string& partyId, int seenMessageCount)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->decreaseUnreadedCounter(seenMessageCount);
}

const QModelIndex ChatPartiesTreeModel::getPartyIndexById(const std::string& partyId) const
{
   for (int iContainer = 0; iContainer < rootItem_->childCount(); ++iContainer) {
      auto* container = rootItem_->child(iContainer);

      for (int iParty = 0; iParty < container->childCount(); ++iParty) {
         const PartyTreeItem* party = container->child(iParty);
         if (party->data().canConvert<Chat::ClientPartyPtr>()) {
            const Chat::ClientPartyPtr clientPtr = party->data().value<Chat::ClientPartyPtr>();
            if (clientPtr->id() == partyId) {
               return index(iParty, 0, index(iContainer, 0));
            }
         }
      }

      if (container->data().canConvert<QString>()) {
         if (container->data().toString().toStdString() == partyId) {
            return index(iContainer, 0);
         }
      }
   }

   return {};
}

PartyTreeItem* ChatPartiesTreeModel::getItem(const QModelIndex& index) const
{
   if (index.isValid()) {
      PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());
      if (item) {
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
   if (!index.isValid()) {
      return QModelIndex();
   }

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

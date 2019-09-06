#include "ChatPartiesTreeModel.h"

ChatPartiesTreeModel::ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, QObject* parent)
   : QAbstractItemModel(parent),
   chatClientServicePtr_(chatClientServicePtr)
{
   rootItem_ = new PartyTreeItem({}, UI::ElementType::Root);
}

ChatPartiesTreeModel::~ChatPartiesTreeModel()
{
}

void ChatPartiesTreeModel::onPartyModelChanged()
{
   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();

   beginResetModel();

   rootItem_->removeAll();

   std::unique_ptr<PartyTreeItem> globalSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabGlobal, UI::ElementType::Container, rootItem_);
   std::unique_ptr<PartyTreeItem> privateSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabPrivate, UI::ElementType::Container, rootItem_);
   std::unique_ptr<PartyTreeItem> requestSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabContactRequest, UI::ElementType::Container, rootItem_);

   Chat::IdPartyList idPartyList = clientPartyModelPtr->getIdPartyList();

   for (const auto& id : idPartyList) {
      Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(id);

      if (clientPartyPtr->isGlobal()) {
         QVariant stored;
         stored.setValue(clientPartyPtr);
         std::unique_ptr<PartyTreeItem> globalItem = std::make_unique<PartyTreeItem>(stored, UI::ElementType::Party, globalSection.get());
         globalSection->insertChildren(std::move(globalItem));
      }

      if (clientPartyPtr->isPrivateStandard()) {
         QVariant stored;
         stored.setValue(clientPartyPtr);

         PartyTreeItem* parentSection = clientPartyPtr->partyState() == Chat::PartyState::INITIALIZED ? privateSection.get() : requestSection.get();

         std::unique_ptr<PartyTreeItem> privateItem = std::make_unique<PartyTreeItem>(stored, UI::ElementType::Party, parentSection);
         parentSection->insertChildren(std::move(privateItem));
      }
   }

   rootItem_->insertChildren(std::move(globalSection));
   rootItem_->insertChildren(std::move(privateSection));
   rootItem_->insertChildren(std::move(requestSection));

   endResetModel();
}

void ChatPartiesTreeModel::onCleanModel()
{
   beginResetModel();
   rootItem_->removeAll();
   endResetModel();
}

void ChatPartiesTreeModel::onPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   const QModelIndex partyIndex = getPartyIndexById(clientPartyPtr->id());

   if (partyIndex.isValid()) {
      emit dataChanged(partyIndex, partyIndex);
   }
}

void ChatPartiesTreeModel::onIncreaseUnseenCounter(const std::string& partyId, int newMessageCount)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->increaseUnseenCounter(newMessageCount);

}

void ChatPartiesTreeModel::onDecreaseUnseenCounter(const std::string& partyId, int seenMessageCount)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->decreaseUnseenCounter(seenMessageCount);
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

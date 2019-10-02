#include "ChatPartiesTreeModel.h"

#include "OtcClient.h"

using namespace bs::network;

ChatPartiesTreeModel::ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, OtcClient *otcClient, QObject* parent)
   : QAbstractItemModel(parent)
   , chatClientServicePtr_(chatClientServicePtr)
   , otcClient_(otcClient)
{
   rootItem_ = new PartyTreeItem({}, UI::ElementType::Root);
}

ChatPartiesTreeModel::~ChatPartiesTreeModel() = default;

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
   emit restoreSelectedIndex();

   onGlobalOTCChanged();
}

void ChatPartiesTreeModel::onGlobalOTCChanged()
{
   QModelIndex otcModelIndex = getOTCGlobalRoot();
   if (!otcModelIndex.isValid()) {
      return;
   }
   PartyTreeItem* otcParty = static_cast<PartyTreeItem*>(otcModelIndex.internalPointer());

   if (otcParty->childCount() != 0) {
      beginRemoveRows(otcModelIndex, 0, otcParty->childCount() - 1);
      otcParty->removeAll();
      endRemoveRows();
   }

   auto fAddOtcParty = [](const bs::network::otc::Peer* peer, std::unique_ptr<PartyTreeItem>& section, otc::PeerType peerType) {
      Chat::ClientPartyPtr otcPartyPtr = std::make_shared<Chat::ClientParty>(peer->contactId,
         Chat::PartyType::PRIVATE_DIRECT_MESSAGE,
         Chat::PartySubType::OTC,
         Chat::PartyState::INITIALIZED);
      otcPartyPtr->setDisplayName(otcPartyPtr->id());

      QVariant stored;
      stored.setValue(otcPartyPtr);

      std::unique_ptr<PartyTreeItem> otcItem = std::make_unique<PartyTreeItem>(stored, UI::ElementType::Party, section.get());
      otcItem->peerType = peerType;
      section->insertChildren(std::move(otcItem));
   };

   beginInsertRows(otcModelIndex, 0, 1);

   std::unique_ptr<PartyTreeItem> sentSection = std::make_unique<PartyTreeItem>(ChatModelNames::TabOTCSentRequest, UI::ElementType::Container, otcParty);
   for (const auto &peer : otcClient_->requests()) {
      // Show only responded requests here
      if (peer->state != otc::State::Idle) {
         fAddOtcParty(peer, sentSection, otc::PeerType::Request);
      }
   }
   otcParty->insertChildren(std::move(sentSection));

   std::unique_ptr<PartyTreeItem> responseSection = std::make_unique<PartyTreeItem>(ChatModelNames::TabOTCReceivedResponse, UI::ElementType::Container, otcParty);
   for (const auto &peer : otcClient_->responses()) {
      fAddOtcParty(peer, responseSection, otc::PeerType::Response);
   }
   otcParty->insertChildren(std::move(responseSection));

   endInsertRows();
   emit restoreSelectedIndex();
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

const QModelIndex ChatPartiesTreeModel::getPartyIndexById(const std::string& partyId, const QModelIndex parent) const
{
   PartyTreeItem* parentItem = nullptr;
   if (parent.isValid()) {
      parentItem = static_cast<PartyTreeItem*>(parent.internalPointer());
   } 
   else {
      parentItem = rootItem_;
   }
   Q_ASSERT(parentItem);

   QList<QPair<QModelIndex, PartyTreeItem*>> itemsToCheck;
   itemsToCheck.push_back({ parent , parentItem });

   QPair<QModelIndex, PartyTreeItem*> currentPair;
   while (!itemsToCheck.isEmpty()) {
      currentPair = itemsToCheck[0];
      itemsToCheck.pop_front();

      QModelIndex iterModelIndex = currentPair.first;
      PartyTreeItem* iterItem= currentPair.second;


      for (int iChild = 0; iChild < iterItem->childCount(); ++iChild) {
         auto* child = iterItem->child(iChild);

         auto childIndex = index(iChild, 0, iterModelIndex);
         if (child->modelType() == UI::ElementType::Container && child->data().canConvert<QString>()) {
            if (child->data().toString().toStdString() == partyId) {
               return childIndex;
            }
         }
         else if (child->modelType() == UI::ElementType::Party && child->data().canConvert<Chat::ClientPartyPtr>()) {
            const Chat::ClientPartyPtr clientPtr = child->data().value<Chat::ClientPartyPtr>();
            if (clientPtr->id() == partyId) {
               return childIndex;
            }
         }

         if (child->childCount() != 0) {
            itemsToCheck.push_back({ childIndex , child });
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

QModelIndex ChatPartiesTreeModel::getOTCGlobalRoot() const
{
   for (int iContainer = 0; iContainer < rootItem_->childCount(); ++iContainer) {
      auto* container = rootItem_->child(iContainer);

      Q_ASSERT(container->data().canConvert<QString>());
      if (container->data().toString() != ChatModelNames::ContainerTabGlobal) {
         continue;
      }

      for (int iParty = 0; iParty < container->childCount(); ++iParty) {
         const PartyTreeItem* party = container->child(iParty);
         if (party->data().canConvert<Chat::ClientPartyPtr>()) {
            const Chat::ClientPartyPtr clientPtr = party->data().value<Chat::ClientPartyPtr>();
            if (clientPtr->displayName() == Chat::OtcRoomName) {
               return index(iParty, 0, index(iContainer, 0));
            }
         }
      }

      return {};
   }

   return {};
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

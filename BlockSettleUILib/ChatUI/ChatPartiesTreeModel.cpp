/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatPartiesTreeModel.h"

#include "OtcClient.h"

using namespace bs::network;

namespace {
   const auto kTogglingIntervalMs = std::chrono::milliseconds(250);
}

ChatPartiesTreeModel::ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, OtcClient *otcClient, QObject* parent)
   : QAbstractItemModel(parent)
   , chatClientServicePtr_(chatClientServicePtr)
   , otcClient_(otcClient)
{
   rootItem_ = new PartyTreeItem({}, UI::ElementType::Root);

   // #flickeringOTC Flickering otc awaiting messages - disabled for now
   //otcWatchToggling_.setInterval(kTogglingIntervalMs);
   //connect(&otcWatchToggling_, &QTimer::timeout, this, &ChatPartiesTreeModel::onUpdateOTCAwaitingColor);
   //otcWatchToggling_.start();
}

ChatPartiesTreeModel::~ChatPartiesTreeModel() = default;

void ChatPartiesTreeModel::onPartyModelChanged()
{
   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   
   // Save unseen state first
   const auto reusableItemData = collectReusableData(rootItem_);

   beginResetModel();

   rootItem_->removeAll();
   otcWatchIndx_.clear();

   std::unique_ptr<PartyTreeItem> globalSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabGlobal, UI::ElementType::Container, rootItem_);
   std::unique_ptr<PartyTreeItem> otcGlobalSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabOTCIdentifier, UI::ElementType::Container, rootItem_);
   std::unique_ptr<PartyTreeItem> privateSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabPrivate, UI::ElementType::Container, rootItem_);
   std::unique_ptr<PartyTreeItem> requestSection = std::make_unique<PartyTreeItem>(ChatModelNames::ContainerTabContactRequest, UI::ElementType::Container, rootItem_);

   auto insertChild = [](PartyTreeItem* section, QVariant stored) -> PartyTreeItem* {
      std::unique_ptr<PartyTreeItem> partyTreeItem = std::make_unique<PartyTreeItem>(stored, UI::ElementType::Party, section);
      PartyTreeItem* pTreeItem = partyTreeItem.get();
      section->insertChildren(std::move(partyTreeItem));
      return pTreeItem;
   };

   const auto idPartyList = clientPartyModelPtr->getIdPartyList();
   const auto clientPartyPtrList = clientPartyModelPtr->getClientPartyListFromIdPartyList(idPartyList);

   for (const auto& clientPartyPtr : clientPartyPtrList) {
      assert(clientPartyPtr);

      QVariant stored;
      stored.setValue(clientPartyPtr);

      if (clientPartyPtr->isGlobalOTC()) {
         insertChild(otcGlobalSection.get(), stored);
      }
      else if (clientPartyPtr->isGlobal()) {
         insertChild(globalSection.get(), stored);
      }
      else if (clientPartyPtr->isPrivateStandard()) {
         if (clientPartyPtr->partyState() == Chat::PartyState::REJECTED) {
            continue;
         }

         PartyTreeItem* parentSection = clientPartyPtr->partyState() == Chat::PartyState::INITIALIZED ? privateSection.get() : requestSection.get();
         PartyTreeItem* newChild = insertChild(parentSection, stored);

         assert(newChild);
         auto it = reusableItemData.find(clientPartyPtr->id());
         if (it != reusableItemData.end()) {
            newChild->applyReusableData(it.value());
         }
      }
   }

   rootItem_->insertChildren(std::move(globalSection));
   rootItem_->insertChildren(std::move(otcGlobalSection));
   rootItem_->insertChildren(std::move(privateSection));
   rootItem_->insertChildren(std::move(requestSection));

   endResetModel();
   emit restoreSelectedIndex();

   if (!reusableItemData.isEmpty()) {
      resetOTCUnseen({});
   }

   onGlobalOTCChanged(reusableItemData);
}

void ChatPartiesTreeModel::onGlobalOTCChanged(QMap<std::string, ReusableItemData> reusableItemData /* = {} */)
{
   QModelIndex otcGlobalModelIndex = getOTCGlobalRoot();
   if (!otcGlobalModelIndex.isValid()) {
      return;
   }

   PartyTreeItem* otcParty = static_cast<PartyTreeItem*>(otcGlobalModelIndex.internalPointer());
   if (reusableItemData.isEmpty()) {
      reusableItemData = collectReusableData(otcParty);
   }

   resetOTCUnseen(otcGlobalModelIndex, false, false);
   if (otcParty->childCount() > 0) {
      beginRemoveRows(otcGlobalModelIndex, 0, otcParty->childCount() - 1);
      otcParty->removeAll();
      endRemoveRows();
   }

   auto fAddOtcParty = [this, &reusableItemData](const bs::network::otc::Peer* peer, std::unique_ptr<PartyTreeItem>& section, otc::PeerType peerType) {
      Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
      Chat::ClientPartyPtr otcPartyPtr = clientPartyModelPtr->getOtcPartyForUsers(currentUser(), peer->contactId);
      if (!otcPartyPtr) {
         return;
      }
      QVariant stored;
      stored.setValue(otcPartyPtr);

      std::unique_ptr<PartyTreeItem> otcItem = std::make_unique<PartyTreeItem>(stored, UI::ElementType::Party, section.get());
      otcItem->peerType = peerType;

      auto it = reusableItemData.find(otcPartyPtr->id());
      if (it != reusableItemData.end()) {
         otcItem->applyReusableData(it.value());
      }

      section->insertChildren(std::move(otcItem));
   };

   beginInsertRows(otcGlobalModelIndex, 0, 1);

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
   if (!reusableItemData.isEmpty()) {
      resetOTCUnseen(otcGlobalModelIndex, true, false);
   }
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

void ChatPartiesTreeModel::onIncreaseUnseenCounter(const std::string& partyId, int newMessageCount, bool isUnseenOTCMessage /* = false */)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->increaseUnseenCounter(newMessageCount);

   if (isUnseenOTCMessage) {
      otcWatchIndx_.insert({ partyIndex });
      partyItem->enableOTCToggling(isUnseenOTCMessage);
   }
}

void ChatPartiesTreeModel::onDecreaseUnseenCounter(const std::string& partyId, int seenMessageCount)
{
   const QModelIndex partyIndex = getPartyIndexById(partyId);
   if (!partyIndex.isValid()) {
      return;
   }

   PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(partyIndex.internalPointer());
   partyItem->decreaseUnseenCounter(seenMessageCount);
   

   if (partyItem->isOTCTogglingMode()) {
      partyItem->enableOTCToggling(false);
      otcWatchIndx_.remove({ partyIndex });
   }
}

void ChatPartiesTreeModel::onUpdateOTCAwaitingColor()
{
   if (otcWatchIndx_.isEmpty()) {
      return;
   }

   for (const auto& index : otcWatchIndx_) {
      if (index.isValid()) {
         PartyTreeItem* partyItem = static_cast<PartyTreeItem*>(index.internalPointer());
         partyItem->changeOTCToggleState();

         emit dataChanged(index, index, { Qt::DecorationRole });
      }      
   }
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
            
            if (!clientPtr) {
               return {};
            }

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

void ChatPartiesTreeModel::forAllPartiesInModel(PartyTreeItem* parent,
   std::function<void(const PartyTreeItem*)>&& applyFunc) const
{
   QList<PartyTreeItem*> itemsToCheck;
   itemsToCheck.push_back(parent ? parent : rootItem_);

   while (!itemsToCheck.isEmpty()) {
      PartyTreeItem* item = itemsToCheck[0];
      itemsToCheck.pop_front();

      applyFunc(item);

      for (int i = 0; i < item->childCount(); ++i) {
         itemsToCheck.push_back(item->child(i));
      }
   }
}

void ChatPartiesTreeModel::forAllIndexesInModel(const QModelIndex& parentIndex,
   std::function<void(const QModelIndex&)>&& applyFunc) const
{
   PartyTreeItem* item = nullptr;
   if (!parentIndex.isValid()) {
      item = rootItem_;
   }
   else {
      item = static_cast<PartyTreeItem*>(parentIndex.internalPointer());
   }

   QList<QModelIndex> itemsToCheck;
   if (parentIndex.isValid()) {
      itemsToCheck.push_back(parentIndex);
   }
   else { // root item
      for (int i = 0; i < rootItem_->childCount(); ++i) {
         itemsToCheck.push_back(index(i, 0));
      }
   }

   while (!itemsToCheck.isEmpty()) {
      QModelIndex itemIndex = std::move(itemsToCheck[0]);
      itemsToCheck.pop_front();

      applyFunc(itemIndex);

      item = static_cast<PartyTreeItem*>(itemIndex.internalPointer());
      for (int i = 0; i < item->childCount(); ++i) {
         itemsToCheck.push_back(index(i, 0, itemIndex));
      }
   }
}

QMap<std::string, ReusableItemData> ChatPartiesTreeModel::collectReusableData(PartyTreeItem* parent)
{
   QMap<std::string, ReusableItemData> reusableData;
   forAllPartiesInModel(parent, [&](const PartyTreeItem* party) {
      if (party->modelType() != UI::ElementType::Party) {
         return;
      }

      const Chat::ClientPartyPtr clientPtr = party->data().value<Chat::ClientPartyPtr>();

      if (party->unseenCount() != 0) {
         reusableData.insert(clientPtr->id(), party->generateReusableData());
      }
   });

   return reusableData;
}

void ChatPartiesTreeModel::resetOTCUnseen(const QModelIndex& parentIndex,
   bool isAddChildren /*= true*/, bool isClearAll /*= true*/)
{
   if (isClearAll) {
      otcWatchIndx_.clear();
   }

   forAllIndexesInModel(parentIndex, [&](const QModelIndex& index) {
      PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());
      if (item->modelType() != UI::ElementType::Party) {
         return;
      }

      if (item->isOTCTogglingMode()) {
         if (isAddChildren) {
            otcWatchIndx_.insert({ index });
         }
         else {
            otcWatchIndx_.remove({ index });
         }
         
      }
   });
}

QModelIndex ChatPartiesTreeModel::getOTCGlobalRoot() const
{
   for (int iContainer = 0; iContainer < rootItem_->childCount(); ++iContainer) {
      auto* container = rootItem_->child(iContainer);

      Q_ASSERT(container->data().canConvert<QString>());
      if (container->data().toString() != ChatModelNames::ContainerTabOTCIdentifier) {
         continue;
      }

      for (int iParty = 0; iParty < container->childCount(); ++iParty) {
         const PartyTreeItem* party = container->child(iParty);
         if (party->data().canConvert<Chat::ClientPartyPtr>()) {
            const Chat::ClientPartyPtr clientPtr = party->data().value<Chat::ClientPartyPtr>();
            if (clientPtr->isGlobalOTC()) {
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
      if (item->data().toString() == ChatModelNames::ContainerTabOTCIdentifier) {
         return { ChatModelNames::ContainerTabOTCDisplayName };
      }

      return item->data();
   }
   else if (item->modelType() == UI::ElementType::Party) {
      Q_ASSERT(item->data().canConvert<Chat::ClientPartyPtr>());
      Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();
      if (!clientPartyPtr) {
         return QVariant();
      }

      if (clientPartyPtr->isGlobalOTC()) {
         return { ChatModelNames::PrivatePartyGlobalOTCDisplayName };
      }
      return QString::fromStdString(clientPartyPtr->displayName());
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

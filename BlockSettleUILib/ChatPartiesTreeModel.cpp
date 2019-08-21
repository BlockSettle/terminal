#include "ChatPartiesTreeModel.h"

/*
// Abstract Party
AbstractParty::AbstractParty(std::string const& displayName)
   : displayName_(displayName)
{
}

AbstractParty::~AbstractParty()
{
}

AbstractParty::AbstractParty(const AbstractParty& other)
{
   (*this) = other;
}

AbstractParty::AbstractParty(AbstractParty&& other)
{
   (*this) = std::move(other);
}

bool AbstractParty::operator==(const AbstractParty& other)
{
   return displayName_ == other.displayName_;
}

void AbstractParty::operator=(AbstractParty&& other)
{
   displayName_ = std::move(other.displayName_);
}

void AbstractParty::operator=(const AbstractParty& other)
{
   displayName_ = other.displayName_;
}

int AbstractParty::columnCount()
{
   return columnCount_;
}

const std::string& AbstractParty::getDisplayName() const
{
   return displayName_;
}

void AbstractParty::setDisplayName(const std::string& newName)
{
   displayName_ = newName;
}

// Party
Party::Party(const std::string& displayName, Chat::PartySubType subType, Chat::ClientStatus status)
   : AbstractParty(displayName)
   , subType_(subType)
   , status_(status)
{
}

Party::~Party()
{
}

Party::Party(const Party& other)
   : AbstractParty(other)
{
   *this = other;
}

Party::Party(Party&& other)
   : AbstractParty(other)
{
   *this = std::move(other);
}

bool Party::operator==(const Party& other)
{
   return static_cast<AbstractParty*>(this) == &other
      && status_ == other.status_
      && subType_ == other.subType_
      && parent_ == other.parent_
      ;
}

void Party::operator=(Party&& other)
{
   *(static_cast<AbstractParty*>(this)) = other;
   status_ = other.status_;
   subType_ = other.subType_;
   parent_ = other.parent_;
   other.parent_ = nullptr;
}

void Party::operator=(const Party& other)
{
   *(static_cast<AbstractParty*>(this)) = other;
   status_ = other.status_;
   subType_ = other.subType_;
   parent_ = other.parent_;
}

int Party::rowCount() const
{
   return rowCount_;
}

UI::ElementType Party::elementType() const
{
   return UI::ElementType::Party;
}

QVariant Party::data(UI::PartyRoles role) const
{
   if (UI::PartyRoles::Status == role) {
      return { static_cast<int>(status_) };
   }
   else if (UI::PartyRoles::Type == role) {
      return { parent_->data(role) };
   }
   else if (UI::PartyRoles::SubType == role) {
      return { static_cast<int>(subType_) };
   }
   else if (UI::PartyRoles::Name == role) {
      return { QString::fromStdString(displayName_) };
   }

   return {};
}

int Party::row() const
{
#ifndef QT_NO_DEBUG
   Q_ASSERT(parent_);
#endif
   if (!parent_) {
      return 0;
   }

   return parent_->childIndex(this);
}

AbstractParty* Party::parentItem()
{
   return parent_;
}

AbstractParty* Party::childItem(int row)
{
   Q_UNUSED(row);
   return nullptr;
}

int Party::childIndex(const AbstractParty* child) const
{
   Q_UNUSED(child);
   return 0;
}

Chat::ClientStatus Party::getClientStatus() const
{
   return status_;
}

void Party::setClientStatus(Chat::ClientStatus status)
{
   status_ = status;
}

Chat::PartySubType Party::getPartySubType() const
{
   return subType_;
}

void Party::setPartySubType(Chat::PartySubType subType)
{
   subType_ = subType;
}

void Party::setParent(PartyContainer* parent)
{
   parent_ = parent;
}


// PartyContainer

PartyContainer::PartyContainer(Chat::PartyType partyType, const std::string& displayName, QList<Party>&& children)
   : AbstractParty(displayName)
   , partyType_(partyType)
   , children_(std::move(children))
{
}

PartyContainer::~PartyContainer()
{
}

bool PartyContainer::operator==(const PartyContainer& other)
{
   return static_cast<AbstractParty*>(this) == &other
      && partyType_ == other.partyType_
      && children_ == other.children_;
   ;
}

int PartyContainer::rowCount() const
{
   return children_.size();
}

UI::ElementType PartyContainer::elementType() const
{
   return UI::ElementType::Container;
}

QVariant PartyContainer::data(UI::PartyRoles role) const
{
   if (UI::PartyRoles::Type == role) {
      return { static_cast<int>(partyType_) };
   }
   else if (UI::PartyRoles::Name == role) {
      return { QString::fromStdString(displayName_) };
   }

   return {};
}

int PartyContainer::row() const
{
   return static_cast<int>(partyType_);
}

AbstractParty* PartyContainer::parentItem()
{
   return nullptr;
}

AbstractParty* PartyContainer::childItem(int row)
{
   checkRow(row);
   if (row < 0 || row >= children_.size()) {
      return nullptr;
   }

   return &children_[row];
}

int PartyContainer::childIndex(const AbstractParty* child) const
{
   const Party* party = static_cast<const Party*>(child);
   return children_.indexOf(*party);
}

void PartyContainer::addParty(Party&& party)
{
   party.setParent(this);
   children_.push_back(std::move(party));
   //    std::sort(children_.begin(), children_.end(), [](const Party& left, const Party& right) {
   //        return left.getDisplayName() < right.getDisplayName();
   //    });
}

void PartyContainer::removeParty(int row)
{
   children_.removeAt(row);
}

void PartyContainer::changeParty(int row, Party&& newParty)
{
   checkRow(row);
   Party& party = children_[row];
   newParty.setParent(this);
   party = std::move(newParty);
}

void PartyContainer::replaceAllParties(QList<Party>&& newPartiesList)
{
   children_.clear();
   for (int iParty = 0; iParty < newPartiesList.size(); ++iParty) {
      newPartiesList[iParty].setParent(this);
   }
   children_ = std::move(newPartiesList);
}

void PartyContainer::checkRow(int row) const
{
   Q_UNUSED(row);
#ifndef QT_NO_DEBUG
   Q_ASSERT(row >= 0 && row < children_.size());
#endif
}

Chat::PartyType PartyContainer::getPartyType() const
{
   return partyType_;
}

void PartyContainer::setPartyType(Chat::PartyType newPartyType)
{
   partyType_ = newPartyType;
}

// ChatPartiesTreeModel

namespace {
   //void testModel(ChatPartiesTreeModel* model) {
      //ChatPartiesTreeModel::PartiesList resetList;

      //resetList.push_back({ Chat::PartyType::GLOBAL, {} });
      //auto& listPublic = resetList.back().second;
      //for (int i = 0; i < 10; ++i) {
      //   listPublic.push_back(Party(std::string("User_") + std::to_string(i),
      //      (i % 2) ? Chat::PartySubType::OTC : Chat::PartySubType::STANDARD,
      //      (i % 2) ? Chat::ClientStatus::ONLINE : Chat::ClientStatus::OFFLINE));
      //}

      //resetList.push_back({ Chat::PartyType::PRIVATE_DIRECT_MESSAGE, {} });
      //auto& listPrivate = resetList.back().second;
      //for (int i = 10; i < 20; ++i) {
      //   listPrivate.push_back(Party(std::string("User_") + std::to_string(i),
      //      (i % 2) ? Chat::PartySubType::OTC : Chat::PartySubType::STANDARD,
      //      (i % 2) ? Chat::ClientStatus::ONLINE : Chat::ClientStatus::OFFLINE));
      //}

      //model->replaceAllParties(resetList);

      //model->addParty(Chat::PartyType::GLOBAL,
      //   Party("UserExtra1", Chat::PartySubType::STANDARD, Chat::ClientStatus::ONLINE));
      //model->addParty(Chat::PartyType::GLOBAL,
      //   Party("UserExtra2", Chat::PartySubType::STANDARD, Chat::ClientStatus::OFFLINE));
      //model->addParty(Chat::PartyType::GLOBAL,
      //   Party("UserExtra3", Chat::PartySubType::OTC, Chat::ClientStatus::ONLINE));
      //model->addParty(Chat::PartyType::GLOBAL,
      //   Party("UserExtra4", Chat::PartySubType::OTC, Chat::ClientStatus::OFFLINE));
      //model->addParty(Chat::PartyType::PRIVATE_DIRECT_MESSAGE,
      //   Party("UserExtra5", Chat::PartySubType::STANDARD, Chat::ClientStatus::ONLINE));
      //model->addParty(Chat::PartyType::PRIVATE_DIRECT_MESSAGE,
      //   Party("UserExtra6", Chat::PartySubType::STANDARD, Chat::ClientStatus::OFFLINE));
      //model->addParty(Chat::PartyType::PRIVATE_DIRECT_MESSAGE,
      //   Party("UserExtra7", Chat::PartySubType::OTC, Chat::ClientStatus::ONLINE));
      //model->addParty(Chat::PartyType::PRIVATE_DIRECT_MESSAGE,
      //   Party("UserExtra8", Chat::PartySubType::OTC, Chat::ClientStatus::OFFLINE));

      //model->removeParty("UserExtra1");
      //model->removeParty("UserExtra3");
      //model->removeParty("UserExtra6");
      //model->removeParty("UserExtra8");

      //model->changeParty(Chat::PartyType::GLOBAL, Party("UserExtra2",
      //   Chat::PartySubType::STANDARD,
      //   Chat::ClientStatus::ONLINE));
      //model->changeParty(Chat::PartyType::PRIVATE_DIRECT_MESSAGE, Party("UserExtra4",
      //   Chat::PartySubType::OTC,
      //   Chat::ClientStatus::OFFLINE));
   //}
}

ChatPartiesTreeModel::ChatPartiesTreeModel(Chat::ChatClientServicePtr chatClientServicePtr, QObject* parent)
   : QAbstractItemModel(parent),
   chatClientServicePtr_(chatClientServicePtr),
   partyContainers_(new QList<PartyContainer>)
{
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatPartiesTreeModel::partyModelChanged);

   partyContainers_->reserve(static_cast<int>(Chat::PartyType_ARRAYSIZE));

   // Order matters
   partyContainers_->push_back(PartyContainer(Chat::PartyType::GLOBAL, "Global", {}));
   partyContainers_->push_back(PartyContainer(Chat::PartyType::PRIVATE_DIRECT_MESSAGE, "Private", {}));

   //testModel(this);
}

ChatPartiesTreeModel::~ChatPartiesTreeModel()
{
}

QVariant ChatPartiesTreeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid()) {
      return {};
   }

   AbstractParty* item = static_cast<AbstractParty*>(index.internalPointer());

   if (role == Qt::DisplayRole)
      return item->data(UI::PartyRoles::Name);

   return {};
}

QModelIndex ChatPartiesTreeModel::index(int row, int column, const QModelIndex& parent) const
{
   if (!hasIndex(row, column, parent))
      return QModelIndex();

   if (!parent.isValid()) {
      return createIndex(row, column, &(partyContainers_.get()->operator[](row)));
   }

   PartyContainer* container = static_cast<PartyContainer*>(parent.internalPointer());
#ifndef QT_NO_DEBUG
   Q_ASSERT(container);
#endif
   if (!container) {
      return {};
   }
   return createIndex(row, column, container->childItem(row));
}

QModelIndex ChatPartiesTreeModel::parent(const QModelIndex& index) const
{
   if (!index.isValid())
      return {};

   auto* children = static_cast<AbstractParty*>(index.internalPointer());
   auto* parent = children->parentItem();

   if (!parent) {
      return {};
   }

   return createIndex(parent->row(), 0, parent);
}

int ChatPartiesTreeModel::rowCount(const QModelIndex& parent) const
{
   if (parent.column() > 0) {
      return 0;
   }

   if (!parent.isValid()) {
      return static_cast<int>(Chat::PartyType_ARRAYSIZE);
   }

   return static_cast<AbstractParty*>(parent.internalPointer())->rowCount();
}

int ChatPartiesTreeModel::columnCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent);
   return AbstractParty::columnCount();
}

void ChatPartiesTreeModel::internalReplaceAllParties(PartiesList&& newParties)
{
   beginResetModel();

   for (int iType = 0; iType < static_cast<int>(Chat::PartyType_ARRAYSIZE); ++iType) {
      auto& container = partyContainers_.get()->operator[](static_cast<int>(iType));
      container.replaceAllParties({});
   }

   for (; newParties.size() > 0; newParties.pop_back()) {
      Chat::PartyType type = static_cast<Chat::PartyType>(newParties.size() - 1);
      QList<Party> list = std::move(newParties.back());

      checkType(type);

      auto& container = partyContainers_.get()->operator[](static_cast<int>(type));
      container.replaceAllParties(std::move(list));
   }

   endResetModel();
}

void ChatPartiesTreeModel::internalAddParty(Chat::PartyType type, Party&& party)
{
   checkType(type);
   auto& container = partyContainers_.get()->operator[](static_cast<int>(type));
   beginInsertRows(index(static_cast<int>(type), 0, QModelIndex()), container.rowCount(), container.rowCount());
   container.addParty(std::move(party));
   endInsertRows();
}

void ChatPartiesTreeModel::internalRemoveParty(const std::string& displayName)
{
   FindPartyResult result = findParty(displayName);
   if (!result.isValid()) {
      return;
   }

   beginRemoveRows(index(result.iContainer_, 0, {}), result.iParty_, result.iParty_);
   auto& container = partyContainers_.get()->operator[](result.iContainer_);
   container.removeParty(result.iParty_);
   endRemoveRows();
}

void ChatPartiesTreeModel::internalChangeParty(Chat::PartyType type, Party&& newParty)
{
   FindPartyResult result = findParty(newParty.getDisplayName());
   if (!result.isValid()) {
      return;
   }

   checkType(type);

   if (result.iContainer_ != static_cast<int>(type)) {
      QModelIndex sourceParent = index(result.iContainer_, 0, {});
      auto& sourceContainer = partyContainers_.get()->operator[](result.iContainer_);

      QModelIndex destParent = index(static_cast<int>(type), 0, {});
      auto& destContainer = partyContainers_.get()->operator[](static_cast<int>(type));

      beginMoveRows(sourceParent, result.iParty_, result.iParty_, destParent, destContainer.rowCount());
      sourceContainer.removeParty(result.iParty_);
      destContainer.addParty(std::move(newParty));
      endMoveRows();
      return;
   }

   auto& container = partyContainers_.get()->operator[](result.iContainer_);
   container.changeParty(result.iParty_, std::move(newParty));

   QModelIndex parentIndex = index(result.iContainer_, 0, {});
   QModelIndex childIndex = index(result.iParty_, 0, parentIndex);

   dataChanged(childIndex, childIndex);
}

//void ChatPartiesTreeModel::addParty(Chat::PartyType type, const Party& party)
//{
//   Party newParty = party;
//   internalAddParty(type, std::move(newParty));
//}
//
//void ChatPartiesTreeModel::removeParty(const std::string& displayName)
//{
//   internalRemoveParty(displayName);
//}
//
//void ChatPartiesTreeModel::changeParty(Chat::PartyType type, const Party& party)
//{
//   Party newParty = party;
//   internalChangeParty(type, std::move(newParty));
//}

void ChatPartiesTreeModel::partyModelChanged()
{
   PartiesList newParties;
   newParties.reserve(Chat::PartyType_ARRAYSIZE);
   for (int iPartiesType = Chat::PartyType_ARRAYSIZE; iPartiesType > 0; --iPartiesType) {
      newParties.push_back({});
   }

   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   Chat::IdPartyList idPartyList = clientPartyModelPtr->getIdPartyList();
   for (const auto& id : idPartyList)
   {
      Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(id);

      newParties[static_cast<int>(clientPartyPtr->partyType())].push_back(
         { clientPartyPtr->displayName(), clientPartyPtr->partySubType(), clientPartyPtr->clientStatus() } );
   }

   internalReplaceAllParties(std::move(newParties));
}

ChatPartiesTreeModel::FindPartyResult ChatPartiesTreeModel::findParty(const std::string& displayName) const
{
   int iContainer = 0, iParty = 0;
   for (iParty = 0; iContainer < static_cast<int>(Chat::PartyType_ARRAYSIZE); ++iContainer) {
      PartyContainer& container = partyContainers_.get()->operator[](iContainer);
      for (iParty = 0; iParty < container.rowCount(); ++iParty) {
         if (static_cast<Party*>(container.childItem(iParty))->getDisplayName() == displayName) {
            break;
         }
      }

      if (iParty < container.rowCount()) {
         break;
      }
   }

   if (iContainer >= static_cast<int>(Chat::PartyType_ARRAYSIZE)) {
      return {};
   }

   auto& container = partyContainers_.get()->operator[](iContainer);
   if (container.rowCount() <= iParty) {
      return {};
   }

   return { iContainer, iParty };
}

void ChatPartiesTreeModel::checkType(Chat::PartyType type) const
{
   Q_UNUSED(type);
#ifndef QT_NO_DEBUG
   Q_ASSERT(static_cast<int>(type) >= 0 && type < Chat::PartyType_ARRAYSIZE);
#endif
}

// Internal class
ChatPartiesTreeModel::FindPartyResult::FindPartyResult(int iContaner, int iParty)
   : iContainer_(iContaner)
   , iParty_(iParty)
{
}

ChatPartiesTreeModel::FindPartyResult::FindPartyResult()
{
}

bool ChatPartiesTreeModel::FindPartyResult::isValid() const
{
   return iContainer_ != FindPartyResult::iInvalid && iParty_ != FindPartyResult::iInvalid;
}
*/


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

   PartyTreeItem* globalSection = new PartyTreeItem(QString(QLatin1String("Global")), UI::ElementType::Container, rootItem_);
   PartyTreeItem* privateSection = new PartyTreeItem(QString(QLatin1String("Private")), UI::ElementType::Container, rootItem_);

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
         PartyTreeItem* privateItem = new PartyTreeItem(stored, UI::ElementType::Party, privateSection);
         privateSection->insertChildren(privateItem);
      }
   }

   rootItem_->insertChildren(globalSection);
   rootItem_->insertChildren(privateSection);

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

ChatPartiesSortProxyModel::ChatPartiesSortProxyModel(QObject *parent /*= nullptr*/)
   : QSortFilterProxyModel(parent)
{
   setDynamicSortFilter(true);
}

PartyTreeItem* ChatPartiesSortProxyModel::getInternalData(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return {};
   }

   const auto& sourceIndex = mapToSource(index);
   return static_cast<PartyTreeItem*>(sourceIndex.internalPointer());
}

bool ChatPartiesSortProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
   ChatPartiesTreeModel* source = static_cast<ChatPartiesTreeModel*>(sourceModel());
   Q_ASSERT(source);

   auto index = source->index(row, 0, parent);
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

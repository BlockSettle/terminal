#include "ChatPartiesTreeModel.h"

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
Party::Party(StateOfParty state, std::string displayName)
   : AbstractParty(displayName)
   , state_(state)
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
         && state_ == other.state_
         && parent_ == other.parent_
           ;
}

void Party::operator=(Party&& other)
{
    *(static_cast<AbstractParty*>(this)) = other;
    state_ = other.state_;
    parent_ = other.parent_;
    other.parent_ = nullptr;
}

void Party::operator=(const Party& other)
{
    *(static_cast<AbstractParty*>(this)) = other;
    state_ = other.state_;
    parent_ = other.parent_;
}

int Party::rowCount() const
{
   return rowCount_;
}

ElementType Party::elementType() const
{
   return ElementType::Party;
}

QVariant Party::data(PartyRoles role) const
{
   if (PartyRoles::State == role) {
      return { static_cast<int>(state_) };
   } else if (PartyRoles::Type == role) {
      return { parent_->data(role) };
   } else if (PartyRoles::Name == role) {
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

StateOfParty Party::getStateOfParty() const
{
    return state_;
}

void Party::setStateOfParty(StateOfParty state)
{
    state_ = state;
}

void Party::setParent(PartyContainer* parent)
{
    parent_ = parent;
}


// PartyContainer

PartyContainer::PartyContainer(PartyType partyType, std::string displayName, QList<Party>&& children)
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

ElementType PartyContainer::elementType() const
{
   return ElementType::Container;
}

QVariant PartyContainer::data(PartyRoles role) const
{
   if (PartyRoles::Type == role) {
      return { static_cast<int>(partyType_) };
   } else if (PartyRoles::Name == role) {
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

PartyType PartyContainer::getPartyType() const
{
    return partyType_;
}

void PartyContainer::setPartyType(PartyType newPartyType)
{
    partyType_ = newPartyType;
}

// ChatPartiesTreeModel

namespace  {
   void testModel(ChatPartiesTreeModel *model) {
      ChatPartiesTreeModel::PartiesList resetList;

      resetList.push_back({ PartyType::Public, {} });
      auto &listPublic = resetList.back().second;
      for (int i = 0; i < 10; ++i) {
          listPublic.push_back(Party(StateOfParty::Online, std::string("User%1") + std::to_string(i)));
      }

      resetList.push_back({ PartyType::Private, {} });
      auto &listPrivate = resetList.back().second;
      for (int i = 10; i < 20; ++i) {
          listPrivate.push_back(Party(StateOfParty::Online, std::string("User%1") + std::to_string(i)));
      }

      resetList.push_back({ PartyType::OTC, {} });
      auto &listOTC = resetList.back().second;
      for (int i = 20; i < 30; ++i) {
          listOTC.push_back(Party(StateOfParty::Online, std::string("User%1") + std::to_string(i)));
      }

      model->replaceAllParties(resetList);

      model->addParty(PartyType::Public, Party(StateOfParty::Online, "User1"));
      model->addParty(PartyType::Private, Party(StateOfParty::Offline, "User20"));
      model->addParty(PartyType::Private, Party(StateOfParty::Offline, "User21"));
      model->addParty(PartyType::Private, Party(StateOfParty::Offline, "User22"));
      model->addParty(PartyType::Private, Party(StateOfParty::Online, "User23"));
      model->addParty(PartyType::Private, Party(StateOfParty::Offline, "User24"));
      model->addParty(PartyType::OTC, Party(StateOfParty::Online, "User3"));

      model->removeParty("User1");
      model->removeParty("User20");
      model->removeParty("User21");
      model->removeParty("User23");

      model->changeParty(PartyType::Public, Party(StateOfParty::Online, "User22"));
      model->changeParty(PartyType::OTC, Party(StateOfParty::Online, "User23"));
      model->changeParty(PartyType::Private, Party(StateOfParty::Online, "User24"));
   }
}

ChatPartiesTreeModel::ChatPartiesTreeModel(QObject* parent)
   : QAbstractItemModel(parent)
   , partyContainers_(new QList<PartyContainer>)
{
   partyContainers_->reserve(static_cast<int>(PartyType::Total));

   // Order matters
   partyContainers_->push_back(PartyContainer(PartyType::Private, "Private", {}));
   partyContainers_->push_back(PartyContainer(PartyType::Public, "Public", {}));
   partyContainers_->push_back(PartyContainer(PartyType::OTC, "OTC", {}));

   testModel(this);
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
        return item->data(PartyRoles::Name);

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
      return static_cast<int>(PartyType::Total);
   }

   return static_cast<AbstractParty*>(parent.internalPointer())->rowCount();
}

int ChatPartiesTreeModel::columnCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent);
    return AbstractParty::columnCount();
}

void ChatPartiesTreeModel::internalReplaceAllParties(ChatPartiesTreeModel::PartiesList&& newParties)
{
    beginResetModel();
    for (int iType = 0; iType < static_cast<int>(PartyType::Total); ++iType) {
        auto &container = partyContainers_.get()->operator[](static_cast<int>(iType));
        container.replaceAllParties({});
    }

    for (;newParties.size() > 0; newParties.pop_back()) {
        auto pairTypeList = newParties.back();
        PartyType type = pairTypeList.first;
        QList<Party> list = std::move(pairTypeList.second);

        checkType(type);

        auto& container = partyContainers_.get()->operator[](static_cast<int>(type));
        container.replaceAllParties(std::move(list));
    }

    endResetModel();
}

void ChatPartiesTreeModel::internalAddParty(PartyType type, Party&& party)
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
    auto &container = partyContainers_.get()->operator[](result.iContainer_);
    container.removeParty(result.iParty_);
    endRemoveRows();
}

void ChatPartiesTreeModel::internalChangeParty(PartyType type, Party&& newParty)
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

void ChatPartiesTreeModel::addParty(PartyType type, const Party& party)
{
    Party newParty = party;
    internalAddParty(type, std::move(newParty));
}

void ChatPartiesTreeModel::removeParty(const std::string& displayName)
{
    internalRemoveParty(displayName);
}

void ChatPartiesTreeModel::changeParty(PartyType type, const Party& party)
{
    Party newParty = party;
    internalChangeParty(type, std::move(newParty));
}

void ChatPartiesTreeModel::replaceAllParties(const PartiesList& parties)
{
    PartiesList newParties = parties;
    internalReplaceAllParties(std::move(newParties));
}

ChatPartiesTreeModel::FindPartyResult ChatPartiesTreeModel::findParty(const std::string& displayName) const
{
    int iContainer = 0, iParty = 0;
    for (iParty = 0; iContainer < static_cast<int>(PartyType::Total); ++iContainer) {
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

    if (iContainer >= static_cast<int>(PartyType::Total)) {
        return {};
    }

    auto& container = partyContainers_.get()->operator[](iContainer);
    if (container.rowCount() <= iParty) {
        return {};
    }

    return { iContainer, iParty };
}

void ChatPartiesTreeModel::checkType(PartyType type) const
{
    Q_UNUSED(type);
#ifndef QT_NO_DEBUG
   Q_ASSERT(static_cast<int>(type) >= 0 && type < PartyType::Total);
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

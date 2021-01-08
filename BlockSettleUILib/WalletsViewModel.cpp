/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletsViewModel.h"

#include <QFont>
#include <QTreeView>
#include <QSortFilterProxyModel>

#include "SignContainer.h"
#include "UiUtils.h"
#include "ValidityFlag.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


void WalletNode::remove(WalletNode* child)
{
   bool found = false;
   for (auto& c : children_) {
      if (!found && (child->id() == c->id())) {
         found = true;
         continue;
      }
      if (found) {
         c->incRow(-1);
      }
   }
   children_.removeOne(child);
}

void WalletNode::replace(WalletNode *child)
{
   for (auto &c : children_) {
      if (child->id() == c->id()) {
         c = child;
         break;
      }
   }
}

void WalletNode::clear()
{
   qDeleteAll(children_);
   children_.clear();
}

WalletNode *WalletNode::child(int index) const
{
   return ((index >= nbChildren()) || (index < 0)) ? nullptr : children_[index];
}

WalletNode *WalletNode::findByWalletId(const std::string &walletId)
{
   if (id() == walletId) {
      return this;
   }
   for (const auto &child : children_) {
      auto node = child->findByWalletId(walletId);
      if (node != nullptr) {
         return node;
      }
   }
   return nullptr;
}

class WalletGroupNode;
class WalletRootNode : public WalletNode
{
public:
   WalletRootNode(WalletsViewModel *vm, const bs::sync::WalletInfo &wallet
      , const std::string &name, const std::string &desc, WalletNode::Type type, int row
      , WalletNode *parent, BTCNumericTypes::balance_type balTotal = 0
      , BTCNumericTypes::balance_type balUnconf = 0, BTCNumericTypes::balance_type balSpend = 0
      , size_t nbAddr = 0)
      : WalletNode(vm, type, row, parent)
      , hdWallet_(wallet), desc_(desc)
      , balTotal_(balTotal), balUnconf_(balUnconf), balSpend_(balSpend), nbAddr_(nbAddr)
   {
      name_ = name;
   }

   QVariant data(int col, int role) const override {
      if (role == Qt::DisplayRole) {
         if (viewModel_->showRegularWallets()) {
            switch (static_cast<WalletsViewModel::WalletRegColumns>(col)) {
            case WalletsViewModel::WalletRegColumns::ColumnName:
               return QString::fromStdString(name_);
            case WalletsViewModel::WalletRegColumns::ColumnDescription:
               return type() == Type::WalletRegular || type() == Type::WalletPrimary ? QString::fromStdString(desc_) : QVariant();
            case WalletsViewModel::WalletRegColumns::ColumnState:
               return getState();
            case WalletsViewModel::WalletRegColumns::ColumnNbAddresses:
               return nbAddr_ ? QString::number(nbAddr_) : QString();
            default:
               return QVariant();
            }
         }
         else {
            switch (static_cast<WalletsViewModel::WalletColumns>(col)) {
            case WalletsViewModel::WalletColumns::ColumnTotalBalance:
               return displayAmountOrLoading(balTotal_);
            case WalletsViewModel::WalletColumns::ColumnSpendableBalance:
               return displayAmountOrLoading(balSpend_);
            case WalletsViewModel::WalletColumns::ColumnUnconfirmedBalance:
               return displayAmount(balUnconf_);
            case WalletsViewModel::WalletColumns::ColumnName:
               return QString::fromStdString(name_);
            case WalletsViewModel::WalletColumns::ColumnDescription:
               return type() == Type::WalletRegular || type() == Type::WalletPrimary ? QString::fromStdString(desc_) : QVariant();
            case WalletsViewModel::WalletColumns::ColumnState:
               return getState();
            case WalletsViewModel::WalletColumns::ColumnNbAddresses:
               return nbAddr_ ? QString::number(nbAddr_) : QString();
            case WalletsViewModel::WalletColumns::ColumnID:
               return QString::fromStdString(id());
            default:
               return QVariant();
            }
         }
      }
      return QVariant();
   }

   std::string id() const override {
      return (!hdWallet_.ids.empty() ? hdWallet_.ids[0] : std::string{});
   }

   void setState(State state) override {
      WalletNode::setState(state);
      for (auto child : children_) {
         child->setState(state);
      }
   }

//   void addGroups(const std::vector<std::shared_ptr<bs::sync::hd::Group>> &groups);
   WalletGroupNode *addGroup(bs::core::wallet::Type, const std::string &name, const std::string &desc);

   std::vector<bs::sync::WalletInfo> wallets() const override
   {
      decltype(wallets_) ret = wallets_;

      for (const auto *g : qAsConst(children_)) {
         const auto tmp = g->wallets();
         ret.insert(ret.cend(), tmp.cbegin(), tmp.cend());
      }

      return ret;
   }
   BTCNumericTypes::balance_type getBalanceTotal() const { return balTotal_; }
   BTCNumericTypes::balance_type getBalanceUnconf() const { return balUnconf_; }
   BTCNumericTypes::balance_type getBalanceSpend() const { return balSpend_; }
   size_t getNbUsedAddresses() const { return nbAddr_; }
   bs::sync::WalletInfo hdWallet() const override { return hdWallet_; }

   void updateCounters(WalletRootNode *node) {
      if (!node) {
         return;
      }
      if (node->getBalanceTotal() > 0) {
         balTotal_.store(balTotal_.load() + node->getBalanceTotal());
      }
      if (node->getBalanceUnconf() > 0) {
         balUnconf_.store(balUnconf_.load() + node->getBalanceUnconf());
      }
      if (node->getBalanceSpend() > 0) {
         balSpend_.store(balSpend_.load() + node->getBalanceSpend());
      }
      if (type() != Type::GroupCC) {
         nbAddr_ += node->getNbUsedAddresses();
      }
   }

protected:
   std::string desc_;
   std::atomic<BTCNumericTypes::balance_type> balTotal_, balUnconf_, balSpend_;
   size_t      nbAddr_;
   bs::sync::WalletInfo hdWallet_;
   std::vector<bs::sync::WalletInfo> wallets_;

protected:
   QString displayAmountOrLoading(BTCNumericTypes::balance_type balance) const {
      if (balance < 0) {
         return QObject::tr("Loading...");
      }
      return displayAmount(balance);
   }

   QString displayAmount(BTCNumericTypes::balance_type balance) const {
      if (qFuzzyIsNull(balance) || (type_ == WalletNode::Type::GroupCC)) {
         return QString();
      }
      if (parent_->type() == WalletNode::Type::GroupCC) {
         return UiUtils::displayCCAmount(balance);
      }
      return UiUtils::displayAmount(balance);
   }

   QString getState() const {
      switch (state_) {
      case State::Primary:    return QObject::tr("Primary");
      case State::Full:       return QObject::tr("Full");
      case State::Offline:    return QObject::tr("Watching-Only");
      case State::Hardware:        return QObject::tr("Hardware");
      case State::Undefined:  return {};
      }
      return {};
   }

   static WalletNode::Type getNodeType(bs::core::wallet::Type grpType) {
      switch (grpType) {
      case bs::core::wallet::Type::Bitcoin:
         return Type::GroupBitcoin;

      case bs::core::wallet::Type::Authentication:
         return Type::GroupAuth;

      case bs::core::wallet::Type::ColorCoin:
         return Type::GroupCC;

      default:
         return Type::Unknown;
      }
   }
};

class WalletLeafNode : public WalletRootNode
{
public:
   WalletLeafNode(WalletsViewModel *vm, const bs::sync::WalletInfo &wallet
      , const bs::sync::WalletInfo &rootWallet, int row, WalletNode *parent)
      : WalletRootNode(vm, rootWallet, wallet.name, wallet.description, Type::Leaf, row, parent
         , 0, 0, 0, 0)
      , wallet_(wallet)
   {}

   void setBalances(const std::shared_ptr<bs::sync::Wallet> &wallet)
   {
      nbAddr_ = wallet->getUsedAddressCount();
      wallet->onBalanceAvailable([this, wallet, handle = validityFlag_.handle()]() mutable {
         ValidityGuard lock(handle);
         if (!handle.isValid()) {
            return;
         }
         balTotal_ = wallet->getTotalBalance();
         balUnconf_ = wallet->getUnconfirmedBalance();
         balSpend_ = wallet->getSpendableBalance();
      });
   }

   void setBalances(size_t nbAddr, BTCNumericTypes::balance_type total
      , BTCNumericTypes::balance_type spendable, BTCNumericTypes::balance_type unconfirmed)
   {
      nbAddr_ = nbAddr;
      balTotal_ = total;
      balSpend_ = spendable;
      balUnconf_ = unconfirmed;
   }

   std::vector<bs::sync::WalletInfo> wallets() const override
   {
      return { wallet_ };
   }

   std::string id() const override
   {
      return *wallet_.ids.cbegin();
   }

   QVariant data(int col, int role) const override {
      if (role == Qt::FontRole) {
         if (*wallet_.ids.cbegin() == viewModel_->selectedWallet()) {
            QFont font;
            font.setUnderline(true);
            return font;
         }
      }
      return WalletRootNode::data(col, role);
   }

private:
   bs::sync::WalletInfo wallet_;
   ValidityFlag validityFlag_;
};

class WalletGroupNode : public WalletRootNode
{
public:
   WalletGroupNode(WalletsViewModel *vm, const bs::sync::WalletInfo &hdWallet
      , const std::string &name, const std::string &desc, WalletNode::Type type
      , int row, WalletNode *parent)
      : WalletRootNode(vm, hdWallet, name, desc, type, row, parent) {}

   void setState(State state) override {
      for (auto child : children_) {
         child->setState(state);
      }
   }

   std::vector<bs::sync::WalletInfo> wallets() const override { return wallets_; }

   void addLeaf(const bs::sync::WalletInfo &leaf, const std::shared_ptr<bs::sync::hd::Leaf> &wallet) {
      if (viewModel_->showRegularWallets() && (leaf.type != bs::core::wallet::Type::Bitcoin
         || leaf.purpose == bs::hd::Purpose::NonSegWit)) {
         return;
      }
      const auto leafNode = new WalletLeafNode(viewModel_, leaf, hdWallet_, nbChildren(), this);
      leafNode->setBalances(wallet);
      add(leafNode);
      updateCounters(leafNode);
      wallets_.push_back(leaf);
   }

   void addLeaf(const bs::sync::HDWalletData::Leaf &leaf, bs::core::wallet::Type type) {
      if (viewModel_->showRegularWallets() && (type != bs::core::wallet::Type::Bitcoin
         || leaf.path.get(-2) == bs::hd::Purpose::NonSegWit)) {
         return;
      }
      bs::sync::WalletInfo wi;
      wi.format = bs::sync::WalletFormat::Plain;
      wi.ids = leaf.ids;
      wi.name = leaf.name;
      wi.description = leaf.description;
      wi.purpose = static_cast<bs::hd::Purpose>(leaf.path.get(-2));
      const auto leafNode = new WalletLeafNode(viewModel_, wi, hdWallet_, nbChildren(), this);
      add(leafNode);
      wallets_.push_back(wi);
   }
};

WalletGroupNode *WalletRootNode::addGroup(bs::core::wallet::Type type
   , const std::string &name, const std::string &desc)
{
   if (viewModel_->showRegularWallets() && (type != bs::core::wallet::Type::Bitcoin)) {
      return nullptr;
   }
   const auto groupNode = new WalletGroupNode(viewModel_, hdWallet_, name, desc
      , getNodeType(type), nbChildren(), this);
   add(groupNode);
   return groupNode;
}


WalletsViewModel::WalletsViewModel(const std::string &defaultWalletId
   , QObject* parent, bool showOnlyRegular)
   : QAbstractItemModel(parent)
   , defaultWalletId_(defaultWalletId)
   , showRegularWallets_(showOnlyRegular)
{
   rootNode_ = std::make_shared<WalletNode>(this, WalletNode::Type::Root);
}

WalletNode *WalletsViewModel::getNode(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return rootNode_.get();
   }
   return static_cast<WalletNode*>(index.internalPointer());
}

int WalletsViewModel::columnCount(const QModelIndex &) const
{
   return showRegularWallets_ ? static_cast<int>(WalletRegColumns::ColumnCount) : static_cast<int>(WalletColumns::ColumnCount);
}

int WalletsViewModel::rowCount(const QModelIndex &parent) const
{
   return getNode(parent)->nbChildren();
}

std::vector<bs::sync::WalletInfo> WalletsViewModel::getWallets(const QModelIndex &index) const
{
   const auto node = getNode(index);
   if (node == nullptr) {
      return {};
   }
   return node->wallets();
}

bs::sync::WalletInfo WalletsViewModel::getWallet(const QModelIndex &index) const
{
   const auto &wallets = getWallets(index);
   if (wallets.size() == 1) {
      return wallets[0];
   }
   return {};
}

QVariant WalletsViewModel::data(const QModelIndex &index, int role) const
{
   if (role == Qt::TextAlignmentRole) {
      if (showRegularWallets_) {
         switch (static_cast<WalletRegColumns>(index.column()))
         {
            case WalletRegColumns::ColumnNbAddresses:
               return Qt::AlignRight;
            default:
               return QVariant();
         }
      }

      switch (static_cast<WalletColumns>(index.column()))
      {
         case WalletColumns::ColumnSpendableBalance:
         case WalletColumns::ColumnUnconfirmedBalance:
         case WalletColumns::ColumnTotalBalance:
         case WalletColumns::ColumnNbAddresses:
            return Qt::AlignRight;
         default:
            return QVariant();
      }
   }

   return getNode(index)->data(index.column(), role);
}

QVariant WalletsViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      if (showRegularWallets_) {
         switch (static_cast<WalletRegColumns>(section)) {
         case WalletRegColumns::ColumnDescription:
            return tr("Description");
         case WalletRegColumns::ColumnName:
            return tr("Name");
         case WalletRegColumns::ColumnState:
            return tr("Signer state");
         case WalletRegColumns::ColumnNbAddresses:
            return tr("# Used Addresses");
         default:
            return QVariant();
         }
      }
      else {
         switch (static_cast<WalletColumns>(section)) {
         case WalletColumns::ColumnSpendableBalance:
            return tr("Spendable Balance");
         case WalletColumns::ColumnTotalBalance:
            return tr("Total Balance");
         case WalletColumns::ColumnUnconfirmedBalance:
            return tr("Unconfirmed Balance");
         case WalletColumns::ColumnDescription:
            return tr("Description");
         case WalletColumns::ColumnName:
            return tr("Name");
         case WalletColumns::ColumnState:
            return tr("Wallet type");
         case WalletColumns::ColumnNbAddresses:
            return tr("# Used Addresses");
         case WalletColumns::ColumnID:
            return tr("ID");
         default:
            return QVariant();
         }
      }
   }

   return QVariant();
}

QModelIndex WalletsViewModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent)) {
      return QModelIndex();
   }

   auto node = getNode(parent);
   auto child = node->child(row);
   if (child == nullptr) {
      return QModelIndex();
   }
   return createIndex(row, column, static_cast<void*>(child));
}

QModelIndex WalletsViewModel::parent(const QModelIndex &child) const
{
   if (!child.isValid()) {
      return QModelIndex();
   }

   auto node = getNode(child);
   auto parentNode = (node == nullptr) ? nullptr : node->parent();
   if ((parentNode == nullptr) || (parentNode == rootNode_.get())) {
      return QModelIndex();
   }
   return createIndex(parentNode->row(), 0, static_cast<void*>(parentNode));
}

bool WalletsViewModel::hasChildren(const QModelIndex& parent) const
{
   auto node = getNode(parent);
   return node->hasChildren();
}

Qt::ItemFlags WalletsViewModel::flags(const QModelIndex &index) const
{
   Qt::ItemFlags flags = QAbstractItemModel::flags(index);

   if (bitcoinLeafSelectionMode_) {
      WalletNode::Type nodeType = getNode(index)->type();

      if (nodeType != WalletNode::Type::Leaf) {
         return flags & ~Qt::ItemIsSelectable;
      }
   }
   return flags;
}

std::shared_ptr<bs::sync::Wallet> WalletsViewModel::getAuthWallet() const
{
   return walletsManager_->getAuthWallet();
}

static WalletNode::Type getHDWalletType(const std::shared_ptr<bs::sync::hd::Wallet> &hdWallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   if (walletsMgr->getPrimaryWallet() == hdWallet) {
      return WalletNode::Type::WalletPrimary;
   }
/*   if (walletsMgr->getDummyWallet() == hdWallet) {
      return WalletNode::Type::WalletDummy;
   }*/
   return WalletNode::Type::WalletRegular;
}

void WalletsViewModel::onWalletInfo(unsigned int id, bs::hd::WalletInfo)
{
   if (hdInfoReqIds_.empty() || (hdInfoReqIds_.find(id) == hdInfoReqIds_.end())) {
      return;
   }
   const auto walletId = hdInfoReqIds_[id];
   hdInfoReqIds_.erase(id);
   const auto state = WalletNode::State::Full;
   signerStates_[walletId] = state;
   for (int i = 0; i < rootNode_->nbChildren(); i++) {
      auto hdNode = rootNode_->child(i);
      if (hdNode->id() == walletId) {
         hdNode->setState(state);
      }
   }
}

void WalletsViewModel::onHDWalletError(unsigned int id, std::string)
{
   if (hdInfoReqIds_.empty() || (hdInfoReqIds_.find(id) == hdInfoReqIds_.end())) {
      return;
   }
   const auto walletId = hdInfoReqIds_[id];
   hdInfoReqIds_.erase(id);
   const auto state = WalletNode::State::Undefined;
   signerStates_[walletId] = state;
   for (int i = 0; i < rootNode_->nbChildren(); i++) {
      auto hdNode = rootNode_->child(i);
      if (hdNode->id() == walletId) {
         hdNode->setState(state);
      }
   }
}

void WalletsViewModel::onSignerAuthenticated()
{
   for (const auto &hdWallet : walletsManager_->hdWallets()) {
      if (!hdWallet) {
         continue;
      }
      const auto walletId = hdWallet->walletId();
      hdInfoReqIds_[signContainer_->GetInfo(walletId)] = walletId;
   }
}

void WalletsViewModel::onNewWalletAdded(const std::string &walletId)
{
   if (!signContainer_) {
      return;
   }
   hdInfoReqIds_[signContainer_->GetInfo(walletId)] = walletId;
}

void WalletsViewModel::onHDWallet(const bs::sync::WalletInfo &wi)
{
   const auto &wallet = rootNode_->findByWalletId(*wi.ids.cbegin());
   int row = wallet ? wallet->row() : rootNode_->nbChildren();
   const auto hdNode = new WalletRootNode(this, wi, wi.name, wi.description
      , wi.primary ? WalletNode::Type::WalletPrimary : WalletNode::Type::WalletRegular
      , row, rootNode_.get());
   if (wallet) {
      rootNode_->replace(hdNode);
      emit dataChanged(createIndex(row, 0, static_cast<void*>(rootNode_.get()))
         , createIndex(row, (int)WalletColumns::ColumnCount - 1, static_cast<void*>(rootNode_.get())));
   }
   else {
      beginInsertRows(QModelIndex()
         , rootNode_->nbChildren(), rootNode_->nbChildren());
      rootNode_->add(hdNode);
      endInsertRows();
   }
   emit needHDWalletDetails(*wi.ids.cbegin());
}

void WalletsViewModel::onWalletDeleted(const bs::sync::WalletInfo& wi)
{
   const auto& wallet = rootNode_->findByWalletId(*wi.ids.cbegin());
   if (wallet) {
      beginRemoveRows(QModelIndex(), wallet->row(), wallet->row());
      rootNode_->remove(wallet);
      endRemoveRows();
   }
}

void WalletsViewModel::onHDWalletDetails(const bs::sync::HDWalletData &hdWallet)
{
   WalletNode *node{ nullptr };
   for (int i = 0; i < rootNode_->nbChildren(); ++i) {
      if (rootNode_->child(i)->id() == hdWallet.id) {
         node = rootNode_->child(i);
         break;
      }
   }
   const auto &hdNode = dynamic_cast<WalletRootNode *>(node);
   if (!node || !hdNode) {
      return;
   }
   for (const auto &group : hdWallet.groups) {
      if (group.type == bs::hd::CoinType::BlockSettle_Settlement) {
         continue;
      }
      WalletGroupNode *groupNode{ nullptr };
      for (int i = 0; i < node->nbChildren(); ++i) {
         const auto &child = node->child(i);
         if (child->name() == group.name) {
            groupNode = dynamic_cast<WalletGroupNode *>(child);
            break;
         }
      }
      bs::core::wallet::Type groupType{ bs::core::wallet::Type::Unknown };
      switch (group.type) {
      case bs::hd::CoinType::Bitcoin_main:
      case bs::hd::CoinType::Bitcoin_test:
         groupType = bs::core::wallet::Type::Bitcoin;
         break;
      case bs::hd::CoinType::BlockSettle_Auth:
         groupType = bs::core::wallet::Type::Authentication;
         break;
      case bs::hd::CoinType::BlockSettle_CC:
         groupType = bs::core::wallet::Type::ColorCoin;
         break;
      default: break;
      }
      if (!groupNode) {
         const auto& nbChildren = node->nbChildren();
         groupNode = hdNode->addGroup(groupType, group.name, group.description);
         if (groupNode) {
            beginInsertRows(createIndex(node->row(), 0, static_cast<void*>(node))
               , nbChildren, nbChildren);
            endInsertRows();
         }
      }
      if (!groupNode) {
         continue;
      }
      for (const auto &leaf : group.leaves) {
         const auto &leafNode = groupNode->findByWalletId(*leaf.ids.cbegin());
         if (!leafNode) {
            beginInsertRows(createIndex(groupNode->row(), 0, static_cast<void*>(groupNode))
               , groupNode->nbChildren(), groupNode->nbChildren());
            groupNode->addLeaf(leaf, groupType);
            endInsertRows();
         }
         emit needWalletBalances(*leaf.ids.cbegin());
      }
   }
}

void WalletsViewModel::onWalletBalances(const bs::sync::WalletBalanceData &wbd)
{
   auto node = rootNode_->findByWalletId(wbd.id);
   auto leafNode = dynamic_cast<WalletLeafNode *>(node);
   if (!node || !leafNode) {
      return;
   }
   leafNode->setBalances(wbd.nbAddresses, wbd.balTotal, wbd.balSpendable, wbd.balUnconfirmed);
   node = leafNode->parent();
   emit dataChanged(createIndex(leafNode->row(), (int)WalletColumns::ColumnSpendableBalance, static_cast<void*>(node))
      , createIndex(leafNode->row(), (int)WalletColumns::ColumnNbAddresses, static_cast<void*>(node)));

   auto groupNode = dynamic_cast<WalletGroupNode *>(node);
   if (!groupNode) {
      return;
   }
   groupNode->updateCounters(leafNode);
   emit dataChanged(createIndex(groupNode->row(), (int)WalletColumns::ColumnSpendableBalance, static_cast<void*>(groupNode->parent()))
      , createIndex(leafNode->row(), (int)WalletColumns::ColumnNbAddresses, static_cast<void*>(groupNode->parent())));
}

void WalletsViewModel::LoadWallets(bool keepSelection)
{
   const auto treeView = qobject_cast<QTreeView *>(QObject::parent());
   std::string selectedWalletId;
   if (keepSelection && (treeView != nullptr)) {
      selectedWalletId = "empty";
      const auto sel = treeView->selectionModel()->selectedRows();
      if (!sel.empty()) {
         const auto fltModel = qobject_cast<QSortFilterProxyModel *>(treeView->model());
         const auto index = fltModel ? fltModel->mapToSource(sel[0]) : sel[0];
         const auto node = getNode(index);
         if (node != nullptr) {
            const auto &wallets = node->wallets();
            if (wallets.size() == 1) {
               selectedWalletId = *wallets[0].ids.cbegin();
            }
         }
      }
   }

   beginResetModel();
   rootNode_->clear();
   for (const auto &hdWallet : walletsManager_->hdWallets()) {
      if (!hdWallet) {
         continue;
      }
      const auto hdNode = new WalletRootNode(this, bs::sync::WalletInfo::fromWallet(hdWallet)
         , hdWallet->name(), hdWallet->description()
         , getHDWalletType(hdWallet, walletsManager_), rootNode_->nbChildren(), rootNode_.get());
      rootNode_->add(hdNode);

      // filter groups
      // don't display Settlement
      for (const auto &group : hdWallet->getGroups()) {
         if (group->type() == bs::core::wallet::Type::Settlement) {
            continue;
         }
         auto groupNode = hdNode->addGroup(group->type(), group->name(), group->description());
         if (groupNode) {
            for (const auto &leaf : group->getLeaves()) {
               groupNode->addLeaf(bs::sync::WalletInfo::fromLeaf(leaf), leaf);
            }
         }
      }
      if (signContainer_) {
         if (signContainer_->isOffline()) {
            hdNode->setState(WalletNode::State::Offline);
         }
         else if (hdWallet->isHardwareWallet()) {
            hdNode->setState(WalletNode::State::Hardware);
         }
         else if (signContainer_->isWalletOffline(hdWallet->walletId())) {
            hdNode->setState(WalletNode::State::Offline);
         }
         else if (hdWallet->isPrimary()) {
            hdNode->setState(WalletNode::State::Primary);
         } else {
            hdNode->setState(WalletNode::State::Full);
         }
      }
   }

/*   const auto stmtWallet = walletsManager_->getSettlementWallet();
   if (!showRegularWallets() && (stmtWallet != nullptr)) {
      const auto stmtNode = new WalletLeafNode(this, stmtWallet, rootNode_->nbChildren(), rootNode_.get());
      rootNode_->add(stmtNode);
   }*/   //TODO: add later if decided
   endResetModel();

   QModelIndexList selection;
   if (selectedWalletId.empty()) {
      selectedWalletId = defaultWalletId_;
   }
   auto node = rootNode_->findByWalletId(selectedWalletId);
   if (node != nullptr) {
      selection.push_back(createIndex(node->row(), 0, static_cast<void*>(node)));
   }
   else if(rootNode_->hasChildren()) {
      node = rootNode_->child(0);
      selection.push_back(createIndex(node->row(), 0, static_cast<void*>(node)));
   }
   
   if (treeView != nullptr) {
      for (int i = 0; i < rowCount(); i++) {
         treeView->expand(index(i, 0));
         // Expand XBT leaves
         treeView->expand(index(0, 0, index(i, 0)));
      }

      if (!selection.empty()) {
         treeView->setCurrentIndex(selection[0]);
         treeView->selectionModel()->select(selection[0], QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         treeView->expand(selection[0]);
         treeView->scrollTo(selection[0]);
      }
   }
   emit updateAddresses();
}

void WalletsViewModel::onWalletChanged()
{
   LoadWallets(true);
}

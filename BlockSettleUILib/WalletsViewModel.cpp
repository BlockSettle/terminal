#include "WalletsViewModel.h"
#include <QFont>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "SignContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"


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
   if ((type() == Type::Leaf) && (wallets()[0] != nullptr) && (wallets()[0]->walletId() == walletId)) {
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

class WalletRootNode : public WalletNode
{
public:
   WalletRootNode(WalletsViewModel *vm, const std::shared_ptr<bs::sync::hd::Wallet> &wallet
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
      return (hdWallet_ ? hdWallet_->walletId() : std::string{});
   }

   void setState(State state) override {
      WalletNode::setState(state);
      for (auto child : children_) {
         child->setState(state);
      }
   }

   void addGroups(const std::vector<std::shared_ptr<bs::sync::hd::Group>> &groups);

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override
   {
      std::vector<std::shared_ptr<bs::sync::Wallet>> ret = wallets_;

      for (const auto * g : qAsConst(children_)) {
         const auto tmp = g->wallets();
         ret.insert(ret.end(), tmp.cbegin(), tmp.cend());
      }

      return ret;
   }
   BTCNumericTypes::balance_type getBalanceTotal() const { return balTotal_; }
   BTCNumericTypes::balance_type getBalanceUnconf() const { return balUnconf_; }
   BTCNumericTypes::balance_type getBalanceSpend() const { return balSpend_; }
   size_t getNbUsedAddresses() const { return nbAddr_; }
   std::shared_ptr<bs::sync::hd::Wallet> hdWallet() const override { return hdWallet_; }

protected:
   std::string desc_;
   BTCNumericTypes::balance_type balTotal_, balUnconf_, balSpend_;
   size_t      nbAddr_;
   std::shared_ptr<bs::sync::hd::Wallet>           hdWallet_;
   std::vector<std::shared_ptr<bs::sync::Wallet>>  wallets_;

protected:
   void updateCounters(WalletRootNode *node) {
      if (!node) {
         return;
      }
      if (node->getBalanceTotal() > 0) {
         balTotal_ += node->getBalanceTotal();
      }
      if (node->getBalanceUnconf() > 0) {
         balUnconf_ += node->getBalanceUnconf();
      }
      if (node->getBalanceSpend() > 0) {
         balSpend_ += node->getBalanceSpend();
      }
      if (type() != Type::GroupCC) {
         nbAddr_ += node->getNbUsedAddresses();
      }
   }

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
      case State::Connected:     return QObject::tr("Full");
      case State::Offline:       return QObject::tr("Watching-Only");
      default:    return {};
      }
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
   WalletLeafNode(WalletsViewModel *vm, const std::shared_ptr<bs::sync::Wallet> &wallet
      , const std::shared_ptr<bs::sync::hd::Wallet> &rootWallet, int row, WalletNode *parent)
      : WalletRootNode(vm, rootWallet, wallet->shortName(), wallet->description(), Type::Leaf, row, parent
         , 0, 0, 0, wallet->getUsedAddressCount())
      , wallet_(wallet)
      , isValidFlag_(std::make_shared<bool>())
   {
      wallet->onBalanceAvailable([this, wallet, isValid = std::weak_ptr<void>(isValidFlag_)] {
         if (!isValid.lock()) {
            return;
         }
         balTotal_ = wallet->getTotalBalance();
         balUnconf_ = wallet->getUnconfirmedBalance();
         balSpend_ = wallet->getSpendableBalance();
      });
   }

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override { return {wallet_}; }

   std::string id() const override {
      return wallet_->walletId();
   }

   QVariant data(int col, int role) const override {
      if (role == Qt::FontRole) {
         if (wallet_ == viewModel_->selectedWallet()) {
            QFont font;
            font.setUnderline(true);
            return font;
         }
      }
      return WalletRootNode::data(col, role);
   }

private:
   std::shared_ptr<bs::sync::Wallet>   wallet_;
   std::shared_ptr<void> isValidFlag_;
};

class WalletGroupNode : public WalletRootNode
{
public:
   WalletGroupNode(WalletsViewModel *vm, const std::shared_ptr<bs::sync::hd::Wallet> &hdWallet
      , const std::string &name, const std::string &desc, WalletNode::Type type
      , int row, WalletNode *parent)
      : WalletRootNode(vm, hdWallet, name, desc, type, row, parent) {}

   void setState(State state) override {
      for (auto child : children_) {
         child->setState(state);
      }
   }

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override { return wallets_; }

   void addLeaves(const std::vector<std::shared_ptr<bs::sync::Wallet>> &leaves) {
      for (const auto &leaf : leaves) {
         if (viewModel_->showRegularWallets() && (leaf->type() != bs::core::wallet::Type::Bitcoin)) {
            continue;
         }
         const auto leafNode = new WalletLeafNode(viewModel_, leaf, hdWallet_, nbChildren(), this);
         add(leafNode);
         updateCounters(leafNode);
         wallets_.push_back(leaf);
      }
   }
};

void WalletRootNode::addGroups(const std::vector<std::shared_ptr<bs::sync::hd::Group>> &groups)
{
   for (const auto &group : groups) {
      if (viewModel_->showRegularWallets() && (group->type() != bs::core::wallet::Type::Bitcoin)) {
         continue;
      }
      const auto groupNode = new WalletGroupNode(viewModel_, hdWallet_, group->name(), group->description()
         , getNodeType(group->type()), nbChildren(), this);
      add(groupNode);
      groupNode->addLeaves(group->getAllLeaves());
   }
}


WalletsViewModel::WalletsViewModel(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::string &defaultWalletId, const std::shared_ptr<SignContainer> &container
   , QObject* parent, bool showOnlyRegular)
   : QAbstractItemModel(parent)
   , walletsManager_(walletsManager)
   , signContainer_(container)
   , defaultWalletId_(defaultWalletId)
   , showRegularWallets_(showOnlyRegular)
{
   rootNode_ = std::make_shared<WalletNode>(this, WalletNode::Type::Root);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, [this](std::string) { onWalletChanged(); });
   connect(walletsManager_.get(), &bs::sync::WalletsManager::blockchainEvent, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::newWalletAdded, this, &WalletsViewModel::onNewWalletAdded);

   if (signContainer_) {
      connect(signContainer_.get(), &SignContainer::QWalletInfo, this, &WalletsViewModel::onWalletInfo);
      connect(signContainer_.get(), &SignContainer::Error, this, &WalletsViewModel::onHDWalletError);
      connect(signContainer_.get(), &SignContainer::authenticated, this, &WalletsViewModel::onSignerAuthenticated);
      connect(signContainer_.get(), &SignContainer::ready, this, &WalletsViewModel::onWalletChanged);
   }
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

std::vector<std::shared_ptr<bs::sync::Wallet>> WalletsViewModel::getWallets(const QModelIndex &index) const
{
   const auto node = getNode(index);
   if (node == nullptr) {
      return {};
   }
   return node->wallets();
}

std::shared_ptr<bs::sync::Wallet> WalletsViewModel::getWallet(const QModelIndex &index) const
{
   const auto &wallets = getWallets(index);
   if (wallets.size() == 1) {
      return wallets[0];
   }
   return nullptr;
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
            return tr("# Used Addrs");
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
            return tr("# Used Addrs");
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
   const auto state = WalletNode::State::Connected;
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
   for (unsigned int i = 0; i < walletsManager_->hdWalletsCount(); i++) {
      const auto &hdWallet = walletsManager_->getHDWallet(i);
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
               selectedWalletId = wallets[0]->walletId();
            }
         }
      }
   }

   const auto hdCount = walletsManager_->hdWalletsCount();
   beginResetModel();
   rootNode_->clear();
   for (unsigned int i = 0; i < hdCount; i++) {
      const auto &hdWallet = walletsManager_->getHDWallet(i);
      if (!hdWallet) {
         continue;
      }
      const auto hdNode = new WalletRootNode(this, hdWallet, hdWallet->name(), hdWallet->description()
         , getHDWalletType(hdWallet, walletsManager_), rootNode_->nbChildren(), rootNode_.get());
      rootNode_->add(hdNode);

      // filter groups
      // don't display Settlement
      auto groups = hdWallet->getGroups();
      std::vector<std::shared_ptr<bs::sync::hd::Group>> filteredGroups;

      std::copy_if(groups.begin(), groups.end(), std::back_inserter(filteredGroups),
         [](const std::shared_ptr<bs::sync::hd::Group>& item)
         { return item->type() != bs::core::wallet::Type::Settlement; }
      );

      hdNode->addGroups(filteredGroups);
      if (signContainer_) {
         if (signContainer_->isOffline() || signContainer_->isWalletOffline(hdWallet->walletId())) {
            hdNode->setState(WalletNode::State::Offline);
         }
         else {
            hdNode->setState(WalletNode::State::Connected);
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
   const auto node = rootNode_->findByWalletId(selectedWalletId);
   if (node != nullptr) {
      selection.push_back(createIndex(node->row(), 0, static_cast<void*>(node)));
   }
   
   if (treeView != nullptr) {
      for (int i = 0; i < rowCount(); i++) {
         treeView->expand(index(i, 0));
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

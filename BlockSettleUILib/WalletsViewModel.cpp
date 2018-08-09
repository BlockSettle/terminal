#include "WalletsViewModel.h"
#include <QFont>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "SignContainer.h"
#include "WalletsManager.h"
#include "HDWallet.h"
#include "UiUtils.h"


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
   if ((type() == Type::Leaf) && (wallets()[0] != nullptr) && (wallets()[0]->GetWalletId() == walletId)) {
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
   WalletRootNode(WalletsViewModel *vm, const std::string &name, const std::string &desc, WalletNode::Type type, int row, WalletNode *parent
      , BTCNumericTypes::balance_type balTotal = 0, BTCNumericTypes::balance_type balUnconf = 0
      , BTCNumericTypes::balance_type balSpend = 0, size_t nbAddr = 0)
      : WalletNode(vm, type, row, parent), desc_(desc)
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
               return QString::fromStdString(desc_);
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
               return QString::fromStdString(desc_);
            case WalletsViewModel::WalletColumns::ColumnState:
               return getState();
            case WalletsViewModel::WalletColumns::ColumnNbAddresses:
               return nbAddr_ ? QString::number(nbAddr_) : QString();
            default:
               return QVariant();
            }
         }
      }
      return QVariant();
   }

   std::string id() const override {
      return (hdWallet_ ? hdWallet_->getWalletId() : std::string{});
   }

   void setState(State state) override {
      WalletNode::setState(state);
      for (auto child : children_) {
         child->setState(state);
      }
   }

   void addGroups(const std::vector<std::shared_ptr<bs::hd::Group>> &groups);

   std::vector<std::shared_ptr<bs::Wallet>> wallets() const override { return wallets_; }
   BTCNumericTypes::balance_type getBalanceTotal() const { return balTotal_; }
   BTCNumericTypes::balance_type getBalanceUnconf() const { return balUnconf_; }
   BTCNumericTypes::balance_type getBalanceSpend() const { return balSpend_; }
   size_t getNbUsedAddresses() const { return nbAddr_; }
   std::shared_ptr<bs::hd::Wallet> hdWallet() const override { return hdWallet_; }

   void setHdWallet(const std::shared_ptr<bs::hd::Wallet> &hdWallet) {
      hdWallet_ = hdWallet;
   }

protected:
   std::string desc_;
   BTCNumericTypes::balance_type balTotal_, balUnconf_, balSpend_;
   size_t      nbAddr_;
   std::shared_ptr<bs::hd::Wallet>           hdWallet_;
   std::vector<std::shared_ptr<bs::Wallet>>  wallets_;

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
      case State::Connected:     return QObject::tr("Connected");
      case State::Offline:       return QObject::tr("Offline");
      default:    return {};
      }
   }

   static WalletNode::Type getNodeType(bs::wallet::Type grpType) {
      switch (grpType) {
      case bs::wallet::Type::Bitcoin:
         return Type::GroupBitcoin;

      case bs::wallet::Type::Authentication:
         return Type::GroupAuth;

      case bs::wallet::Type::ColorCoin:
         return Type::GroupCC;

      default:
         return Type::Unknown;
      }
   }
};

class WalletLeafNode : public WalletRootNode
{
public:
   WalletLeafNode(WalletsViewModel *vm, const std::shared_ptr<bs::Wallet> &wallet, int row, WalletNode *parent)
      : WalletRootNode(vm, wallet->GetShortName(), wallet->GetWalletDescription(), Type::Leaf, row, parent
         , wallet->GetTotalBalance(), wallet->GetUnconfirmedBalance(), wallet->GetSpendableBalance()
         , wallet->GetUsedAddressCount())
      , wallet_(wallet)
   { }

   std::vector<std::shared_ptr<bs::Wallet>> wallets() const override { return {wallet_}; }

   std::string id() const override {
      return wallet_->GetWalletId();
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
   std::shared_ptr<bs::Wallet>   wallet_;
};

class WalletGroupNode : public WalletRootNode
{
public:
   WalletGroupNode(WalletsViewModel *vm, const std::string &name, const std::string &desc, WalletNode::Type type
      , int row, WalletNode *parent)
      : WalletRootNode(vm, name, desc, type, row, parent) {}

   void setState(State state) override {
      for (auto child : children_) {
         child->setState(state);
      }
   }

   void addLeaves(const std::vector<std::shared_ptr<bs::Wallet>> &leaves) {
      for (const auto &leaf : leaves) {
         if (viewModel_->showRegularWallets() && (leaf == viewModel_->getAuthWallet())) {
            continue;
         }
         const auto leafNode = new WalletLeafNode(viewModel_, leaf, nbChildren(), this);
         add(leafNode);
         updateCounters(leafNode);
         wallets_.push_back(leaf);
      }
   }
};

void WalletRootNode::addGroups(const std::vector<std::shared_ptr<bs::hd::Group>> &groups)
{
   for (const auto &group : groups) {
      if (viewModel_->showRegularWallets()) {
         if ((group->getType() == bs::wallet::Type::Authentication)
            || (group->getType() == bs::wallet::Type::ColorCoin)) {
            continue;
         }
      }
      const auto groupNode = new WalletGroupNode(viewModel_, group->getName(), group->getDesc(), getNodeType(group->getType()), nbChildren(), this);
      add(groupNode);
      groupNode->addLeaves(group->getAllLeaves());
   }
}


WalletsViewModel::WalletsViewModel(const std::shared_ptr<WalletsManager> &walletsManager
   , const std::string &defaultWalletId, const std::shared_ptr<SignContainer> &container
   , QObject* parent, bool showOnlyRegular)
   : QAbstractItemModel(parent)
   , walletsManager_(walletsManager)
   , signContainer_(container)
   , defaultWalletId_(defaultWalletId)
   , showRegularWallets_(showOnlyRegular)
{
   rootNode_ = std::make_shared<WalletNode>(this, WalletNode::Type::Root);
   connect(walletsManager_.get(), &WalletsManager::walletsReady, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &WalletsManager::walletChanged, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &WalletsManager::blockchainEvent, this, &WalletsViewModel::onWalletChanged);
   connect(walletsManager_.get(), &WalletsManager::newWalletAdded, this, &WalletsViewModel::onNewWalletAdded);

   if (signContainer_) {
      connect(signContainer_.get(), &SignContainer::HDWalletInfo, this, &WalletsViewModel::onHDWalletInfo);
      connect(signContainer_.get(), &SignContainer::MissingWallets, this, &WalletsViewModel::onMissingWallets);
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

std::vector<std::shared_ptr<bs::Wallet>> WalletsViewModel::getWallets(const QModelIndex &index) const
{
   const auto node = getNode(index);
   if (node == nullptr) {
      return {};
   }
   return node->wallets();
}

std::shared_ptr<bs::Wallet> WalletsViewModel::getWallet(const QModelIndex &index) const
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
      else {
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
      return QVariant();
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
            return tr("Signer state");
         case WalletColumns::ColumnNbAddresses:
            return tr("# Used Addrs");
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

std::shared_ptr<bs::Wallet> WalletsViewModel::getAuthWallet() const
{
   return walletsManager_->GetAuthWallet();
}

static WalletNode::Type getHDWalletType(const std::shared_ptr<bs::hd::Wallet> &hdWallet, const std::shared_ptr<WalletsManager> &walletsMgr)
{
   if (walletsMgr->GetPrimaryWallet() == hdWallet) {
      return WalletNode::Type::WalletPrimary;
   }
   if (walletsMgr->GetDummyWallet() == hdWallet) {
      return WalletNode::Type::WalletDummy;
   }
   return WalletNode::Type::WalletRegular;
}

void WalletsViewModel::onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType, const SecureBinaryData &)
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

void WalletsViewModel::onMissingWallets(const std::vector<std::string> &ids)
{
   for (const auto &id : ids) {
      auto node = rootNode_->findByWalletId(id);
      if (node) {
         node->setState(WalletNode::State::Offline);
      }
      failedLeaves_.insert(id);
   }
}

void WalletsViewModel::onSignerAuthenticated()
{
   for (unsigned int i = 0; i < walletsManager_->GetHDWalletsCount(); i++) {
      const auto &hdWallet = walletsManager_->GetHDWallet(i);
      if (!hdWallet) {
         continue;
      }
      hdInfoReqIds_[signContainer_->GetInfo(hdWallet)] = hdWallet->getWalletId();
   }
}

void WalletsViewModel::onNewWalletAdded(const std::string &walletId)
{
   if (!signContainer_) {
      return;
   }
   const auto &hdWallet = walletsManager_->GetHDWalletById(walletId);
   if (hdWallet) {
      hdInfoReqIds_[signContainer_->GetInfo(hdWallet)] = walletId;
   }
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
               selectedWalletId = wallets[0]->GetWalletId();
            }
         }
      }
   }

   const auto hdCount = walletsManager_->GetHDWalletsCount();
   beginResetModel();
   rootNode_->clear();
   for (unsigned int i = 0; i < hdCount; i++) {
      const auto &hdWallet = walletsManager_->GetHDWallet(i);
      if (!hdWallet) {
         continue;
      }
      const auto hdNode = new WalletRootNode(this, hdWallet->getName(), hdWallet->getDesc()
         , getHDWalletType(hdWallet, walletsManager_), rootNode_->nbChildren(), rootNode_.get());
      hdNode->setHdWallet(hdWallet);
      rootNode_->add(hdNode);
      hdNode->addGroups(hdWallet->getGroups());
      if (signContainer_) {
         if ((signContainer_->opMode() == SignContainer::OpMode::Offline) || signContainer_->isOffline()) {
            hdNode->setState(WalletNode::State::Offline);
         }
         else {
            const auto stateIt = signerStates_.find(hdWallet->getWalletId());
            if (stateIt != signerStates_.end()) {
               hdNode->setState(stateIt->second);
            }
         }
      }
   }
   for (const auto &failedLeaf : failedLeaves_) {
      auto leaf = rootNode_->findByWalletId(failedLeaf);
      if (leaf) {
         leaf->setState(WalletNode::State::Offline);
      }
   }

   const auto &stmtWallet = walletsManager_->GetSettlementWallet();
   if (!showRegularWallets() && (stmtWallet != nullptr)) {
      const auto stmtNode = new WalletLeafNode(this, stmtWallet, rootNode_->nbChildren(), rootNode_.get());
      rootNode_->add(stmtNode);
   }
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
}

void WalletsViewModel::onWalletChanged()
{
   LoadWallets(true);
}


QVariant QmlWalletsViewModel::data(const QModelIndex &index, int role) const
{
   if (index.isValid() && role >= firstRole) {
      const auto &node = getNode(index);
      if (!node) {
         return tr("invalid node");
      }
      auto hdWallet = node->hdWallet();
      if (node->type() == WalletNode::Type::Leaf) {
         auto parent = node->parent();
         if (parent) {
            parent = parent->parent();
         }
         hdWallet = parent ? parent->hdWallet() : nullptr;
      }

      switch (role) {
      case NameRole:       return node->data(static_cast<int>(WalletRegColumns::ColumnName), Qt::DisplayRole);
      case DescRole:       return node->data(static_cast<int>(WalletRegColumns::ColumnDescription), Qt::DisplayRole);
      case WalletIdRole:   return QString::fromStdString(node->id());
      case IsEncryptedRole: return hdWallet ? static_cast<int>(hdWallet->encryptionType()) : 0;
      case EncKeyRole:     return hdWallet ? QString::fromStdString(hdWallet->encryptionKey().toBinStr()) : QString();
      case StateRole:
         if ((node->type() != WalletNode::Type::Leaf)) {
            return {};
         }
         switch (hdWallet ? hdWallet->encryptionType() : bs::wallet::EncryptionType::Unencrypted) {
         case bs::wallet::EncryptionType::Password:   return tr("Password");
         case bs::wallet::EncryptionType::Freja:   return tr("Freja eID");
         default:    return tr("No");
         }
      case RootWalletIdRole:  return hdWallet ? QString::fromStdString(hdWallet->getWalletId()) : QString();
      case IsHDRootRole:   return ((node->type() == WalletNode::Type::WalletPrimary)
                                 || (node->type() == WalletNode::Type::WalletRegular));
      default:    break;
      }
   }
   return WalletsViewModel::data(index, role);
}

QHash<int, QByteArray> QmlWalletsViewModel::roleNames() const
{
   return QHash<int, QByteArray> {
      { NameRole, QByteArrayLiteral("name") },
      { DescRole, QByteArrayLiteral("desc") },
      { StateRole, QByteArrayLiteral("state") },
      { WalletIdRole, QByteArrayLiteral("walletId") },
      { IsHDRootRole, QByteArrayLiteral("isHdRoot") },
      { RootWalletIdRole, QByteArrayLiteral("rootWalletId") },
      { IsEncryptedRole, QByteArrayLiteral("isEncrypted") },
      { EncKeyRole, QByteArrayLiteral("encryptionKey") }
   };
}

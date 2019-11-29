/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCPortfolioModel.h"

#include "AssetManager.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <QFont>

#include <stdexcept>
#include <unordered_map>

class AssetNode
{
public:
   AssetNode(const QString& name, const QString& nodeId, AssetNode* parent)
      : name_{name}
      , id_{nodeId}
      , parent_{parent}
      , row_{-1}
   {}

   virtual ~AssetNode() noexcept = default;

   AssetNode(const AssetNode&) = delete;
   AssetNode& operator = (const AssetNode&) = delete;

   AssetNode(AssetNode&&) = delete;
   AssetNode& operator = (AssetNode&&) = delete;

   bool isRoot() const { return parent_ == nullptr; }

   AssetNode* getParent() const { return parent_; }
   int getRow() const { return row_; }
   void setRow( const int row) { row_ = row; }

   virtual AssetNode* getChild(int row) const { return nullptr; }

public:
   virtual bool HasChildren() const {
      return false;
   }

   virtual int childrenCount() const { return 0; }

   QString GetName() const {
      return name_;
   }

   QString GetNodeId() const {
      return id_;
   }

   virtual bool HasBalance() const {
      return true;
   }
   virtual QString GetBalance() const = 0;

   virtual bool HasXBTValue() const = 0;
   virtual QString GetXBTValueString() const = 0;

   virtual double GetXBTAmount() const = 0;

private:
   const QString  name_;
   const QString  id_;
   AssetNode*     parent_;
   int            row_;
};

//==============================================================================
// Specific group nodes. Difference in balance representation

class XBTAssetNode : public AssetNode
{
public:
   XBTAssetNode(const QString& walletName, const QString& walletId, AssetNode* parent)
      : AssetNode(walletName, walletId, parent) {}
   ~XBTAssetNode() noexcept override = default;

   XBTAssetNode(const XBTAssetNode&) = delete;
   XBTAssetNode& operator = (const XBTAssetNode&) = delete;

   XBTAssetNode(XBTAssetNode&&) = delete;
   XBTAssetNode& operator = (XBTAssetNode&&) = delete;

public:
   bool SetXBTAmount(double amount) {
      if (amount_ != amount) {
         amount_ = amount;
         return true;
      }

      return false;
   }

public:
   bool HasBalance() const override {
      return false;
   }

   bool HasXBTValue() const override {
      return true;
   }

   QString GetBalance() const override { return QString{}; }
   QString GetXBTValueString() const override { return UiUtils::displayAmount(amount_);}

   double GetXBTAmount() const override { return amount_; }

private:
   double amount_ = -1;
};


class FXAssetNode : public AssetNode
{
public:
   FXAssetNode(const QString& name, AssetNode* parent)
      : AssetNode(name, name, parent) {}
   ~FXAssetNode() noexcept override = default;

   FXAssetNode(const FXAssetNode&) = delete;
   FXAssetNode& operator = (const FXAssetNode&) = delete;

   FXAssetNode(FXAssetNode&&) = delete;
   FXAssetNode& operator = (FXAssetNode&&) = delete;

public:
   bool SetFXAmount(double amount) {
      if (amount_ != amount) {
         amount_ = amount;
         return true;
      }

      return false;
   }

   // set to 0, and XBT value will be empty
   bool SetPrice(double price) {
      if (price_ != price) {
         price_ = price;
         return true;
      }

      return false;
   }

public:
   bool HasBalance() const override {
      return true;
   }

   bool HasXBTValue() const override {
      return !qFuzzyIsNull(price_) && !qFuzzyIsNull(amount_);
   }

   QString GetBalance() const override { return UiUtils::displayCurrencyAmount(amount_); }

   QString GetXBTValueString() const override
   {
      return HasXBTValue()
         ? UiUtils::displayAmount( GetXBTAmount() )
         : QString{};
   }

   // price is inverted in AssetManager
   double GetXBTAmount() const override { return amount_ * price_; }

private:
   double amount_ = 0;
   double price_ = 0;
};


class CCAssetNode : public AssetNode
{
public:
   CCAssetNode(const QString& name, AssetNode* parent)
      : AssetNode(name, name, parent) {}
   ~CCAssetNode() noexcept override = default;

   CCAssetNode(const CCAssetNode&) = delete;
   CCAssetNode& operator = (const CCAssetNode&) = delete;

   CCAssetNode(CCAssetNode&&) = delete;
   CCAssetNode& operator = (CCAssetNode&&) = delete;

public:
   bool SetCCAmount(double amount) {
      if (amount_ != amount) {
         amount_ = amount;
         return true;
      }

      return false;
   }

   // set to 0, and XBT value will be empty
   bool SetPrice(double price) {
      if (price_ != price) {
         price_ = price;
         return true;
      }

      return false;
   }

public:
   bool HasBalance() const override {
      return true;
   }

   bool HasXBTValue() const override {
      return !qFuzzyIsNull(price_);
   }

   QString GetBalance() const override { return UiUtils::displayCCAmount(amount_); }

   QString GetXBTValueString() const override
   {
      return HasXBTValue()
         ? UiUtils::displayAmount(GetXBTAmount())
         : QString{};
   }

   double GetXBTAmount() const override { return amount_ * price_; }

private:
   double amount_ = 0;
   double price_ = 0;
};

//==============================================================================

class AssetGroupNode : public AssetNode
{
public:
   AssetGroupNode(const QString& name, AssetNode* parent)
    : AssetNode(name, name, parent)
    {}

   ~AssetGroupNode() noexcept override
   {
      qDeleteAll(children_);
   }

   AssetGroupNode(const AssetGroupNode&) = delete;
   AssetGroupNode& operator = (const AssetGroupNode&) = delete;

   AssetGroupNode(AssetGroupNode&&) = delete;
   AssetGroupNode& operator = (AssetGroupNode&&) = delete;

public:
   AssetNode* getChild(int row) const override {
      if ((row >= 0) && (row < children_.size())) {
         return children_[row];
      }

      return nullptr;
   }

protected:
   void AddChild(AssetNode* newChild)
   {
      const auto childId = newChild->GetNodeId().toStdString();

      if (getNodeById(childId) != nullptr) {
         delete newChild;
         return;
      }

      int index = childrenCount();
      idToIndex_.emplace(childId, index);
      nodeIDs_.emplace(childId);
      newChild->setRow(index);
      children_.append(newChild);
   }

   void RemoveChild(AssetNode* childToRemove)
   {
      int index = childToRemove->getRow();
      if ((index >= 0) && (index < children_.size())) {
         children_.removeAt(index);

         const auto nodeId = childToRemove->GetNodeId().toStdString();

         idToIndex_.erase(nodeId);
         nodeIDs_.erase(nodeId);

         for (int i=index; i<children_.size(); ++i) {
            children_[i]->setRow(i);
            idToIndex_[children_[i]->GetNodeId().toStdString()] = i;
         }

         delete childToRemove;
      } else {
         throw std::logic_error("Removing child with invalid index");
      }
   }

   AssetNode* getNodeById(const std::string& id)
   {
      auto it = idToIndex_.find(id);
      if (it == idToIndex_.end()) {
         return nullptr;
      }

      return children_[it->second];
   }

   std::unordered_set<std::string> getNodeIdSet() const
   {
      return nodeIDs_;
   }

public:
   bool HasChildren() const override {
      return !children_.isEmpty();
   }

   int childrenCount() const override {
      return children_.size();
   }

   bool HasBalance() const override {
      return false;
   }
   QString GetBalance() const override { return QString{}; }

   bool HasXBTValue() const override
   {
      if (!HasChildren()) {
         return false;
      }

      for (int i=0; i < children_.size(); ++i) {
         if (children_[i]->HasXBTValue()) {
            return true;
         }
      }

      return false;
   }

   QString GetXBTValueString() const override
   {
      return UiUtils::displayAmount(GetXBTAmount());
   }

   double GetXBTAmount() const override
   {
      double result = 0;

      for (int i=0; i < children_.size(); ++i) {
         const auto child = children_[i];

         if (child->HasXBTValue()) {
            result += child->GetXBTAmount();
         }
      }

      return result;
   }

private:
   QList<AssetNode*>                      children_;
   std::unordered_map<std::string, int>   idToIndex_;
   std::unordered_set<std::string>        nodeIDs_;
};

class XBTAssetGroupNode : public AssetGroupNode
{
public:
   XBTAssetGroupNode(const QString& xbtGroupName, AssetNode* parent)
    : AssetGroupNode(xbtGroupName, parent)
   {}
   ~XBTAssetGroupNode() noexcept override = default;

   XBTAssetGroupNode(const XBTAssetGroupNode&) = delete;
   XBTAssetGroupNode& operator = (const XBTAssetGroupNode&) = delete;

   XBTAssetGroupNode(XBTAssetGroupNode&&) = delete;
   XBTAssetGroupNode& operator = (XBTAssetGroupNode&&) = delete;

   void AddAsset(const QString& walletName, const QString& walletId)
   {
      AddChild(new XBTAssetNode(walletName, walletId, this));
   }

   void RemoveWallet(const std::string& walletId)
   {
      RemoveChild(getNodeById(walletId));
   }

   std::unordered_set<std::string> GetWalletIds() const
   {
      return getNodeIdSet();
   }

   XBTAssetNode* GetXBTNode(const std::string& walletId)
   {
      return dynamic_cast<XBTAssetNode*>(getNodeById(walletId));
   }
};

class CCAssetGroupNode : public AssetGroupNode
{
public:
   CCAssetGroupNode(const QString& xbtGroupName, AssetNode* parent)
    : AssetGroupNode(xbtGroupName, parent)
   {}
   ~CCAssetGroupNode() noexcept override = default;

   CCAssetGroupNode(const CCAssetGroupNode&) = delete;
   CCAssetGroupNode& operator = (const CCAssetGroupNode&) = delete;

   CCAssetGroupNode(CCAssetGroupNode&&) = delete;
   CCAssetGroupNode& operator = (CCAssetGroupNode&&) = delete;

   void AddAsset(const QString& name)
   {
      AddChild(new CCAssetNode(name, this));
   }

   CCAssetNode* GetCCNode(const std::string& name)
   {
      return dynamic_cast<CCAssetNode*>(getNodeById(name));
   }

   std::unordered_set<std::string> GetCCNames() const
   {
      return getNodeIdSet();
   }

   void RemoveWallet(const std::string& id)
   {
      RemoveChild(getNodeById(id));
   }
};

class FXAssetGroupNode : public AssetGroupNode
{
public:
   FXAssetGroupNode(const QString& xbtGroupName, AssetNode* parent)
    : AssetGroupNode(xbtGroupName, parent)
   {}
   ~FXAssetGroupNode() noexcept override = default;

   FXAssetGroupNode(const FXAssetGroupNode&) = delete;
   FXAssetGroupNode& operator = (const FXAssetGroupNode&) = delete;

   FXAssetGroupNode(FXAssetGroupNode&&) = delete;
   FXAssetGroupNode& operator = (FXAssetGroupNode&&) = delete;

   void AddAsset(const QString& name)
   {
      AddChild(new FXAssetNode(name, this));
   }

   FXAssetNode* GetFXNode(const std::string& name)
   {
      return dynamic_cast<FXAssetNode*>(getNodeById(name));
   }
};

class RootAssetGroupNode : public AssetGroupNode
{
public:
   RootAssetGroupNode(const QString& xbtGroupName, const QString& ccGroupName, const QString& fxGroupName)
      : AssetGroupNode(QString::fromStdString("root"), nullptr)
      , xbtGroupName_{xbtGroupName}
      , ccGroupName_{ccGroupName}
      , fxGroupName_{fxGroupName}
   {
      setRow(0);
   }

   ~RootAssetGroupNode() noexcept override = default;

   RootAssetGroupNode(const RootAssetGroupNode&) = delete;
   RootAssetGroupNode& operator = (const RootAssetGroupNode&) = delete;

   RootAssetGroupNode(RootAssetGroupNode&&) = delete;
   RootAssetGroupNode& operator = (RootAssetGroupNode&&) = delete;

   FXAssetGroupNode* GetFXGroup()
   {
      if (fxGroup_ == nullptr) {
         fxGroup_ = new FXAssetGroupNode(fxGroupName_, this);
         AddChild(fxGroup_);
      }

      return fxGroup_;
   }

   bool HaveFXGroup() const
   {
      return fxGroup_ != nullptr;
   }

   void RemoveFXGroup()
   {
      if (fxGroup_ != nullptr) {
         RemoveChild(fxGroup_);
         fxGroup_ = nullptr;
      }
   }

   bool HaveXBTGroup() const
   {
      return xbtGroup_ != nullptr;
   }

   void RemoveXBTGroup()
   {
      if (xbtGroup_ != nullptr) {
         RemoveChild(xbtGroup_);
         xbtGroup_ = nullptr;
      }
   }

   XBTAssetGroupNode* GetXBTGroup()
   {
      if (xbtGroup_ == nullptr) {
         xbtGroup_ = new XBTAssetGroupNode(xbtGroupName_, this);
         AddChild(xbtGroup_);
      }

      return xbtGroup_;
   }

   bool HaveCCGroup() const
   {
      return ccGroup_ != nullptr;
   }

   void RemoveCCGroup()
   {
      if (ccGroup_ != nullptr) {
         RemoveChild(ccGroup_);
         ccGroup_ = nullptr;
      }
   }

   CCAssetGroupNode* GetCCGroup()
   {
      if (ccGroup_ == nullptr) {
         ccGroup_ = new CCAssetGroupNode(ccGroupName_, this);
         AddChild(ccGroup_);
      }

      return ccGroup_;
   }

private:
   const QString xbtGroupName_;
   const QString ccGroupName_;
   const QString fxGroupName_;

   FXAssetGroupNode     *fxGroup_ = nullptr;
   XBTAssetGroupNode    *xbtGroup_ = nullptr;
   CCAssetGroupNode     *ccGroup_ = nullptr;
};

//==============================================================================

CCPortfolioModel::CCPortfolioModel(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<AssetManager>& assetManager
      , QObject *parent)
 : QAbstractItemModel(parent)
 , assetManager_{assetManager}
 , walletsManager_{walletsMgr}
{
   root_ = std::make_shared<RootAssetGroupNode>(tr("XBT"), tr("Private Shares"), tr("Cash"));

   connect(assetManager_.get(), &AssetManager::fxBalanceLoaded
      , this, &CCPortfolioModel::onFXBalanceLoaded, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::fxBalanceCleared
      , this, &CCPortfolioModel::onFXBalanceCleared, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::xbtPriceChanged
      , this, &CCPortfolioModel::onXBTPriceChanged, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::balanceChanged
      , this, &CCPortfolioModel::onFXBalanceChanged, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::ccPriceChanged
      , this, &CCPortfolioModel::onCCPriceChanged, Qt::QueuedConnection);

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &CCPortfolioModel::reloadXBTWalletsList);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &CCPortfolioModel::reloadXBTWalletsList);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &CCPortfolioModel::reloadXBTWalletsList);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, [this](const std::string&) { reloadXBTWalletsList(); });
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportFinished, this, &CCPortfolioModel::reloadXBTWalletsList);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::blockchainEvent, this, &CCPortfolioModel::updateXBTBalance);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &CCPortfolioModel::updateXBTBalance);
}

int CCPortfolioModel::columnCount(const QModelIndex & parent) const
{
   return PortfolioColumns::PortfolioColumnsCount;
}

QVariant CCPortfolioModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant{};
   }

   if (role == Qt::DisplayRole) {
      switch(section) {
      case PortfolioColumns::AssetNameColumn:
         return tr("Asset");
      case PortfolioColumns::BalanceColumn:
         return tr("Balance");
      case PortfolioColumns::XBTValueColumn:
         return tr("Value (XBT)");
      default:
         return QVariant{};
      }
   } else if (role == Qt::TextAlignmentRole) {
      switch(section) {
      case PortfolioColumns::AssetNameColumn:
         return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
      case PortfolioColumns::BalanceColumn:
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      case PortfolioColumns::XBTValueColumn:
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      default:
         return QVariant{};
      }
   }

   return QVariant{};
}

QVariant CCPortfolioModel::data(const QModelIndex& index, int role) const
{
   auto node = getNodeByIndex(index);

   if (role == Qt::DisplayRole) {
      switch(index.column()) {
      case PortfolioColumns::AssetNameColumn:
         return node->GetName();
      case PortfolioColumns::BalanceColumn:
         if (node->HasBalance()) {
            return node->GetBalance();
         } else {
            return QVariant{};
         }
      case PortfolioColumns::XBTValueColumn:
         if (node->HasXBTValue()) {
            return node->GetXBTValueString();
         } else {
            return QVariant{};
         }
      default:
         return QVariant{};
      }
   } else if (role == Qt::TextAlignmentRole) {
      switch(index.column()) {
      case PortfolioColumns::AssetNameColumn:
         return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
      case PortfolioColumns::BalanceColumn:
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      case PortfolioColumns::XBTValueColumn:
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      default:
         return QVariant{};
      }
   } else if (role == Qt::FontRole) {
      switch(index.column()) {
      case PortfolioColumns::XBTValueColumn:
         if (node->HasChildren()) {
            QFont font;
            font.setBold(true);
            return font;
         }
         // fall through
      default:
         return QVariant{};
      }
   }

   return QVariant{};
}

QModelIndex CCPortfolioModel::index(int row, int column, const QModelIndex & parentIndex) const
{
   if (!hasIndex(row, column, parentIndex)) {
      return QModelIndex{};
   }

   auto node = getNodeByIndex(parentIndex);
   auto child = node->getChild(row);
   if (child == nullptr) {
      return QModelIndex{};
   }
   return createIndex(row, column, static_cast<void*>(child));
}

QModelIndex CCPortfolioModel::parent(const QModelIndex& childIndex) const
{
   if (!childIndex.isValid()) {
      return QModelIndex{};
   }

   auto node = getNodeByIndex(childIndex);
   auto parentNode = node->getParent();

   if ((parentNode == nullptr) || (parentNode == root_.get())) {
      return QModelIndex{};
   }

   return createIndex(parentNode->getRow(), 0, static_cast<void*>(parentNode));
}

int CCPortfolioModel::rowCount(const QModelIndex & parentIndex) const
{
   return getNodeByIndex(parentIndex)->childrenCount();
}

std::shared_ptr<AssetManager> CCPortfolioModel::assetManager()
{
   return assetManager_;
}

std::shared_ptr<bs::sync::WalletsManager> CCPortfolioModel::walletsManager()
{
   return walletsManager_;
}

bool CCPortfolioModel::hasChildren(const QModelIndex& parentIndex) const
{
   return getNodeByIndex(parentIndex)->HasChildren();
}

AssetNode* CCPortfolioModel::getNodeByIndex(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return root_.get();
   }

   return static_cast<AssetNode*>(index.internalPointer());
}

void CCPortfolioModel::onFXBalanceLoaded()
{
   auto fxGroup = root_->GetFXGroup();

   // insert under root
   beginInsertRows(QModelIndex{}, fxGroup->getRow(), fxGroup->getRow());

   auto currencyList = assetManager_->currencies();
   for (const auto& symbolName : currencyList) {
      fxGroup->AddAsset(QString::fromStdString(symbolName));

      const double balance = assetManager_->getBalance(symbolName);
      const double price = assetManager_->getPrice(symbolName);

      auto fxNode = fxGroup->GetFXNode(symbolName);
      fxNode->SetFXAmount(balance);
      fxNode->SetPrice(price);
   }

   endInsertRows();
}

void CCPortfolioModel::onFXBalanceCleared()
{
   if (root_->HaveFXGroup()) {
      beginResetModel();
      root_->RemoveFXGroup();
      endResetModel();
   }
}

void CCPortfolioModel::onXBTPriceChanged(const std::string& currency)
{
   if (root_->HaveFXGroup()) {
      auto fxGroup = root_->GetFXGroup();

      auto fxNode = fxGroup->GetFXNode(currency);
//!      assert(fxNode != nullptr);    // produces crash on login to Celer
      if (!fxNode) {    //! workaround
         return;
      }

      const double balance = assetManager_->getBalance(currency);
      const double price = assetManager_->getPrice(currency);
      const bool priceChanged = fxNode->SetPrice(price);
      const bool balanceChanged = fxNode->SetFXAmount(balance);

      if (fxNode->HasXBTValue() && (priceChanged || balanceChanged)) {
         dataChanged(index(fxGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , index(fxGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , {Qt::DisplayRole});

         auto parentIndex = createIndex(fxGroup->getRow(), 0, static_cast<void*>(fxGroup));

         dataChanged(index(fxNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
            , index(fxNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
            , {Qt::DisplayRole});
      }
   }
}

void CCPortfolioModel::onFXBalanceChanged(const std::string& currency)
{
   if (currency != bs::network::XbtCurrency) {
      onXBTPriceChanged(currency);
   }
}

void CCPortfolioModel::onCCPriceChanged(const std::string& currency)
{
   if (root_->HaveCCGroup()) {
      auto ccGroup = root_->GetCCGroup();
      auto ccNode = ccGroup->GetCCNode(currency);

      if (ccNode != nullptr) {
         const double newPrice = assetManager_->getPrice(currency);
         const bool priceChanged = ccNode->SetPrice(newPrice);

         if (priceChanged) {
            dataChanged(index(ccGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , index(ccGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , {Qt::DisplayRole});

            auto parentIndex = createIndex(ccGroup->getRow(), 0, static_cast<void*>(ccGroup));

            dataChanged(index(ccNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
               , index(ccNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
               , {Qt::DisplayRole});
         }
      }
   }
}

void CCPortfolioModel::reloadXBTWalletsList()
{
   if (walletsManager_->hdWallets().empty()) {
      if (root_->HaveXBTGroup()) {
         beginResetModel();
         root_->RemoveXBTGroup();
         endResetModel();
      }
   } else {
      std::unordered_set<std::string>  displayedWallets{};

      struct walletInfo
      {
         std::string walletName;
         std::string walletId;
      };

      std::vector<walletInfo>           walletsToAdd{};

      if (root_->HaveXBTGroup()) {
         displayedWallets = root_->GetXBTGroup()->GetWalletIds();
      }

      for (const auto &hdWallet : walletsManager_->hdWallets()) {
         if (displayedWallets.find(hdWallet->walletId()) == displayedWallets.end()) {
            walletsToAdd.push_back(walletInfo{hdWallet->name(), hdWallet->walletId()});
         } else {
            displayedWallets.erase(hdWallet->walletId());
         }
      }

      if (!walletsToAdd.empty() || !displayedWallets.empty()) {
         beginResetModel();

         auto xbtGroup = root_->GetXBTGroup();

         // remove first if required
         for ( const auto &walletId : displayedWallets) {
            xbtGroup->RemoveWallet(walletId);
         }

         for ( const auto &walletInfo : walletsToAdd) {
            xbtGroup->AddAsset(QString::fromStdString(walletInfo.walletName)
               , QString::fromStdString(walletInfo.walletId));
         }

         if (!xbtGroup->HasChildren()) {
            root_->RemoveXBTGroup();
         }

         endResetModel();
      }
   }

   reloadCCWallets();

   updateXBTBalance();
}

void CCPortfolioModel::updateXBTBalance()
{
   if (root_->HaveXBTGroup()) {
      auto xbtGroup = root_->GetXBTGroup();

      bool balanceUpdated = false;

      auto parentIndex = createIndex(xbtGroup->getRow(), 0, static_cast<void*>(xbtGroup));

      for (const auto &hdWallet : walletsManager_->hdWallets()) {
         const auto walletId = hdWallet->walletId();
         const auto xbtNode = xbtGroup->GetXBTNode(walletId);
         if (xbtNode != nullptr) {
            const double balance = hdWallet->getTotalBalance();
            if (xbtNode->SetXBTAmount(balance)) {
               dataChanged(index(xbtNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
                  , index(xbtNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
                  , {Qt::DisplayRole});

               balanceUpdated = true;
            }
         }
      }

      if (balanceUpdated) {
         dataChanged(index(xbtGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , index(xbtGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , {Qt::DisplayRole});
      }
   }

   updateCCBalance();
}

void CCPortfolioModel::reloadCCWallets()
{
   if (walletsManager_->hdWallets().empty()) {
      if (root_->HaveCCGroup()) {
         beginResetModel();
         root_->RemoveCCGroup();
         endResetModel();
      }
   } else {
      std::unordered_set<std::string>  displayedWallets{};

      std::vector<std::string>         walletsToAdd{};

      const auto privateShares = assetManager_->privateShares();
      walletsToAdd.reserve(privateShares.size());

      if (root_->HaveCCGroup()) {
         displayedWallets = root_->GetCCGroup()->GetCCNames();
      }

      for (const auto& walletName : privateShares) {
         if (displayedWallets.find(walletName) == displayedWallets.end()) {
            walletsToAdd.emplace_back(std::move(walletName));
         } else {
            displayedWallets.erase(walletName);
         }
      }

      if (!walletsToAdd.empty() || !displayedWallets.empty()) {
         beginResetModel();

         auto ccGroup = root_->GetCCGroup();

         // remove first if required
         for ( const auto &walletName : displayedWallets) {
            ccGroup->RemoveWallet(walletName);
         }

         for ( const auto &walletName : walletsToAdd) {
            ccGroup->AddAsset(QString::fromStdString(walletName));
         }

         if (!ccGroup->HasChildren()) {
            root_->RemoveCCGroup();
         }

         endResetModel();
      }
   }
}

void CCPortfolioModel::updateCCBalance()
{
   if (root_->HaveCCGroup()) {
      auto ccGroup = root_->GetCCGroup();

      bool balanceUpdated = false;

      auto parentIndex = createIndex(ccGroup->getRow(), 0, static_cast<void*>(ccGroup));

      const auto privateShares = assetManager_->privateShares();

      for (const auto &ccName : privateShares) {
         auto ccNode = ccGroup->GetCCNode(ccName);
         if (ccNode != nullptr) {
            const double balance = assetManager_->getBalance(ccName);

            if (ccNode->SetCCAmount(balance)) {
               dataChanged(index(ccNode->getRow(), PortfolioColumns::BalanceColumn, parentIndex)
                  , index(ccNode->getRow(), PortfolioColumns::XBTValueColumn, parentIndex)
                  , {Qt::DisplayRole});

               balanceUpdated = true;
            }
         }
      }

      if (balanceUpdated) {
         dataChanged(index(ccGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , index(ccGroup->getRow(), PortfolioColumns::XBTValueColumn)
            , {Qt::DisplayRole});
      }
   }
}

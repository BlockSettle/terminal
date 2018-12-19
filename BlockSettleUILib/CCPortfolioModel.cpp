#include "CCPortfolioModel.h"

#include "AssetManager.h"
#include "UiUtils.h"
#include "WalletsManager.h"

#include <QFont>

#include <stdexcept>
#include <unordered_map>

class AssetNode
{
public:
   AssetNode(const QString& name, AssetNode* parent)
      : name_{name}
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

   virtual bool HasBalance() const {
      return true;
   }
   virtual QString GetBalance() const = 0;

   virtual bool HasXBTValue() const = 0;
   virtual QString GetXBTValueString() const = 0;

   virtual double GetXBTAmount() const = 0;

private:
   const QString  name_;
   AssetNode*     parent_;
   int            row_;
};

//==============================================================================
// Specific group nodes. Difference in balance representation

class XBTAssetNode : public AssetNode
{
public:
   XBTAssetNode(const QString& name, AssetNode* parent)
      : AssetNode(name, parent) {}
   ~XBTAssetNode() noexcept override = default;

   XBTAssetNode(const XBTAssetNode&) = delete;
   XBTAssetNode& operator = (const XBTAssetNode&) = delete;

   XBTAssetNode(XBTAssetNode&&) = delete;
   XBTAssetNode& operator = (XBTAssetNode&&) = delete;

public:
   void SetXBTAmount(double amount) {
      amount_ = amount;
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
      : AssetNode(name, parent) {}
   ~FXAssetNode() noexcept override = default;

   FXAssetNode(const FXAssetNode&) = delete;
   FXAssetNode& operator = (const FXAssetNode&) = delete;

   FXAssetNode(FXAssetNode&&) = delete;
   FXAssetNode& operator = (FXAssetNode&&) = delete;

public:
   void SetFXAmount(double amount) {
      amount_ = amount;
   }

   // set to 0, and XBT value will be empty
   void SetPrice(double price) {
      price_ = price;
   }

public:
   bool HasBalance() const override {
      return true;
   }

   bool HasXBTValue() const override {
      return !qFuzzyIsNull(price_);
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
      : AssetNode(name, parent) {}
   ~CCAssetNode() noexcept override = default;

   CCAssetNode(const CCAssetNode&) = delete;
   CCAssetNode& operator = (const CCAssetNode&) = delete;

   CCAssetNode(CCAssetNode&&) = delete;
   CCAssetNode& operator = (CCAssetNode&&) = delete;

public:
   void SetCCAmount(double amount) {
      amount_ = amount;
   }

   // set to 0, and XBT value will be empty
   void SetPrice(double price) {
      price_ = price;
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
    : AssetNode(name, parent)
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
   virtual void AddAsset(const QString& name) = 0;

   AssetNode* getChild(int row) const override {
      if ((row >= 0) && (row < children_.size())) {
         return children_[row];
      }

      return nullptr;
   }

protected:
   void AddChild(AssetNode* newChild)
   {
      int index = childrenCount();
      nameToIndex_.emplace(newChild->GetName().toStdString(), index);
      newChild->setRow(index);
      children_.append(newChild);
   }

   void RemoveChild(AssetNode* childToRemove)
   {
      int index = childToRemove->getRow();
      if ((index >= 0) && (index < children_.size())) {
         children_.removeAt(index);

         for (int i=index; i<children_.size(); ++i) {
            children_[i]->setRow(i);
            nameToIndex_[children_[i]->GetName().toStdString()] = i;
         }

         delete childToRemove;
      } else {
         throw std::logic_error("Removing child with invalid index");
      }
   }

   AssetNode* getNodeByName(const std::string& name)
   {
      auto it = nameToIndex_.find(name);
      if (it == nameToIndex_.end()) {
         return nullptr;
      }

      return children_[it->second];
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
   std::unordered_map<std::string, int>   nameToIndex_;
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

   void AddAsset(const QString& name) override
   {
      AddChild(new XBTAssetNode(name, this));
   }

   XBTAssetNode* GetXBTNode(const std::string& name)
   {
      return dynamic_cast<XBTAssetNode*>(getNodeByName(name));
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

   void AddAsset(const QString& name) override
   {
      AddChild(new CCAssetNode(name, this));
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

   void AddAsset(const QString& name) override
   {
      AddChild(new FXAssetNode(name, this));
   }

   FXAssetNode* GetFXNode(const std::string& name)
   {
      return dynamic_cast<FXAssetNode*>(getNodeByName(name));
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

   void AddAsset(const QString& name) override
   {
      throw std::logic_error("RootAssetGroupNode::AddAsset should never be called directly");
   }

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

   XBTAssetGroupNode* GetXBTGroup()
   {
      if (xbtGroup_ == nullptr) {
         xbtGroup_ = new XBTAssetGroupNode(xbtGroupName_, this);
         AddChild(xbtGroup_);
      }

      return xbtGroup_;
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

CCPortfolioModel::CCPortfolioModel(const std::shared_ptr<WalletsManager>& walletsManager
      , const std::shared_ptr<AssetManager>& assetManager
      , QObject *parent)
 : QAbstractItemModel(parent)
 , assetManager_{assetManager}
 , walletsManager_{walletsManager}
{
   root_ = std::make_shared<RootAssetGroupNode>(tr("XBT"), tr("Private Shares"), tr("Cash"));

   connect(assetManager_.get(), &AssetManager::fxBalanceLoaded
      , this, &CCPortfolioModel::onFXBalanceLoaded, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::fxBalanceCleared
      , this, &CCPortfolioModel::onFXBalanceCleared, Qt::QueuedConnection);
   connect(assetManager_.get(), &AssetManager::xbtPriceChanged
      , this, &CCPortfolioModel::onXBTPriceChanged, Qt::QueuedConnection);
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
   // load list of FX products
   // create nodes
   // set balances
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
}
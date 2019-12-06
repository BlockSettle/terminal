/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SecuritiesModel.h"

#include "AssetManager.h"

#include <set>

class BaseSecurityNode
{
public:
   BaseSecurityNode() = default;
   virtual ~BaseSecurityNode() noexcept = default;

   BaseSecurityNode(const BaseSecurityNode&) = delete;
   BaseSecurityNode& operator = (const BaseSecurityNode&) = delete;

   BaseSecurityNode(BaseSecurityNode&&) = delete;
   BaseSecurityNode& operator = (BaseSecurityNode&&) = delete;

public:
   virtual bool isRoot() const = 0;

   virtual int getCheckedState() const = 0;
   virtual void onUserSetCheckedState(int checkedState) = 0;
   virtual void onChildNewState(bool) {};

   virtual bool hasChildren() const = 0;
   virtual size_t childrenCount() const = 0;
   virtual BaseSecurityNode* getParent() const = 0;
   virtual BaseSecurityNode* getChild(int row) const = 0;

   virtual int getRow() const = 0;

   virtual QString getName() const = 0;
};

class SecurityNode : public BaseSecurityNode
{
public:
   SecurityNode(const QString& name, bool isChecked)
    : name_(name)
    , isChecked_(isChecked)
    , parent_(nullptr)
   {
   }

public:
   void SetParent(BaseSecurityNode* parent) {
      parent_ = parent;
   }

   bool isChecked() const { return isChecked_; }
   void SetChecked(bool checked) { isChecked_ = checked; }

   bool isRoot() const override { return false; }
   QString getName() const override { return name_; }
   int getCheckedState() const override
   {
      return isChecked_ ? Qt::Checked : Qt::Unchecked;
   }

   BaseSecurityNode* getParent() const override { return parent_; }
   BaseSecurityNode* getChild(int) const override
   {
      return nullptr;
   }

   int getRow() const override { return 0; }
   bool hasChildren() const override     { return false; }
   size_t childrenCount() const override { return 0; }

   void onUserSetCheckedState(int checkedState) override
   {
      auto newState = checkedState == Qt::Checked;
      if (isChecked_ != newState) {
         isChecked_ = newState;
         parent_->onChildNewState(isChecked_);
      }
   }

private:
   QString  name_;
   bool     isChecked_;
   BaseSecurityNode* parent_;
};

class SecurityGroupNode : public BaseSecurityNode
{
public:
   SecurityGroupNode(const QString& name)
    : name_(name)
    , row_(-1)
    , checkedCount_(0)
   {}

   void SetRow(int row)
   {
      row_ = row;
   }

   void AddChild(const std::shared_ptr<SecurityNode> child)
   {
      child->SetParent((BaseSecurityNode*)this);
      children_.emplace_back(child);
      if (child->isChecked()) {
         checkedCount_++;
      }
   }

   bool isRoot() const override { return false; }
   QString getName() const override { return name_; }
   int getCheckedState() const override
   {
      if (checkedCount_ == 0) {
         return Qt::Unchecked;
      }
      if (checkedCount_ == children_.size()) {
         return Qt::Checked;
      }

      return Qt::PartiallyChecked;
   }

   BaseSecurityNode* getParent() const override { return nullptr; }
   BaseSecurityNode* getChild(int row) const override
   {
      if ((row >= children_.size()) || (row < 0 )) {
         return nullptr;
      }

      return children_[row].get();
   }

   int getRow() const override { return row_; }
   bool hasChildren() const override     { return !children_.empty(); }
   size_t childrenCount() const override { return children_.size(); }

   void onChildNewState(bool isChecked) override {
      if (isChecked) {
         checkedCount_++;
      } else {
         checkedCount_--;
      }
   }

   void onUserSetCheckedState(int checkedState) override
   {
      auto isChecked = checkedState == Qt::Checked;
      checkedCount_ = isChecked ? children_.size() : 0;

      for (auto child : children_) {
         child->SetChecked(isChecked);
      }
   }

private:
   QString name_;
   int row_;
   std::vector<std::shared_ptr<SecurityNode>> children_;

   int checkedCount_;
};

class RootSecuritiesNode : public BaseSecurityNode
{
public:
   void AddGroupNode(const std::shared_ptr<SecurityGroupNode>& groupNode)
   {
      groupNode->SetRow(groupNodes_.size());
      groupNodes_.emplace_back(groupNode);
   }

public:
   bool isRoot() const override { return true; }

   int getCheckedState() const override { return Qt::Unchecked; }

   bool hasChildren() const override { return !groupNodes_.empty(); }
   size_t childrenCount() const override { return groupNodes_.size(); }
   BaseSecurityNode* getParent() const override { return nullptr; }
   BaseSecurityNode* getChild(int row) const override
   {
      if ((row >= groupNodes_.size()) || (row < 0 )) {
         return nullptr;
      }

      return groupNodes_[row].get();
   }

   int getRow() const override { return 0; }

   QString getName() const override { return QString{}; }
   void onUserSetCheckedState(int checkedState) override {}

private:
   std::vector<std::shared_ptr<BaseSecurityNode>> groupNodes_;
};


SecuritiesModel::SecuritiesModel(const std::shared_ptr<AssetManager> &assetMgr
   , const QStringList &showSettings, QObject *parent)
 : QAbstractItemModel(parent)
{
   std::set<QString> instrVisible;
   for (const auto &setting : showSettings) {
      instrVisible.insert(setting);
   }

   rootNode_ = std::make_shared<RootSecuritiesNode>();

   if (assetMgr && assetMgr->hasSecurities()) {
      for (int asset = static_cast<int>(bs::network::Asset::first); asset < static_cast<int>(bs::network::Asset::last); asset++) {
         const QString groupName = tr(bs::network::Asset::toString(static_cast<bs::network::Asset::Type>(asset)));

         auto groupNode = std::make_shared<SecurityGroupNode>(groupName);

         const auto securities = assetMgr->securities(static_cast<bs::network::Asset::Type>(asset));
         for (const auto &security : securities) {
            const bool isVisible = instrVisible.find(security) == instrVisible.end();
            auto item = std::make_shared<SecurityNode>(security, isVisible);

            item->SetChecked(isVisible);
            groupNode->AddChild(item);
         }

         rootNode_->AddGroupNode(groupNode);
      }
   }
}

int SecuritiesModel::columnCount(const QModelIndex& parent) const
{
   return 1;
}

int SecuritiesModel::rowCount(const QModelIndex& parent) const
{
   auto node = getNodeByIndex(parent);
   return node->childrenCount();
}

QVariant SecuritiesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   return QVariant{};
}

Qt::ItemFlags SecuritiesModel::flags(const QModelIndex & index) const
{
   Qt::ItemFlags flags = QAbstractItemModel::flags(index);
   if (index.column() == 0) {
      flags |= Qt::ItemIsUserCheckable;
   }
   return flags;
}

QVariant SecuritiesModel::data(const QModelIndex& index, int role) const
{
   const auto node = getNodeByIndex(index);
   if (node->isRoot()) {
      return QVariant{};
   }

   if (role == Qt::DisplayRole) {
      if (index.column() == 0) {
         return node->getName();
      }
   } else if (role == Qt::CheckStateRole) {
      if (index.column() == 0) {
         return node->getCheckedState();
      }
   }

   return QVariant{};
}

bool SecuritiesModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
   if (role == Qt::CheckStateRole) {
      emit layoutAboutToBeChanged();

      int state = value.toInt();

      auto node = getNodeByIndex(index);
      if (node->isRoot()) {
         return false;
      }
      node->onUserSetCheckedState(state);

      emit layoutChanged();
      return true;
   }

   return false;
}

QModelIndex SecuritiesModel::index(int row, int column, const QModelIndex& parent) const
{
   if (!hasIndex(row, column, parent)) {
      return QModelIndex();
   }

   auto node = getNodeByIndex(parent);
   auto child = node->getChild(row);
   if (child == nullptr) {
      return QModelIndex();
   }
   return createIndex(row, column, static_cast<void*>(child));
}

QModelIndex SecuritiesModel::parent(const QModelIndex& child) const
{
   if (!child.isValid()) {
      return QModelIndex{};
   }

   auto node = getNodeByIndex(child);
   auto parentNode = node->getParent();

   if ((parentNode == nullptr) || (parentNode->isRoot())) {
      return QModelIndex{};
   }

   return createIndex(parentNode->getRow(), 0, static_cast<void*>(parentNode));
}

bool SecuritiesModel::hasChildren(const QModelIndex& parent) const
{
   auto node = getNodeByIndex(parent);
   return node->hasChildren();
}

QStringList SecuritiesModel::getVisibilitySettings() const
{
   QStringList result;

   for (int igroup=0; igroup < rootNode_->childrenCount(); igroup++) {
      auto groupNode = rootNode_->getChild(igroup);

      for (int i=0; i < groupNode->childrenCount(); i++) {
         auto child = dynamic_cast<SecurityNode*>(groupNode->getChild(i));
         if (!child->isChecked()) {
            result << child->getName();
         }
      }
   }

   return result;
}

BaseSecurityNode* SecuritiesModel::getNodeByIndex(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return rootNode_.get();
   }
   return static_cast<BaseSecurityNode*>(index.internalPointer());
}

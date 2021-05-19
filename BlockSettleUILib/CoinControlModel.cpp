/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CoinControlModel.h"
#include <QColor>
#include <QList>
#include <QString>
#include <QFont>
#include "BTCNumericTypes.h"
#include "BtcUtils.h"
#include "SelectedTransactionInputs.h"
#include "TxClasses.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"


class TransactionNode;
class CoinControlNode
{
public:
   // Default sort order is native, then nested and then CPFP root node
   enum class Type
   {
      DoesNotMatter,
      Native,
      Nested,
      CpfpRoot,
   };

   CoinControlNode(Type type, const QString& name, const QString &comment, int row, CoinControlNode *parent = nullptr)
      : type_(type)
      , name_(name)
      , comment_(comment)
      , row_(row)
      , parent_(parent)
   {}

   virtual ~CoinControlNode() noexcept
   {
      qDeleteAll(children_);
   }

   CoinControlNode(const CoinControlNode&) = delete;
   CoinControlNode& operator = (const CoinControlNode&) = delete;

   CoinControlNode(CoinControlNode&&) = delete;
   CoinControlNode& operator = (CoinControlNode&&) = delete;

   int getRow() const { return row_; }

   QString getName() const { return name_; }
   QString getComment() const { return comment_; }
   virtual int getUtxoCount() const { return 0; }

   bool hasChildren() const { return !children_.empty(); }
   size_t  nbChildren() const { return (size_t)children_.count(); }
   void appendChildNode(CoinControlNode* node) { children_.push_back(node); }
   CoinControlNode* getChild(const int i) { return (i < children_.size() ? children_[i] : nullptr); }

   CoinControlNode* getParent() const { return parent_; }
   virtual BTCNumericTypes::balance_type getSelectedAmount() const = 0;
   virtual BTCNumericTypes::balance_type getTotalAmount() const = 0;
   virtual int getSelectionCount() const = 0;

   bool isRoot() const { return parent_ == nullptr; }

   virtual void setCheckedState(int state) = 0;
   virtual int  getCheckedState() const = 0;

   virtual void updateParentWithSelectionInfo(int totalSelectedDiff
      , BTCNumericTypes::balance_type selectedAmountDiff
      , BTCNumericTypes::balance_type totalAmountDiff) = 0;

   void UpdateChildsState(int state) {
      for (int i = 0; i < children_.size(); ++i) {
         children_[i]->setCheckedState(state);
      }
   }

   virtual void ApplySelection(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
   {
      for (int i = 0; i < children_.size(); ++i) {
         children_[i]->ApplySelection(selectedInputs);
      }
   }

   virtual void notifyChildAdded() {}

   void sort(int column, Qt::SortOrder order);

   static Type detectType(const bs::Address& address) {
      return (address.getType() & ADDRESS_NESTED_MASK) ? Type::Nested : Type::Native;
   }

   virtual bool isEnabled() const
   {
      return true;
   }

private:
   const Type                 type_;
   QString                    name_;
   QString                    comment_;
   QList<CoinControlNode*>    children_;
   int                        row_;
   CoinControlNode         *  parent_;
};

class TransactionNode : public CoinControlNode
{
public:
   TransactionNode(bool isSelected, int transactionIndex, const UTXO& transaction, const std::shared_ptr<bs::sync::Wallet> &wallet
      , CoinControlNode *parent)
      : CoinControlNode(CoinControlNode::Type::DoesNotMatter, QString::fromStdString(transaction.getTxHash().toHexStr(true))
         , QString(), parent->nbChildren(), parent)
      , checkedState_(isSelected ? Qt::Checked : Qt::Unchecked)
      , transactionIndex_(transactionIndex)
   {
      amount_ = wallet ? wallet->getTxBalance(transaction.getValue()) : transaction.getValue() / BTCNumericTypes::BalanceDivider;
   }

   bool isEnabled() const override
   {
      return (transactionIndex_ >= 0);
   }

   ~TransactionNode() noexcept override = default;

   BTCNumericTypes::balance_type getTotalAmount() const override
   {
      return amount_;
   }
   BTCNumericTypes::balance_type getSelectedAmount() const override { return amount_; }

   int getSelectionCount() const override {
      return (checkedState_ == Qt::Checked ? 1 : 0);
   }

   int  getCheckedState() const override
   {
      return checkedState_;
   }

   void setCheckedState(int state) override
   {
      if (!isEnabled() || (checkedState_ == state)) {
         return;
      }

      checkedState_ = state;
      if (checkedState_ == Qt::Checked) {
         getParent()->updateParentWithSelectionInfo(1, amount_, 0);
      } else {
         getParent()->updateParentWithSelectionInfo(-1, -amount_, 0);
      }
   }

   void updateParentWithSelectionInfo(int, BTCNumericTypes::balance_type, BTCNumericTypes::balance_type) override {}

   void ApplySelection(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs) override
   {
      // transactionIndex_ will be -1 for unconfirmed inputs
      if (transactionIndex_ >= 0) {
         selectedInputs->SetTransactionSelection(transactionIndex_, checkedState_ == Qt::Checked);
      }
   }

protected:
   BTCNumericTypes::balance_type amount_;
   int checkedState_;
   int transactionIndex_;
};

class CPFPTransactionNode : public TransactionNode
{
public:
   CPFPTransactionNode(bool isSelected, int transactionIndex, const UTXO& transaction, const std::shared_ptr<bs::sync::Wallet> &wallet
      , CoinControlNode *parent)
      : TransactionNode(isSelected, transactionIndex, transaction, wallet, parent) {}

   void ApplySelection(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs) override
   {
      selectedInputs->SetCPFPTransactionSelection(transactionIndex_, checkedState_ == Qt::Checked);
   }
};

class AddressNode : public CoinControlNode
{
public:
   AddressNode(Type type, const QString& name, const QString &comment, int row, CoinControlNode *parent = nullptr)
      : CoinControlNode(type, name, comment, row, parent)
      , utxoCount_(0)
   {}
   ~AddressNode() noexcept override = default;

   void addTransaction(TransactionNode *transaction)
   {
      appendChildNode(transaction);
      notifyChildAdded();
      if (transaction->isEnabled()) {
         addBalance(transaction->getSelectionCount(), transaction->getTotalAmount()
            , transaction->getTotalAmount());
      }
      incrementUtxoCount();
   }

   bool isEnabled() const override
   {
      return (totalBalance_ > 0);
   }

   void incrementUtxoCount()
   {
      ++utxoCount_;

      if (getParent()) {
         auto a = dynamic_cast<AddressNode*>(getParent());

         if (a) {
            a->incrementUtxoCount();
         }
      }
   }

   BTCNumericTypes::balance_type getTotalAmount() const override {
      return totalBalance_;
   }
   BTCNumericTypes::balance_type getSelectedAmount() const override {
      return selectedBalance_;
   }

   int getSelectionCount() const override {
      return totalSelected_;
   }

   int  getCheckedState() const override
   {
      return checkedState_;
   }

   void setCheckedState(int state) override
   {
      UpdateChildsState(state);
   }

   int getUtxoCount() const override
   {
      return utxoCount_;
   }

protected:
   void updateParentWithSelectionInfo(int totalSelectedDiff, BTCNumericTypes::balance_type selectedAmountDiff
      , BTCNumericTypes::balance_type totalAmountDiff) override
   {
      addBalance(true, selectedAmountDiff, totalAmountDiff, totalSelectedDiff);
   }

   void notifyChildAdded() override
   {
      ++totalChildren_;
      if (!isRoot()) {
         getParent()->notifyChildAdded();
      }
   }

private:
   void addBalance(bool selected, BTCNumericTypes::balance_type amount, BTCNumericTypes::balance_type totalInc, int countInc = 1)
   {
      const auto oldTotalSelected = totalSelected_;
      const auto oldSelected = selectedBalance_;

      totalBalance_ += totalInc;
      if (selected) {
         totalSelected_ += countInc;
         selectedBalance_ += amount;
      }

      updateCheckState();

      if (!isRoot()) {
         getParent()->updateParentWithSelectionInfo(totalSelected_ - oldTotalSelected
            , selectedBalance_ - oldSelected, totalInc);
      }
   }

   void updateCheckState()
   {
      if (totalSelected_ != totalChildren_) {
         if (totalSelected_ == 0) {
            checkedState_ = Qt::Unchecked;
         }
         else {
            checkedState_ = Qt::PartiallyChecked;
         }
      }
      else {
         checkedState_ = Qt::Checked;
      }
   }

private:
   BTCNumericTypes::balance_type totalBalance_ = 0;
   BTCNumericTypes::balance_type selectedBalance_ = 0;
   int totalSelected_ = 0;
   int totalChildren_ = 0;
   int checkedState_ = Qt::Unchecked;
   int utxoCount_;
};


void CoinControlNode::sort(int column, Qt::SortOrder order) {
   qSort(std::begin(children_), std::end(children_)
      , [column, order](CoinControlNode* left, CoinControlNode* right) {
      bool res = true;
      switch(column){
      case 0:
         res = (left->type_ != right->type_) ? (left->type_ < right->type_) : (left->getName().compare(right->getName()) < 0);
         break;
      case 1:
         res = left->getUtxoCount() < right->getUtxoCount();
         break;
      case 2:
         res = left->getComment().compare(right->getComment()) < 0;
         break;
      default:
         res = ((TransactionNode*)left)->getTotalAmount() < ((TransactionNode*)right)->getTotalAmount();
         break;
      }

      if (order == Qt::DescendingOrder)
         return !res;

      return res;
   });
}

CoinControlModel::CoinControlModel(const std::shared_ptr<SelectedTransactionInputs> &selectedInputs, QObject* parent)
   : QAbstractItemModel(parent)
   , wallet_(selectedInputs->GetWallet())
{
   root_ = std::make_shared<AddressNode>(CoinControlNode::Type::DoesNotMatter, tr("Unspent transactions"), QString(), 0);
   loadInputs(selectedInputs);
}

QVariant CoinControlModel::data(const QModelIndex& index, int role) const
{
   const auto node = getNodeByIndex(index);

   if (role == Qt::DisplayRole) {
      switch(index.column())
      {
      case ColumnName:
         return node->getName();
      case ColumnComment:
         return node->getComment();
      case ColumnUTXOCount:
         return node->getUtxoCount();
      case ColumnBalance: {
         const auto amount = (node->getSelectionCount() == 0) ? node->getTotalAmount() : node->getSelectedAmount();
         return (wallet_ && wallet_->type() == bs::core::wallet::Type::ColorCoin) ? UiUtils::displayCCAmount(amount) : UiUtils::displayAmount(amount);
      }
      default:
         return QVariant{};
      }
   } else if (role == Qt::CheckStateRole) {
      if (index.column() == 0) {
         return node->getCheckedState();
      }
   } else if (role == Qt::UserRole) {
      return node->isEnabled();
   } else if (role == Qt::TextColorRole) {
      return node->isEnabled() ? QVariant{} : QColor(Qt::gray);
   } else if (role == Qt::TextAlignmentRole) {
      if (index.column() == ColumnBalance) {
         return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      } else if (index.column() == ColumnUTXOCount) {
         return Qt::AlignCenter;
      }
   }
   return QVariant{};
}

bool CoinControlModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
   if (role == Qt::CheckStateRole) {
      auto node = getNodeByIndex(index);
      if (!node->isEnabled()) {
         return false;
      }
      node->setCheckedState(value.toInt());

      emit layoutChanged();
      emit selectionChanged();
      return true;
   }

   return false;
}

QVariant CoinControlModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal || role != Qt::DisplayRole ) {
      return QVariant();
   }

   switch (section) {
   case ColumnName:
      return tr("Address/Hash");
   case ColumnUTXOCount:
      return tr("#");
   case ColumnComment:
      return tr("Comment");
   case ColumnBalance:
      return tr("Balance");
   }
   return QVariant();
}

int CoinControlModel::rowCount(const QModelIndex& parent) const
{
   auto node = getNodeByIndex(parent);
   int result = node->nbChildren();
   return result;
}

int CoinControlModel::columnCount(const QModelIndex &) const
{
   return ColumnsCount;
}

bool CoinControlModel::hasChildren(const QModelIndex& parent) const
{
   auto node = getNodeByIndex(parent);
   bool result = node->hasChildren();
   return result;
}

QModelIndex CoinControlModel::index(int row, int column, const QModelIndex& parent) const
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

QModelIndex CoinControlModel::parent(const QModelIndex& child) const
{
   if (!child.isValid()) {
      return QModelIndex();
   }

   auto node = getNodeByIndex(child);
   auto parentNode = node->getParent();

   if ((parentNode == nullptr) || (parentNode == root_.get())) {
      return QModelIndex();
   }

   return createIndex(parentNode->getRow(), 0, static_cast<void*>(parentNode));
}

Qt::ItemFlags CoinControlModel::flags(const QModelIndex& index) const
{
   Qt::ItemFlags flags = QAbstractItemModel::flags(index);
   if (index.column() == 0) {
      flags |= Qt::ItemIsUserCheckable;
   }
   auto node = getNodeByIndex(index);
   if (node && !node->isEnabled()) {
      flags &= ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
   }
   return flags;
}

size_t CoinControlModel::GetSelectedTransactionsCount() const
{
   return root_->getSelectionCount();
}

QString CoinControlModel::GetSelectedBalance() const
{
   if (root_ == nullptr) {
      return {};
   }
   const auto amount = qMax<BTCNumericTypes::balance_type>(root_->getSelectedAmount(), 0);
   return (wallet_ && wallet_->type() == bs::core::wallet::Type::ColorCoin) ? UiUtils::displayCCAmount(amount) : UiUtils::displayAmount(amount);
}

QString CoinControlModel::GetTotalBalance() const
{
   if (root_ == nullptr) {
      return {};
   }
   const auto amount = qMax<BTCNumericTypes::balance_type>(root_->getTotalAmount(), 0);
   return (wallet_ && wallet_->type() == bs::core::wallet::Type::ColorCoin) ? UiUtils::displayCCAmount(amount) : UiUtils::displayAmount(amount);
}

CoinControlNode* CoinControlModel::getNodeByIndex(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return root_.get();
   }
   return static_cast<CoinControlNode*>(index.internalPointer());
}

static int addressWeight(const bs::Address &addr)
{
   if (!addr.isValid()) {
      return INT_MAX;
   }
   switch (addr.getType()) {
   case AddressEntryType_P2WPKH: return 0;
   case AddressEntryType_P2SH:
   case static_cast<AddressEntryType>(AddressEntryType_P2SH + AddressEntryType_P2WPKH):
      return 1;
   case AddressEntryType_P2PKH:  return 2;
   default: return 3;
   }
}

void CoinControlModel::loadInputs(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   struct InputKey {
      UTXO  utxo;
      int   index;

      bool operator<(const InputKey &other) const
      {
         const auto aw1 = addressWeight(bs::Address::fromUTXO(utxo));
         const auto aw2 = addressWeight(bs::Address::fromUTXO(other.utxo));
         if (aw1 != aw2) {
            return aw1 < aw2;
         }
         if (index != other.index) {
            return (index < other.index);
         }
         return (utxo < other.utxo);
      }
   };
   struct UtxoState {
      bool selected;
      bool enabled;
   };
   std::map<InputKey, UtxoState> inputs;
   for (int i = 0; i < selectedInputs->GetTransactionsCount(); ++i) {
      inputs[{selectedInputs->GetTransaction(i), i}] = {
         selectedInputs->IsTransactionSelected(i), true };
   }

   for (const auto &utxo : selectedInputs->getIncompleteUTXOs()) {
      inputs[{utxo, -1}] = { false, false };
   }

   for (const auto &input : inputs) {
      const auto address = bs::Address::fromUTXO(input.first.utxo);
      const auto addrStr = address.display();

      auto addressIt = addressNodes_.find(addrStr);
      AddressNode *addressNode = nullptr;

      if (addressIt == addressNodes_.end()) {
         auto wallet = selectedInputs->GetWallet();
         auto comment = wallet ? wallet->getAddressComment(bs::Address::fromHash(input.first.utxo.getRecipientScrAddr())) : "";
         addressNode = new AddressNode(CoinControlNode::detectType(address), QString::fromStdString(address.display())
            , QString::fromStdString(comment), (int)addressNodes_.size(), root_.get());
         root_->appendChildNode(addressNode);
         addressNodes_.emplace(addrStr, addressNode);
      } else {
         addressNode = static_cast<AddressNode*>(addressIt->second);
      }
      addressNode->addTransaction(new TransactionNode(input.second.selected
         , input.first.index, input.first.utxo, selectedInputs->GetWallet(), addressNode));    //TODO: Add TX comment
   }

   const auto cpfpList = selectedInputs->GetCPFPInputs();
   if (!cpfpList.empty()) {
      cpfp_ = std::make_shared<AddressNode>(CoinControlNode::Type::CpfpRoot, tr("CPFP Eligible Outputs"), tr("Child-Pays-For-Parent transactions")
         , addressNodes_.size(), root_.get());
      root_->appendChildNode(cpfp_.get());
      for (size_t i = 0; i < cpfpList.size(); i++) {
         const auto &input = cpfpList[i];
         const auto address = bs::Address::fromUTXO(input);
         const auto addrStr = address.display();
         AddressNode *addressNode = nullptr;
         const auto itAddr = cpfpNodes_.find(addrStr);

         if (itAddr == cpfpNodes_.end()) {
            const int row = cpfpNodes_.size();
            const auto& wallet = selectedInputs->GetWallet();
            addressNode = new AddressNode(CoinControlNode::Type::DoesNotMatter, QString::fromStdString(address.display())
               , QString::fromStdString(wallet ? wallet->getAddressComment(bs::Address::fromHash(input.getRecipientScrAddr())) : "")
               , row, cpfp_.get());
            cpfp_->appendChildNode(addressNode);
            cpfpNodes_[addrStr] = addressNode;
         }
         else {
            addressNode = static_cast<AddressNode *>(itAddr->second);
         }
         const auto isSel = selectedInputs->IsTransactionSelected(i + selectedInputs->GetTransactionsCount());
         addressNode->addTransaction(new CPFPTransactionNode(isSel, i, input, selectedInputs->GetWallet(), addressNode));
      }
   }
}

void CoinControlModel::ApplySelection(const std::shared_ptr<SelectedTransactionInputs>& selectedInputs)
{
   root_->ApplySelection(selectedInputs);
}

void CoinControlModel::clearSelection()
{
   root_->UpdateChildsState(Qt::Unchecked);
   emit layoutChanged();
}

void CoinControlModel::selectAll(int sel)
{
   root_->UpdateChildsState(static_cast<Qt::CheckState>(sel));
   emit layoutChanged();
   emit selectionChanged();
}

void CoinControlModel::sort(int column, Qt::SortOrder order){
   root_->sort(column, order);
   emit layoutChanged();
   emit selectionChanged();
}

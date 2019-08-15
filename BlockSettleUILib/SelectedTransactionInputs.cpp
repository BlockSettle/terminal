#include "SelectedTransactionInputs.h"

#include <QPointer>
#include <QApplication>
#include "Wallets/SyncWallet.h"

SelectedTransactionInputs::SelectedTransactionInputs(const std::shared_ptr<bs::sync::Wallet> &wallet
   , bool isSegWitInputsOnly, bool confirmedOnly
   , const CbSelectionChanged &selectionChanged, const std::function<void()> &cbInputsReset)
   : QObject(nullptr), wallet_(wallet)
   , isSegWitInputsOnly_(isSegWitInputsOnly)
   , confirmedOnly_(confirmedOnly)
   , selectionChanged_(selectionChanged)
{
   ResetInputs(cbInputsReset);
}

SelectedTransactionInputs::SelectedTransactionInputs(const std::shared_ptr<bs::sync::Wallet> &wallet
   , const std::vector<UTXO> &utxos
   , const CbSelectionChanged &selectionChanged)
   : QObject(nullptr), wallet_(wallet)
   , isSegWitInputsOnly_(false)
   , confirmedOnly_(false)
   , selectionChanged_(selectionChanged)
   , useAutoSel_(false)
{
   SetFixedInputs(utxos);
}

void SelectedTransactionInputs::Reload(const std::vector<UTXO> &utxos)
{
   const auto &cbFilter = [this, utxos] {
      for (const auto &utxo : utxos) {
         if (!filterUTXO(inputs_, utxo, 0)) {
            filterUTXO(cpfpInputs_, utxo, inputs_.size());
         }
      }
   };
   ResetInputs(cbFilter);
}

void SelectedTransactionInputs::SetFixedInputs(const std::vector<UTXO> &inputs)
{
   cpfpInputs_.clear();
   inputs_ = isSegWitInputsOnly_ ? filterNonSWInputs(inputs) : inputs;

   resetSelection();
}

void SelectedTransactionInputs::resetSelection()
{
   selection_ = std::vector<bool>(inputs_.size() + cpfpInputs_.size(), false);
   totalSelected_ = 0;
   selectedBalance_ = 0;

   totalBalance_ = 0;
   for (size_t i = 0; i < GetTotalTransactionsCount(); i++) {
      totalBalance_ += GetTransaction(i).getValue();
   }

   if (selectionChanged_) {
      selectionChanged_();
   }
}

void SelectedTransactionInputs::onCPFPReceived(std::vector<UTXO> inputs)
{
   cpfpInputs_ = inputs;
   QPointer<SelectedTransactionInputs> thisPtr = this;
   wallet_->getSpendableTxOutList([thisPtr](const std::vector<UTXO> &utxos) {
      QMetaObject::invokeMethod(qApp, [thisPtr, utxos] {
         if (thisPtr) {
            thisPtr->onUTXOsReceived(utxos);
         }
      });
   }, UINT64_MAX);
}

void SelectedTransactionInputs::onUTXOsReceived(std::vector<UTXO> inputs)
{
   inputs_ = inputs;
   resetSelection();
   for (const auto &cb : resetCallbacks_) {
      if (cb) {
         cb();
      }
   }
   resetCallbacks_.clear();
}

void SelectedTransactionInputs::ResetInputs(const std::function<void()> &cb)
{
   inputs_.clear();
   cpfpInputs_.clear();

   resetCallbacks_.push_back(cb);
   if (resetCallbacks_.size() > 1) {
      return;
   }

   QPointer<SelectedTransactionInputs> thisPtr = this;
   if (confirmedOnly_) {
      wallet_->getSpendableTxOutList([thisPtr](const std::vector<UTXO> &inputs) {
         QMetaObject::invokeMethod(qApp, [thisPtr, inputs] {
            if (thisPtr) {
               thisPtr->onUTXOsReceived(inputs);
            }
         });
      }, UINT64_MAX);
   }
   else {
      wallet_->getSpendableZCList([thisPtr](const std::vector<UTXO> &inputs) {
         QMetaObject::invokeMethod(qApp, [thisPtr, inputs] {
            if (thisPtr) {
               thisPtr->onCPFPReceived(inputs);
            }
         });
      });
   }
}

bool SelectedTransactionInputs::filterUTXO(std::vector<UTXO> &inputs, const UTXO &utxo, size_t selectionStart)
{
   const auto it = std::find(inputs.begin(), inputs.end(), utxo);
   if (it == inputs.end()) {
      return false;
   }
   const auto index = it - inputs.begin();
   if (selection_[selectionStart + index]) {
      totalSelected_--;
      selectedBalance_ -= utxo.getValue();
   }
   inputs.erase(it);
   selection_.erase(selection_.begin() + selectionStart + index);
   totalBalance_ -= utxo.getValue();
   return true;
}

static bool isSegWitInput(const UTXO &utxo)
{
   const auto recipAddr = bs::Address::fromUTXO(utxo);
   switch (recipAddr.getType()) {
   case AddressEntryType_P2WPKH:
   case AddressEntryType_P2WSH:
   case AddressEntryType_P2SH:
      return true;
   case AddressEntryType_Default:   // fallback for script not from our wallet
   default: break;                  // fallback for incorrectly deserialized wallet
   }
   return false;
}

static bool isSegWit(const UTXO &input)
{
   return input.isSegWit() || isSegWitInput(input);
}

std::vector<UTXO> SelectedTransactionInputs::filterNonSWInputs(const std::vector<UTXO> &inputs)
{
   std::vector<UTXO> filteredInputs;
   filteredInputs.reserve(inputs.size());

   for (const auto &input : inputs) {
      if (isSegWit(input)) {
         filteredInputs.emplace_back(input);
      }
   }
   return filteredInputs;
}

size_t SelectedTransactionInputs::GetTransactionsCount() const
{
   return inputs_.size();
}

size_t SelectedTransactionInputs::GetTotalTransactionsCount() const
{
   return GetTransactionsCount() + cpfpInputs_.size();
}

size_t SelectedTransactionInputs::GetSelectedTransactionsCount() const
{
   return totalSelected_;
}

uint64_t SelectedTransactionInputs::GetBalance() const
{
   return useAutoSel_ ? totalBalance_ : selectedBalance_;
}

const UTXO& SelectedTransactionInputs::GetTransaction(size_t i) const
{
   if (i < inputs_.size()) {
      return inputs_[i];
   }
   return cpfpInputs_[i - inputs_.size()];
}

bool SelectedTransactionInputs::IsTransactionSelected(size_t i) const
{
   return selection_[i];
}

void SelectedTransactionInputs::SetTransactionSelection(size_t i, const bool isSelected)
{
   if (isSelected != selection_[i]) {
      selection_[i] = isSelected;
      if (isSelected) {
         ++totalSelected_;
         selectedBalance_ += GetTransaction(i).getValue();
      } else {
         --totalSelected_;
         selectedBalance_ -= GetTransaction(i).getValue();
      }
      if (selectionChanged_) {
         selectionChanged_();
      }
   }
}

bool SelectedTransactionInputs::SetUTXOSelection(const BinaryData &hash, uint32_t txOutIndex, const bool selected)
{
   for (size_t i = 0; i < GetTotalTransactionsCount(); i++) {
      const auto &utxo = GetTransaction(i);
      if ((utxo.getTxHash() == hash) && (txOutIndex == utxo.getTxOutIndex())) {
         SetTransactionSelection(i, selected);
         return true;
      }
   }
   return false;
}

void SelectedTransactionInputs::SetCPFPTransactionSelection(size_t i, const bool isSelected)
{
   SetTransactionSelection(i + GetTransactionsCount(), isSelected);
}

std::vector<UTXO> SelectedTransactionInputs::GetSelectedTransactions() const
{
   if (useAutoSel_) {
      return GetAllTransactions();
   }

   std::vector<UTXO> selectedTransactions;
   selectedTransactions.reserve(totalSelected_);

   for (size_t i=0; i<selection_.size(); ++i) {
      if (selection_[i]) {
         const auto &utxo = GetTransaction(i);
         selectedTransactions.emplace_back(utxo);
      }
   }

   return selectedTransactions;
}

std::vector<UTXO> SelectedTransactionInputs::GetAllTransactions() const
{
   std::vector<UTXO> allTransactions;
   allTransactions.reserve(inputs_.size() + cpfpInputs_.size());
   allTransactions.insert(allTransactions.end(), inputs_.begin(), inputs_.end());
   allTransactions.insert(allTransactions.end(), cpfpInputs_.begin(), cpfpInputs_.end());
   return allTransactions;
}

void SelectedTransactionInputs::SetUseAutoSel(const bool autoSelect)
{
   if (autoSelect != useAutoSel_) {
      useAutoSel_ = autoSelect;
      if (selectionChanged_) {
         selectionChanged_();
      }
   }
}

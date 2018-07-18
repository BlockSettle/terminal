#include "SelectedTransactionInputs.h"
#include "MetaData.h"

SelectedTransactionInputs::SelectedTransactionInputs(const std::shared_ptr<bs::Wallet> &wallet
   , bool swTransactionsOnly, bool confirmedOnly
   , const selectionChangedCallback &selectionChanged)
   : wallet_(wallet)
   , swTransactionsOnly_(swTransactionsOnly)
   , confirmedOnly_(confirmedOnly)
   , selectionChanged_(selectionChanged)
{
   resetInputs([] {});
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
   resetInputs(cbFilter);
}

void SelectedTransactionInputs::SetFixedInputs(const std::vector<UTXO> &inputs)
{
   cpfpInputs_.clear();
   inputs_ = swTransactionsOnly_ ? filterNonSWInputs(inputs) : inputs;

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

void SelectedTransactionInputs::resetInputs(std::function<void()> cb)
{
   inputs_.clear();
   cpfpInputs_.clear();

   const auto &cbSpendable = [this, cb](std::vector<UTXO> inputs) {
      inputs_ = inputs;
      resetSelection();
      cb();
   };
   const auto &cbCPFP = [this, cbSpendable](std::vector<UTXO> inputs) {
      cpfpInputs_ = inputs;
      wallet_->getSpendableTxOutList(cbSpendable);
   };

   if (confirmedOnly_) {
      wallet_->getSpendableTxOutList(cbSpendable);
   }
   else {
      wallet_->getSpendableZCList(cbCPFP);
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
   size_t index = SIZE_MAX;
   for (size_t i = 0; i < GetTotalTransactionsCount(); i++) {
      const auto &utxo = GetTransaction(i);
      if ((utxo.getTxHash() == hash) && (txOutIndex == utxo.getTxOutIndex())) {
         index = i;
         break;
      }
   }
   if (index == SIZE_MAX) {
      return false;
   }
   SetTransactionSelection(index, selected);
   return true;
}

void SelectedTransactionInputs::SetCPFPTransactionSelection(size_t i, const bool isSelected)
{
   SetTransactionSelection(i + GetTransactionsCount(), isSelected);
}

std::vector<UTXO> SelectedTransactionInputs::GetSelectedTransactions() const
{
   std::vector<UTXO> selectedTransactions;
   if (useAutoSel_) {
      return inputs_;
   }

   selectedTransactions.reserve(totalSelected_);

   for (size_t i=0; i<selection_.size(); ++i) {
      if (selection_[i]) {
         const auto &utxo = GetTransaction(i);
         selectedTransactions.emplace_back(utxo);
      }
   }

   return selectedTransactions;
}

bool SelectedTransactionInputs::isSegWit(const UTXO &input) const
{
   return wallet_->IsSegWitInput(input);
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

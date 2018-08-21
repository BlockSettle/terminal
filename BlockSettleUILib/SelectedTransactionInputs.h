#ifndef __SELECTED_TRANSACTION_INPUTS_H__
#define __SELECTED_TRANSACTION_INPUTS_H__

#include <vector>
#include <memory>
#include <functional>

#include "TxClasses.h"

namespace bs {
   class Wallet;
}

class SelectedTransactionInputs
{
public:
   using selectionChangedCallback = std::function<void()>;

public:
   SelectedTransactionInputs(const std::shared_ptr<bs::Wallet> &wallet
      , bool swTransactionsOnly, bool confirmedOnly = false
      , const selectionChangedCallback &selectionChanged = nullptr);
   ~SelectedTransactionInputs() noexcept = default;

   SelectedTransactionInputs(const SelectedTransactionInputs&) = delete;
   SelectedTransactionInputs& operator = (const SelectedTransactionInputs&) = delete;

   SelectedTransactionInputs(SelectedTransactionInputs&&) = delete;
   SelectedTransactionInputs& operator = (SelectedTransactionInputs&&) = delete;

   void SetFixedInputs(const std::vector<UTXO> &inputs);

   bool UseAutoSel() const { return useAutoSel_; }
   void SetUseAutoSel(const bool autoSelect);

   size_t GetTransactionsCount() const;
   size_t GetTotalTransactionsCount() const;
   size_t GetSelectedTransactionsCount() const;
   uint64_t GetTotalBalanceSelected() const { return selectedBalance_; }
   uint64_t GetTotalBalance() const { return totalBalance_; }
   uint64_t GetBalance() const;

   const UTXO& GetTransaction(size_t i) const;
   bool IsTransactionSelected(size_t i) const;
   void SetTransactionSelection(size_t i, const bool selected = true);
   bool SetUTXOSelection(const BinaryData &hash, uint32_t txOutIndex, const bool selected = true);

   void SetCPFPTransactionSelection(size_t i, const bool isSelected);
   const std::vector<UTXO> &GetCPFPInputs() const { return cpfpInputs_; }

   std::vector<UTXO> GetSelectedTransactions() const;
   std::vector<UTXO> GetAllTransactions() const { return inputs_; }

   std::shared_ptr<bs::Wallet> GetWallet() const { return wallet_; }
   void Reload(const std::vector<UTXO> &);

private:
   std::vector<UTXO> filterNonSWInputs(const std::vector<UTXO> &);
   bool filterUTXO(std::vector<UTXO> &inputs, const UTXO &, size_t selectionStart);
   void resetInputs(std::function<void()>);
   void resetSelection();

private:
   std::shared_ptr<bs::Wallet>   wallet_;
   const bool                    swTransactionsOnly_;
   const bool                    confirmedOnly_;
   std::vector<UTXO>             inputs_;
   std::vector<UTXO>             cpfpInputs_;
   std::vector<bool>             selection_;
   const selectionChangedCallback   selectionChanged_;

   size_t   totalSelected_ = 0;
   uint64_t selectedBalance_ = 0;
   uint64_t totalBalance_ = 0;

   bool useAutoSel_ = true;

private:
   bool isSegWit(const UTXO &input) const;
};

#endif // __SELECTED_TRANSACTION_INPUTS_H__

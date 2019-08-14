#ifndef __SELECTED_TRANSACTION_INPUTS_H__
#define __SELECTED_TRANSACTION_INPUTS_H__

#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include <QObject>
#include "TxClasses.h"

namespace bs {
   namespace sync {
      class Wallet;
   }
}

class SelectedTransactionInputs : public QObject
{
   Q_OBJECT
public:
   using CbSelectionChanged = std::function<void()>;

public:
   SelectedTransactionInputs(const std::shared_ptr<bs::sync::Wallet> &wallet
      , bool isSegWitInputsOnly, bool confirmedOnly = false
      , const CbSelectionChanged &selectionChanged = nullptr
      , const std::function<void()> &cbInputsReset = nullptr);
   SelectedTransactionInputs(const std::shared_ptr<bs::sync::Wallet> &
      , const std::vector<UTXO> &, const CbSelectionChanged &selectionChanged = nullptr);
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
   std::vector<UTXO> GetAllTransactions() const;

   std::shared_ptr<bs::sync::Wallet> GetWallet() const { return wallet_; }
   void Reload(const std::vector<UTXO> &);

   void ResetInputs(const std::function<void()> &);

private:
   std::vector<UTXO> filterNonSWInputs(const std::vector<UTXO> &);
   bool filterUTXO(std::vector<UTXO> &inputs, const UTXO &, size_t selectionStart);
   void resetSelection();

private slots:
   void onCPFPReceived(std::vector<UTXO>);
   void onUTXOsReceived(std::vector<UTXO>);

private:
   std::shared_ptr<bs::sync::Wallet>   wallet_;
   const bool                    isSegWitInputsOnly_;
   const bool                    confirmedOnly_;
   std::vector<UTXO>             inputs_;
   std::vector<UTXO>             cpfpInputs_;
   std::vector<bool>             selection_;
   const CbSelectionChanged      selectionChanged_;
   std::vector<std::function<void()>>  resetCallbacks_;

   size_t   totalSelected_ = 0;
   uint64_t selectedBalance_ = 0;
   uint64_t totalBalance_ = 0;

   bool useAutoSel_ = true;
};

#endif // __SELECTED_TRANSACTION_INPUTS_H__

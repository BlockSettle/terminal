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
      namespace hd {
         class Group;
      }
      class Wallet;
   }
}

class SelectedTransactionInputs : public QObject
{
   Q_OBJECT
public:
   using CbSelectionChanged = std::function<void()>;

public:
   SelectedTransactionInputs(const std::shared_ptr<bs::sync::Wallet> &
      , bool isSegWitInputsOnly, bool confirmedOnly = false
      , const CbSelectionChanged &selectionChanged = nullptr
      , const std::function<void()> &cbInputsReset = nullptr);
   SelectedTransactionInputs(const std::shared_ptr<bs::sync::hd::Group> &
      , bool isSegWitInputsOnly, bool confirmedOnly = false
      , const CbSelectionChanged &selectionChanged = nullptr
      , const std::function<void()> &cbInputsReset = nullptr);
   SelectedTransactionInputs(const std::vector<UTXO> &
      , const CbSelectionChanged &selectionChanged = nullptr);
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
   std::map<UTXO, std::string> getSelectedInputs() const;

   std::shared_ptr<bs::sync::Wallet> GetWallet() const
   {
      return wallets_.empty() ? nullptr : wallets_.front();
   }
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const { return wallets_; }

   void Reload(const std::vector<UTXO> &);

   void ResetInputs(const std::function<void()> &);

private:
   std::vector<UTXO> filterNonSWInputs(const std::vector<UTXO> &);
   bool filterUTXO(std::vector<UTXO> &inputs, const UTXO &, size_t selectionStart);
   void resetSelection();

private slots:
   void onCPFPReceived(const std::shared_ptr<bs::sync::Wallet> &, std::vector<UTXO>);
   void onUTXOsReceived(const std::shared_ptr<bs::sync::Wallet> &, std::vector<UTXO>);

private:
   std::vector<std::shared_ptr<bs::sync::Wallet>>  wallets_;
   const bool                    isSegWitInputsOnly_;
   const bool                    confirmedOnly_;
   std::vector<UTXO>             inputs_;
   std::vector<UTXO>             cpfpInputs_;
   std::map<UTXO, std::string>   inputsMap_;
   std::vector<bool>             selection_;
   const CbSelectionChanged      selectionChanged_;
   std::vector<std::function<void()>>  resetCallbacks_;
   std::map<std::string, std::vector<UTXO>>  accInputs_;
   std::map<std::string, std::vector<UTXO>>  accCpfpInputs_;

   size_t   totalSelected_ = 0;
   uint64_t selectedBalance_ = 0;
   uint64_t totalBalance_ = 0;

   bool useAutoSel_ = true;
};

#endif // __SELECTED_TRANSACTION_INPUTS_H__

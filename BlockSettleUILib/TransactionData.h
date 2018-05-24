#ifndef __TRANSACTION_DATA_H__
#define __TRANSACTION_DATA_H__

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "Address.h"
#include "MetaData.h"
#include "UtxoReservation.h"


class CoinSelection;
class RecipientContainer;
class ScriptRecipient;
class SelectedTransactionInputs;

namespace bs {
   class Wallet;
}

class TransactionData
{
public:
   struct TransactionSummary
   {
      bool     initialized;
      // usedTransactions - count of utxo that will be used in transaction
      size_t   usedTransactions;

      // availableBalance - total available balance. in case of manual selection - same as selectedBalance
      double   availableBalance;

      // selectedBalance - balance of selected inputs
      double   selectedBalance;

      // balanceToSpent - total amount received by recipients
      double   balanceToSpent;

      size_t   transactionSize;
      uint64_t totalFee;
      double   feePerByte;

      bool     hasChange;
      bool     isAutoSelected;
   };

   using onTransactionChanged = std::function<void()>;

public:
   TransactionData(onTransactionChanged changedCallback = onTransactionChanged()
      , bool SWOnly = false, bool confirmedOnly = false);
   ~TransactionData() noexcept;

   TransactionData(const TransactionData&) = delete;
   TransactionData& operator = (const TransactionData&) = delete;

   TransactionData(TransactionData&&) = delete;
   TransactionData& operator = (TransactionData&&) = delete;

   bool SetWallet(const std::shared_ptr<bs::Wallet>& wallet);
   void SetSigningWallet(const std::shared_ptr<bs::Wallet>& wallet) { signWallet_ = wallet; }
   std::shared_ptr<bs::Wallet> GetWallet() const { return wallet_; }
   std::shared_ptr<bs::Wallet> GetSigningWallet() const { return signWallet_; }
   bool SetFeePerByte(float feePerByte);
   float FeePerByte() const { return feePerByte_; }
   void SetTotalFee(uint64_t fee);

   bool IsTransactionValid() const;

   size_t GetRecipientsCount() const;
   std::vector<unsigned int> GetRecipientIdList() const;

   unsigned int RegisterNewRecipient();
   bool UpdateRecipientAddress(unsigned int recipientId, const bs::Address &);
   bool UpdateRecipientAddress(unsigned int recipientId, const std::shared_ptr<AddressEntry> &);
   void ResetRecipientAddress(unsigned int recipientId);
   bool UpdateRecipientAmount(unsigned int recipientId, double amount, bool isMax = false);
   bool UpdateRecipient(unsigned int recipientId, double amount, const bs::Address &);
   void RemoveRecipient(unsigned int recipientId);

   void SetFallbackRecvAddress(const bs::Address &addr) { fallbackRecvAddress_ = addr; }
   bs::Address GetFallbackRecvAddress();

   bs::Address GetRecipientAddress(unsigned int recipientId) const;
   BTCNumericTypes::balance_type GetRecipientAmount(unsigned int recipientId) const;
   bool IsMaxAmount(unsigned int recipientId) const;

   bs::wallet::TXSignRequest CreateUnsignedTransaction(bool isRBF = false, const bs::Address &changeAddr = {});
   bs::wallet::TXSignRequest GetSignTXRequest() const;

   bs::wallet::TXSignRequest CreateTXRequest(bool isRBF = false, const bs::Address &changeAddr = {}) const;
   bs::wallet::TXSignRequest CreatePartialTXRequest(uint64_t spendVal, float feePerByte
      , const std::vector<std::shared_ptr<ScriptRecipient>> &, const BinaryData &prevData);

   std::shared_ptr<SelectedTransactionInputs> GetSelectedInputs();
   TransactionSummary GetTransactionSummary() const;

   double CalculateMaxAmount(const bs::Address &recipient = {}) const;

   void ReserveUtxosFor(double amount, const std::string &reserveId, const bs::Address &addr = {});
   void ReloadSelection(const std::vector<UTXO> &);

   void createAddress(const bs::Address &addr, const std::shared_ptr<bs::Wallet> &wallet = nullptr);
   const std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> &createAddresses() const {
      return createAddresses_;
   }

   void clear();
   std::vector<UTXO> inputs() const;

   void setMaxSpendAmount(bool maxAmount = true);
   bool maxSpendAmount() const { return maxSpendAmount_; }

private:
   void InvalidateTransactionData();
   bool UpdateTransactionData();
   bool RecipientsReady() const;

   std::vector<std::shared_ptr<ScriptRecipient>> GetRecipientList() const;

private:
   std::shared_ptr<bs::Wallet>   wallet_, signWallet_;

   std::shared_ptr<SelectedTransactionInputs> selectedInputs_;

   float       feePerByte_;
   uint64_t    totalFee_ = 0;
   // recipients
   unsigned int nextId_;
   std::unordered_map<unsigned int, std::shared_ptr<RecipientContainer>> recipients_;
   mutable bs::Address              fallbackRecvAddress_;
   std::shared_ptr<CoinSelection>   coinSelection_;

   std::vector<UTXO>    usedUTXO_;
   TransactionSummary   summary_;
   bool     maxSpendAmount_ = false;

   bs::wallet::TXSignRequest  unsignedTxReq_;

   const bool  swTransactionsOnly_;
   const bool  confirmedInputs_;

   onTransactionChanged changedCallback_;

   std::vector<UTXO>    reservedUTXO_;
   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>>  createAddresses_;
};

#endif // __TRANSACTION_DATA_H__

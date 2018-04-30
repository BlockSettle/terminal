#include "TransactionData.h"

#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "SwigClient.h"
#include "SelectedTransactionInputs.h"
#include "ScriptRecipient.h"
#include "SettlementWallet.h"
#include "RecipientContainer.h"
#include "PyBlockDataManager.h"
#include "UiUtils.h"

#include <vector>
#include <map>

#include <QDebug>

TransactionData::TransactionData(onTransactionChanged changedCallback, bool SWOnly, bool confOnly)
   : wallet_(nullptr)
   , selectedInputs_(nullptr)
   , feePerByte_(0)
   , nextId_(0)
   , coinSelection_(nullptr)
   , swTransactionsOnly_(SWOnly)
   , confirmedInputs_(confOnly)
   , changedCallback_(changedCallback)
{}

TransactionData::~TransactionData()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

bool TransactionData::SetWallet(const std::shared_ptr<bs::Wallet> &wallet)
{
   if (wallet == nullptr) {
      return false;
   }
   if (wallet != wallet_) {
      wallet_ = wallet;
      selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_
         , swTransactionsOnly_, confirmedInputs_
         , [this]{InvalidateTransactionData();});

      coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
            return this->selectedInputs_->GetSelectedTransactions();
         }
         , std::vector<AddressBookEntry>{}
         , static_cast<unsigned int>(PyBlockDataManager::instance()->GetTopBlockHeight())
         , static_cast<uint64_t>(wallet_->GetTotalBalance() * BTCNumericTypes::BalanceDivider));
      InvalidateTransactionData();
   }

   return true;
}

std::shared_ptr<SelectedTransactionInputs> TransactionData::GetSelectedInputs()
{
   return selectedInputs_;
}

TransactionData::TransactionSummary TransactionData::GetTransactionSummary() const
{
   return summary_;
}

void TransactionData::InvalidateTransactionData()
{
   usedUTXO_.clear();
   memset((void*)&summary_, 0, sizeof(summary_));

   UpdateTransactionData();
   if (changedCallback_) {
      changedCallback_();
   }
}

bool TransactionData::UpdateTransactionData()
{
   if (selectedInputs_ == nullptr) {
      return false;
   }

   auto transactions = selectedInputs_->GetSelectedTransactions();
   if (utxoAdapter_ && reservedUTXO_.empty()) {
      utxoAdapter_->filter(selectedInputs_->GetWallet()->GetWalletId(), transactions);
   }

   for (auto& utxo : transactions) {
      utxo.txinRedeemSizeBytes_ = bs::wallet::getInputScrSize(wallet_->getAddressEntryForAddr(utxo.getRecipientScrAddr()));
   }

   uint64_t availableBalance = 0;
   for (const auto &tx : transactions) {
      availableBalance += tx.getValue();
   }

   summary_.availableBalance = UiUtils::amountToBtc(availableBalance);
   bool maxAmount = true;
   std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipientsMap;
   if (RecipientsReady()) {
      for (const auto& it : recipients_) {
         if (!it.second->IsReady()) {
            return false;
         }
         maxAmount &= it.second->IsMaxAmount();
         recipientsMap.emplace(it.first, it.second->GetScriptRecipient());
      }
   }

   if (selectedInputs_->UseAutoSel()) {
      summary_.isAutoSelected = true;

      if (!recipientsMap.empty()) {
         PaymentStruct payment = !qFuzzyIsNull(feePerByte_)
            ? PaymentStruct(recipientsMap, 0, feePerByte_, 0)
            : PaymentStruct(recipientsMap, totalFee_, 0, 0);
         summary_.balanceToSpent = UiUtils::amountToBtc(payment.spendVal_);


         if (payment.spendVal_ < availableBalance) {
            UtxoSelection selection;
            try {
               selection = coinSelection_->getUtxoSelectionForRecipients(payment, transactions);
            } catch (const std::runtime_error& err) {
               qDebug() << "UpdateTransactionData coinSelection exception: " << err.what();
               return false;
            } catch (...) {
               qDebug() << "UpdateTransactionData coinSelection exception";
               return false;
            }

            usedUTXO_ = selection.utxoVec_;

            summary_.transactionSize = selection.size_;
            summary_.totalFee = selection.fee_;
            summary_.feePerByte = selection.fee_byte_;

            summary_.hasChange = selection.hasChange_ && !maxAmount;

            summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);
         }
      }
   } else {
      summary_.isAutoSelected = false;
      usedUTXO_ = transactions;
      if (!recipientsMap.empty()) {
         PaymentStruct payment = !qFuzzyIsNull(feePerByte_)
            ? PaymentStruct(recipientsMap, 0, feePerByte_, 0)
            : PaymentStruct(recipientsMap, totalFee_, 0, 0);

         summary_.balanceToSpent = UiUtils::amountToBtc(payment.spendVal_);

         if (payment.spendVal_ < availableBalance) {
            auto usedUTXOCopy{usedUTXO_};
            UtxoSelection selection{usedUTXOCopy};
            try {
               selection.computeSizeAndFee(payment);
            } catch (const std::runtime_error& err) {
               qDebug() << "UpdateTransactionData UtxoSelection exception: " << err.what();
               return false;
            } catch (...) {
               qDebug() << "UpdateTransactionData UtxoSelection exception";
               return false;
            }

            summary_.transactionSize = selection.size_;
            summary_.totalFee = selection.fee_;
            summary_.feePerByte = selection.fee_byte_;

            summary_.hasChange = selection.hasChange_ && !maxAmount;

            summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);
         }
      }
   }

   summary_.usedTransactions = usedUTXO_.size();
   summary_.initialized = true;

   return true;
}

double TransactionData::CalculateMaxAmount(const bs::Address &recipient) const
{
   if ((selectedInputs_ == nullptr) || (wallet_ == nullptr)) {
      return -1;
   }
   double fee = totalFee_;
   if (fee <= 0) {
      auto transactions = selectedInputs_->GetSelectedTransactions();
      if (transactions.empty()) {
         transactions = selectedInputs_->GetAllTransactions();
      }
      if (transactions.empty()) {
         return 0;
      }
      for (auto& utxo : transactions) {
         utxo.txinRedeemSizeBytes_ = bs::wallet::getInputScrSize(wallet_->getAddressEntryForAddr(utxo.getRecipientScrAddr()));
      }

      size_t txOutSize = 0;
      for (const auto &recip : recipients_) {
         const auto &scrRecip = recip.second->GetScriptRecipient();
         txOutSize += scrRecip ? scrRecip->getSize() : 31;
      }
      if (!recipient.isNull()) {
         const auto &scrRecip = recipient.getRecipient(0.0);
         txOutSize += scrRecip ? scrRecip->getSize() : 31;
      }

      fee = coinSelection_->getFeeForMaxVal(txOutSize, feePerByte_, transactions);
   }
   fee = fee / BTCNumericTypes::BalanceDivider;

   auto availableBalance = GetTransactionSummary().availableBalance - GetTransactionSummary().balanceToSpent;

   if (availableBalance < fee) {
      return 0;
   } else {
      return availableBalance - fee;
   }
}

bool TransactionData::RecipientsReady() const
{
   if (recipients_.empty()) {
      return false;
   }

   for (const auto& it : recipients_) {
      if (!it.second->IsReady()) {
         return false;
      }
   }

   return true;
}

bool TransactionData::SetFeePerByte(float feePerByte)
{
   feePerByte_ = feePerByte;
   totalFee_ = 0;
   InvalidateTransactionData();
   return true;
}

void TransactionData::SetTotalFee(uint64_t fee)
{
   totalFee_ = fee;
   feePerByte_ = 0;
   InvalidateTransactionData();
}

void TransactionData::ReserveUtxosFor(double amount, const std::string &reserveId, const bs::Address &addr)
{
   if (!utxoAdapter_) {
      utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
      bs::UtxoReservation::addAdapter(utxoAdapter_);
   }
   reservedUTXO_.clear();
   utxoAdapter_->unreserve(reserveId);

   if (!addr.isNull() && !GetRecipientsCount()) {
      const auto recip = RegisterNewRecipient();
      UpdateRecipient(recip, amount, addr);
      reservedUTXO_ = usedUTXO_;
      RemoveRecipient(recip);
   }
   else {
      reservedUTXO_ = usedUTXO_;
   }
   if (!reservedUTXO_.empty()) {
      utxoAdapter_->reserve(wallet_->GetWalletId(), reserveId, reservedUTXO_);
   }
}

void TransactionData::ReloadSelection(const std::vector<UTXO> &utxos)
{
   if (!selectedInputs_) {
      return;
   }
   selectedInputs_->Reload(utxos);
   InvalidateTransactionData();
}

void TransactionData::clear()
{
   totalFee_ = 0;
   feePerByte_ = 0;
   recipients_.clear();
   usedUTXO_.clear();
   reservedUTXO_.clear();
   summary_ = {};
   fallbackRecvAddress_ = {};
   createAddresses_.clear();
}

std::vector<UTXO> TransactionData::inputs() const
{
   if (reservedUTXO_.empty()) {
      return usedUTXO_;
   }
   return reservedUTXO_;
}

bool TransactionData::IsTransactionValid() const
{
   return (wallet_ != nullptr) 
      && (selectedInputs_ != nullptr)
      && summary_.usedTransactions != 0
      && (!qFuzzyIsNull(feePerByte_) || totalFee_ != 0) && RecipientsReady();
}

size_t TransactionData::GetRecipientsCount() const
{
   return recipients_.size();
}

unsigned int TransactionData::RegisterNewRecipient()
{
   unsigned int id = nextId_;
   ++nextId_;

   auto newRecipient = std::make_shared<RecipientContainer>();
   recipients_.emplace(id, newRecipient);

   return id;
}

void TransactionData::RemoveRecipient(unsigned int recipientId)
{
   recipients_.erase(recipientId);
   InvalidateTransactionData();
}

bool TransactionData::UpdateRecipientAddress(unsigned int recipientId, const bs::Address &address)
{
   if (wallet_) {
      const auto &recptAddrEntry = wallet_->getAddressEntryForAddr(address);
      if (recptAddrEntry != nullptr) {
         return UpdateRecipientAddress(recipientId, recptAddrEntry);
      }
   }

   auto it = recipients_.find(recipientId);
   if (it == recipients_.end()) {
      return false;
   }

   bool result = it->second->SetAddress(address);
   if (result) {
      InvalidateTransactionData();
   }

   return result;
}

bool TransactionData::UpdateRecipientAddress(unsigned int recipientId, const std::shared_ptr<AddressEntry> &address)
{
   auto it = recipients_.find(recipientId);
   if (it == recipients_.end()) {
      return false;
   }

   bool result = it->second->SetAddressEntry(address);
   if (result) {
      InvalidateTransactionData();
   }
   return result;
}

void TransactionData::ResetRecipientAddress(unsigned int recipientId)
{
   auto it = recipients_.find(recipientId);
   if (it != recipients_.end()) {
      it->second->ResetAddress();
   }
}

bool TransactionData::UpdateRecipient(unsigned int recipientId, double amount, const bs::Address &address)
{
   auto it = recipients_.find(recipientId);
   if (it == recipients_.end()) {
      return false;
   }

   const bool result = it->second->SetAddress(address) & it->second->SetAmount(amount);
   if (result) {
      InvalidateTransactionData();
   }
   return result;
}

bool TransactionData::UpdateRecipientAmount(unsigned int recipientId, double amount, bool isMax)
{
   auto it = recipients_.find(recipientId);
   if (it == recipients_.end()) {
      return false;
   }

   bool result = it->second->SetAmount(amount, isMax);
   if (result) {
      InvalidateTransactionData();
   }

   return result;
}

std::vector<unsigned int> TransactionData::GetRecipientIdList() const
{
   std::vector<unsigned int> idList;
   idList.reserve(recipients_.size());
   for (const auto& it : recipients_) {
      idList.emplace_back(it.first);
   }

   return idList;
}

bs::Address TransactionData::GetRecipientAddress(unsigned int recipientId) const
{
   const auto &itRecip = recipients_.find(recipientId);
   if (itRecip == recipients_.end()) {
      return bs::Address();
   }
   return itRecip->second->GetAddress();
}

BTCNumericTypes::balance_type TransactionData::GetRecipientAmount(unsigned int recipientId) const
{
   const auto &itRecip = recipients_.find(recipientId);
   if (itRecip == recipients_.end()) {
      return 0;
   }
   return itRecip->second->GetAmount();
}

bool TransactionData::IsMaxAmount(unsigned int recipientId) const
{
   const auto &itRecip = recipients_.find(recipientId);
   if (itRecip == recipients_.end()) {
      return false;
   }
   return itRecip->second->IsMaxAmount();
}

bs::Address TransactionData::GetFallbackRecvAddress() const
{
   if (!fallbackRecvAddress_.isNull()) {
      return fallbackRecvAddress_;
   }
   if (wallet_ != nullptr) {
      const auto addr = wallet_->GetNewExtAddress();
      wallet_->RegisterWallet();
      fallbackRecvAddress_ = addr;
   }
   return fallbackRecvAddress_;
}

std::vector<std::shared_ptr<ScriptRecipient>> TransactionData::GetRecipientList() const
{
   if (!IsTransactionValid()) {
      throw std::logic_error("transaction is invalid");
   }
   if (inputs().empty()) {
      throw std::logic_error("missing inputs");
   }

   std::vector<std::shared_ptr<ScriptRecipient>> recipientList;
   for (const auto& it : recipients_) {
      if (!it.second->IsReady()) {
         throw std::logic_error("recipient[s] not ready");
      }
      recipientList.emplace_back(it.second->GetScriptRecipient());
   }

   return recipientList;
}

bs::wallet::TXSignRequest TransactionData::CreateUnsignedTransaction(bool isRBF, const bs::Address &changeAddress)
{
   unsignedTxReq_ = wallet_->CreateTXRequest(inputs(), GetRecipientList(), summary_.totalFee, isRBF, changeAddress);
   if (!unsignedTxReq_.isValid()) {
      throw std::runtime_error("missing unsigned TX");
   }
   reservedUTXO_ = unsignedTxReq_.inputs;
   return unsignedTxReq_;
}

bs::wallet::TXSignRequest TransactionData::GetSignTXRequest() const
{
   if (!unsignedTxReq_.isValid()) {
      throw std::runtime_error("missing unsigned TX");
   }
   return unsignedTxReq_;
}

bs::wallet::TXSignRequest TransactionData::CreateTXRequest(bool isRBF, const bs::Address &changeAddr) const
{
   return wallet_->CreateTXRequest(inputs(), GetRecipientList(), summary_.totalFee, isRBF, changeAddr);
}

bs::wallet::TXSignRequest TransactionData::CreatePartialTXRequest(uint64_t spendVal, float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients, const BinaryData &prevData)
{
   const auto &changeAddr = wallet_->GetNewChangeAddress();
   createAddress(changeAddr);
   auto txReq = wallet_->CreatePartialTXRequest(spendVal, inputs(), changeAddr, feePerByte, recipients, prevData);
   txReq.populateUTXOs = true;
   return txReq;
}

void TransactionData::createAddress(const bs::Address &addr, const std::shared_ptr<bs::Wallet> &wallet)
{
   if (!wallet) {
      createAddresses_.push_back({wallet_, addr});
   }
   else {
      createAddresses_.push_back({ wallet, addr });
   }
}

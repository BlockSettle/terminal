#include "TransactionData.h"

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "SwigClient.h"
#include "SelectedTransactionInputs.h"
#include "ScriptRecipient.h"
#include "SettlementWallet.h"
#include "RecipientContainer.h"
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
   disableTransactionUpdate();
   changedCallback_ = {};
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void TransactionData::SetCallback(onTransactionChanged changedCallback)
{
   changedCallback_ = changedCallback;
}

bool TransactionData::InputsLoadedFromArmory() const
{
   return inputsLoaded_;
}

bool TransactionData::SetWallet(const std::shared_ptr<bs::Wallet> &wallet, uint32_t topBlock
   , bool resetInputs, const std::function<void()> &cbInputsReset)
{
   if (wallet == nullptr) {
      return false;
   }
   if (wallet != wallet_) {
      wallet_ = wallet;
      inputsLoaded_ = false;

      selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_
         , swTransactionsOnly_, confirmedInputs_
         , [this]()
         {
            inputsLoaded_ = true;
            InvalidateTransactionData();
         }, cbInputsReset);

      coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
            return this->selectedInputs_->GetSelectedTransactions();
         }
         , std::vector<AddressBookEntry>{}
         , static_cast<uint64_t>(wallet_->GetSpendableBalance() * BTCNumericTypes::BalanceDivider)
         , topBlock);
      InvalidateTransactionData();
   }
   else if (resetInputs) {
      if (selectedInputs_) {
         selectedInputs_->ResetInputs(cbInputsReset);
      }
      else {
         selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_
            , swTransactionsOnly_, confirmedInputs_
            , [this] { InvalidateTransactionData(); }, cbInputsReset);
      }
      InvalidateTransactionData();
   }

   return true;
}

bool TransactionData::SetWalletAndInputs(const std::shared_ptr<bs::Wallet> &wallet, const std::vector<UTXO> &utxos
   , uint32_t topBlock)
{
   if (wallet == nullptr) {
      return false;
   }
   wallet_ = wallet;
   selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_, utxos
      , [this] {InvalidateTransactionData(); });

   coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
      return this->selectedInputs_->GetSelectedTransactions();
   }
      , std::vector<AddressBookEntry>{}
   , static_cast<uint64_t>(wallet_->GetSpendableBalance() * BTCNumericTypes::BalanceDivider)
      , topBlock);
   InvalidateTransactionData();

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

   if (transactionUpdateEnabled_) {
      if (changedCallback_) {
         changedCallback_();
      }
   } else {
      transactionUpdateRequired_ = true;
   }
}

bool TransactionData::disableTransactionUpdate()
{
   bool prevState = transactionUpdateEnabled_;
   transactionUpdateEnabled_ = false;
   return prevState;
}

void TransactionData::enableTransactionUpdate()
{
   transactionUpdateEnabled_ = true;
   if (transactionUpdateRequired_) {
      transactionUpdateRequired_ = false;
      InvalidateTransactionData();
   }
}

bool TransactionData::UpdateTransactionData()
{
   if (selectedInputs_ == nullptr) {
      return false;
   }

   std::vector<UTXO> transactions;
   if (!selectedInputs_->UseAutoSel()) {
      transactions = selectedInputs_->GetSelectedTransactions();
   }
   else {
      transactions = selectedInputs_->GetAllTransactions();
   }
   if (utxoAdapter_ && reservedUTXO_.empty()) {
      utxoAdapter_->filter(selectedInputs_->GetWallet()->GetWalletId(), transactions);
   }

   // The for loop is equivalent to CoinSelectionInstance::decorateUTXOs() in
   // Armory. We need it for proper initialization of the UTXO structs when
   // computing TX sizes and fees. All inputs should use SegWit.
   for (auto& utxo : transactions) {
      utxo.txinRedeemSizeBytes_ = 0;
      utxo.witnessDataSizeBytes_ = 0;
      utxo.isInputSW_ = false;

      auto aefa = wallet_->getAddressEntryForAddr(utxo.getRecipientScrAddr());
      if (aefa != nullptr) {
         while (true) {
            utxo.txinRedeemSizeBytes_ += aefa->getInputSize();

            // P2SH AddressEntry objects use nesting to determine the exact
            // P2SH type. The initial P2SH-W2WPKH AddressEntry object (and any
            // non-SegWit AddressEntry objects) won't have witness data. That's
            // fine. Catch the error and keep going.
            try {
               utxo.witnessDataSizeBytes_ += aefa->getWitnessDataSize();
               utxo.isInputSW_ = true;
            }
            catch (const std::runtime_error& re) {}

            // Check for a predecessor, which P2SH-P2PWKH will have. This is how
            // we learn if the original P2SH AddressEntry object uses SegWit.
            auto addrNested = std::dynamic_pointer_cast<AddressEntry_Nested>(aefa);
            if (addrNested == nullptr) {
               break;
            }
            aefa = addrNested->getPredecessor();
         } // while
      } // if
   } // for

   uint64_t availableBalance = 0;
   for (const auto &tx : transactions) {
      availableBalance += tx.getValue();
   }

   summary_.availableBalance = UiUtils::amountToBtc(availableBalance);
   summary_.isAutoSelected = selectedInputs_->UseAutoSel();

   bool maxAmount = true;
   std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipientsMap;
   if (RecipientsReady()) {
      for (const auto& it : recipients_) {
         if (!it.second->IsReady()) {
            return false;
         }
         maxAmount &= it.second->IsMaxAmount();
         const auto &recip = it.second->GetScriptRecipient();
         if (!recip) {
            return false;
         }
         recipientsMap.emplace(it.first, recip);
      }
   }
   if (recipientsMap.empty()) {
      return false;
   }

   PaymentStruct payment = !qFuzzyIsNull(feePerByte_)
      ? PaymentStruct(recipientsMap, 0, feePerByte_, 0)
      : PaymentStruct(recipientsMap, totalFee_, 0, 0);
   summary_.balanceToSpend = UiUtils::amountToBtc(payment.spendVal_);

   if (payment.spendVal_ <= availableBalance) {
      if (maxAmount) {
         std::vector<std::shared_ptr<ScriptRecipient>> recipients;
         for (const auto &recip : recipientsMap) {
            recipients.push_back(recip.second);
         }
         usedUTXO_ = transactions;
         summary_.txVirtSize = bs::wallet::estimateTXVirtSize(usedUTXO_, recipients);
         summary_.totalFee = availableBalance - payment.spendVal_;
         totalFee_ = summary_.totalFee;
         summary_.feePerByte =
            std::round((float)summary_.totalFee / (float)summary_.txVirtSize);
         summary_.hasChange = false;
         summary_.selectedBalance = UiUtils::amountToBtc(availableBalance);
      }
      else if (selectedInputs_->UseAutoSel()) {
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
         summary_.txVirtSize = getVirtSize(selection);
         summary_.totalFee = selection.fee_;
         summary_.feePerByte = selection.fee_byte_;
         summary_.hasChange = selection.hasChange_;
         summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);
      }
      else {
         usedUTXO_ = transactions;

         auto usedUTXOCopy{ usedUTXO_ };
         UtxoSelection selection{ usedUTXOCopy };
         try {
            selection.computeSizeAndFee(payment);
         }
         catch (const std::runtime_error& err) {
            qDebug() << "UpdateTransactionData UtxoSelection exception: " << err.what();
            return false;
         }
         catch (...) {
            qDebug() << "UpdateTransactionData UtxoSelection exception";
            return false;
         }

         summary_.txVirtSize = getVirtSize(selection);
         summary_.totalFee = selection.fee_;
         summary_.feePerByte = selection.fee_byte_;

         summary_.hasChange = selection.hasChange_;

         summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);
      }
      summary_.usedTransactions = usedUTXO_.size();
   }

   summary_.outputsCount = recipients_.size();
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
      std::vector<UTXO> transactions;
      if (selectedInputs_->GetSelectedTransactionsCount()) {
         transactions = selectedInputs_->GetSelectedTransactions();
      }
      if (transactions.empty()) {
         transactions = selectedInputs_->GetAllTransactions();
      }
      if (transactions.empty()) {
         return 0;
      }
      for (auto& utxo : transactions) {
         const auto addrEntry = wallet_->getAddressEntryForAddr(utxo.getRecipientScrAddr());
         if (addrEntry) {
            utxo.txinRedeemSizeBytes_ = bs::wallet::getInputScrSize(addrEntry);
         }
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
      fee += 70;     // a small epsilon to make UtxoSelection happy
   }
   fee = fee / BTCNumericTypes::BalanceDivider;

   auto availableBalance = GetTransactionSummary().availableBalance - GetTransactionSummary().balanceToSpend;

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

// A temporary private function that calculates the virtual size of an incoming
// UtxoSelection object. This needs to be removed when a particular PR
// (https://github.com/goatpig/BitcoinArmory/pull/538) is accepted upstream.
// Note that this function assumes SegWit will be used. It's fine for our
// purposes but it's a bad assumption in general.
size_t TransactionData::getVirtSize(const UtxoSelection& inUTXOSel)
{
   size_t nonWitSize = inUTXOSel.size_ - inUTXOSel.witnessSize_;
   return std::ceil(static_cast<float>(3*nonWitSize + inUTXOSel.size_) / 4.0f);
}

void TransactionData::SetFeePerByte(float feePerByte)
{
   feePerByte_ = feePerByte;
   totalFee_ = 0;
   InvalidateTransactionData();
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

void TransactionData::ClearAllRecipients()
{
   if (!recipients_.empty()) {
      recipients_.clear();
      InvalidateTransactionData();
   }
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

void TransactionData::setMaxSpendAmount(bool maxAmount)
{
   maxSpendAmount_ = maxAmount;
   if (maxAmount) {
      summary_.hasChange = false;
   }
}


bs::Address TransactionData::GetFallbackRecvAddress()
{
   if (!fallbackRecvAddress_.isNull()) {
      return fallbackRecvAddress_;
   }
   if (wallet_ != nullptr) {
      const auto addr = wallet_->GetNewExtAddress();
      createAddress(addr, wallet_);
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

bs::wallet::TXSignRequest TransactionData::CreateTXRequest(bool isRBF
                                                 , const bs::Address &changeAddr
                                                , const uint64_t& origFee) const
{
   return wallet_->CreateTXRequest(inputs(), GetRecipientList()
                                   , summary_.totalFee, isRBF, changeAddr
                                   , origFee);
}

bs::wallet::TXSignRequest TransactionData::CreatePartialTXRequest(uint64_t spendVal, float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients, const BinaryData &prevData
   , const std::vector<UTXO> &utxos)
{
   const auto &changeAddr = wallet_->GetNewChangeAddress();
   createAddress(changeAddr);
   auto txReq = wallet_->CreatePartialTXRequest(spendVal, utxos.empty() ? inputs() : utxos
      , changeAddr, feePerByte, recipients, prevData);
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

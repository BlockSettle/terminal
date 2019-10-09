#include "TransactionData.h"

#include "ArmoryConnection.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "SwigClient.h"
#include "SelectedTransactionInputs.h"
#include "ScriptRecipient.h"
#include "RecipientContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncHDGroup.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <vector>
#include <map>
#include <spdlog/spdlog.h>

static const size_t kMaxTxStdWeight = 400000;


TransactionData::TransactionData(const onTransactionChanged &changedCallback
   , const std::shared_ptr<spdlog::logger> &logger , bool isSegWitInputsOnly, bool confOnly)
   : changedCallback_(changedCallback)
   , logger_(logger)
   , feePerByte_(0)
   , nextId_(0)
   , isSegWitInputsOnly_(isSegWitInputsOnly)
   , confirmedInputs_(confOnly)
{}

TransactionData::~TransactionData() noexcept
{
   disableTransactionUpdate();
   changedCallback_ = {};
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void TransactionData::SetCallback(onTransactionChanged changedCallback)
{
   changedCallback_ = std::move(changedCallback);
}

bool TransactionData::InputsLoadedFromArmory() const
{
   return inputsLoaded_;
}

bool TransactionData::setWallet(const std::shared_ptr<bs::sync::Wallet> &wallet
   , uint32_t topBlock, bool resetInputs, const std::function<void()> &cbInputsReset)
{
   if (!wallet) {
      return false;
   }
   if (wallet != wallet_) {
      wallet_ = wallet;
      inputsLoaded_ = false;

      selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_
         , isSegWitInputsOnly_, confirmedInputs_
         , [this]() {
         inputsLoaded_ = true;
         InvalidateTransactionData();
      }, cbInputsReset);

      coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
         return selectedInputs_->GetSelectedTransactions();
      }
         , std::vector<AddressBookEntry>{}
      , static_cast<uint64_t>(wallet->getSpendableBalance() * BTCNumericTypes::BalanceDivider)
         , topBlock);
      InvalidateTransactionData();
   }
   else if (resetInputs) {
      if (selectedInputs_) {
         selectedInputs_->ResetInputs(cbInputsReset);
      }
      else {
         selectedInputs_ = std::make_shared<SelectedTransactionInputs>(wallet_
            , isSegWitInputsOnly_, confirmedInputs_
            , [this] { InvalidateTransactionData(); }
         , cbInputsReset);
      }
      InvalidateTransactionData();
   }
   return true;
}

bool TransactionData::setGroup(const std::shared_ptr<bs::sync::hd::Group> &group
   , uint32_t topBlock, bool resetInputs, const std::function<void()> &cbInputsReset)
{
   if (!group) {
      return false;
   }
   if (group != group_) {
      wallet_ = nullptr;
      group_ = group;
      const auto leaves = group->getAllLeaves();
      if (!leaves.empty()) {
         wallet_ = leaves.front();
      }
      inputsLoaded_ = false;

      BTCNumericTypes::balance_type spendableBalance = 0;
      for (const auto &leaf : leaves) {
         spendableBalance += leaf->getSpendableBalance();
      }

      selectedInputs_ = std::make_shared<SelectedTransactionInputs>(group_
         , isSegWitInputsOnly_, confirmedInputs_
         , [this]() {
         inputsLoaded_ = true;
         InvalidateTransactionData();
      }, cbInputsReset);

      coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
         return selectedInputs_->GetSelectedTransactions();
      }
         , std::vector<AddressBookEntry>{}
         , static_cast<uint64_t>(spendableBalance * BTCNumericTypes::BalanceDivider)
         , topBlock);
      InvalidateTransactionData();
   } else if (resetInputs) {
      if (selectedInputs_) {
         selectedInputs_->ResetInputs(cbInputsReset);
      } else {
         selectedInputs_ = std::make_shared<SelectedTransactionInputs>(group_
            , isSegWitInputsOnly_, confirmedInputs_
            , [this] { InvalidateTransactionData(); }
         , cbInputsReset);
      }
      InvalidateTransactionData();
   }
   return true;
}
bool TransactionData::setGroupAndInputs(const std::shared_ptr<bs::sync::hd::Group> &group
   , const std::vector<UTXO> &utxos, uint32_t topBlock)
{
   wallet_.reset();
   if (!group) {
      return false;
   }
   const auto leaves = group->getAllLeaves();
   if (leaves.empty()) {
      return false;
   }
   group_ = group;
   return setWalletAndInputs(leaves.front(), utxos, topBlock);
}

bool TransactionData::setWalletAndInputs(const std::shared_ptr<bs::sync::Wallet> &wallet
   , const std::vector<UTXO> &utxos, uint32_t topBlock)
{
   if (!wallet) {
      return false;
   }
   wallet_ = wallet;

   selectedInputs_ = std::make_shared<SelectedTransactionInputs>(
      utxos, [this] { InvalidateTransactionData(); });

   coinSelection_ = std::make_shared<CoinSelection>([this](uint64_t) {
      return selectedInputs_->GetSelectedTransactions();
   }
      , std::vector<AddressBookEntry>{}
   , static_cast<uint64_t>(wallet->getSpendableBalance() * BTCNumericTypes::BalanceDivider)
      , topBlock);
   InvalidateTransactionData();
   return true;
}

TransactionData::TransactionSummary TransactionData::GetTransactionSummary() const
{
   return summary_;
}

void TransactionData::InvalidateTransactionData()
{
   usedUTXO_.clear();
   summary_ = TransactionSummary{};
   maxAmount_ = 0;

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
   if (!selectedInputs_) {
      return false;
   }
   uint64_t availableBalance = 0;

   std::vector<UTXO> transactions = decorateUTXOs();
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

   PaymentStruct payment = (!totalFee_ && !qFuzzyIsNull(feePerByte_))
      ? PaymentStruct(recipientsMap, 0, feePerByte_, 0)
      : PaymentStruct(recipientsMap, totalFee_, 0, 0);
   summary_.balanceToSpend = UiUtils::amountToBtc(payment.spendVal_);

   if (payment.spendVal_ <= availableBalance) {
      if (maxAmount) {
         const UtxoSelection selection = computeSizeAndFee(transactions, payment);
         summary_.txVirtSize = getVirtSize(selection);
         if (summary_.txVirtSize > kMaxTxStdWeight) {
            if (logger_) {
               logger_->error("Bad virtual size value {} - set to 0", summary_.txVirtSize);
            }
            summary_.txVirtSize = 0;
         }
         summary_.totalFee = availableBalance - payment.spendVal_;
         summary_.feePerByte =
            std::round((float)summary_.totalFee / (float)summary_.txVirtSize);
         summary_.hasChange = false;
         summary_.selectedBalance = UiUtils::amountToBtc(availableBalance);
      } else if (selectedInputs_->UseAutoSel()) {
         UtxoSelection selection;
         try {
            selection = coinSelection_->getUtxoSelectionForRecipients(payment
               , transactions);
         } catch (const std::runtime_error &err) {
            if (logger_) {
               logger_->error("UpdateTransactionData (auto-selection) - coinSelection exception: {}"
                  , err.what());
            }
            return false;
         } catch (...) {
            if (logger_) {
               logger_->error("UpdateTransactionData (auto-selection) - coinSelection exception");
            }
            return false;
         }

         usedUTXO_ = selection.utxoVec_;
         summary_.txVirtSize = getVirtSize(selection);
         summary_.totalFee = selection.fee_;
         summary_.feePerByte = selection.fee_byte_;
         summary_.hasChange = selection.hasChange_;
         summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);
      } else {
         UtxoSelection selection = computeSizeAndFee(transactions, payment);
         summary_.txVirtSize = getVirtSize(selection);
         if (summary_.txVirtSize > kMaxTxStdWeight) {
            if (logger_) {
               logger_->error("Bad virtual size value {} - set to 0", summary_.txVirtSize);
            }
            summary_.txVirtSize = 0;
         }
         summary_.totalFee = selection.fee_;
         summary_.feePerByte = selection.fee_byte_;
         summary_.hasChange = selection.hasChange_;
         summary_.selectedBalance = UiUtils::amountToBtc(selection.value_);

         /*         if (!selection.hasChange_) {  // sometimes selection calculation is too intelligent - prevent change address removal
                     summary_.totalFee = totalFee();
                     summary_.feePerByte = feePerByte();
                  }*/
      }
      summary_.usedTransactions = usedUTXO_.size();
   }

   summary_.outputsCount = recipients_.size();
   summary_.initialized = true;

   return true;
}

// Calculate the maximum fee for a given recipient.
double TransactionData::CalculateMaxAmount(const bs::Address &recipient, bool force) const
{
   if (!coinSelection_) {
      if (logger_) {
         logger_->error("[TransactionData::CalculateMaxAmount] wallet is missing");
      }
      return std::numeric_limits<double>::infinity();
   }
   if ((maxAmount_ > 0) && !force) {
      return maxAmount_;
   }

   maxAmount_ = 0;

   if ((feePerByte_ == 0) && totalFee_) {
      const double availableBalance = GetTransactionSummary().availableBalance - \
         GetTransactionSummary().balanceToSpend;
      double totalFee = (totalFee_ < minTotalFee_) ? minTotalFee_ / BTCNumericTypes::BalanceDivider
         : totalFee_ / BTCNumericTypes::BalanceDivider;
      if (availableBalance > totalFee) {
         maxAmount_ = availableBalance - totalFee;
      }
   }
   else {
      std::vector<UTXO> transactions = decorateUTXOs();

      if (transactions.size() == 0) {
         if (logger_) {
            logger_->debug("[TransactionData::CalculateMaxAmount] empty input list");
         }
         return 0;
      }

      std::map<unsigned int, std::shared_ptr<ScriptRecipient>> recipientsMap;
      unsigned int recipId = 0;
      for (const auto &recip : recipients_) {
         const auto recipPtr = recip.second->GetScriptRecipient();
         if (!recipPtr || !recipPtr->getValue()) {
            continue;
         }
         recipientsMap[recipId++] = recipPtr;
      }
      if (!recipient.isNull()) {
         const auto recipPtr = recipient.getRecipient(0.001);  // spontaneous output amount, shouldn't be 0
         if (recipPtr) {
            recipientsMap[recipId++] = recipPtr;
         }
      }
      if (recipientsMap.empty()) {
         return 0;
      }

      const PaymentStruct payment = (!totalFee_ && !qFuzzyIsNull(feePerByte_))
         ? PaymentStruct(recipientsMap, 0, feePerByte_, 0)
         : PaymentStruct(recipientsMap, totalFee_, feePerByte_, 0);

      // Accept the fee returned by Armory. The fee returned may be a few
      // satoshis higher than is strictly required by Core but that's okay.
      // If truly required, the fee can be tweaked later.
      try {
         double fee = coinSelection_->getFeeForMaxVal(payment.size_, feePerByte_
            , transactions) / BTCNumericTypes::BalanceDivider;
         if (fee < minTotalFee_ / BTCNumericTypes::BalanceDivider) {
            fee = minTotalFee_ / BTCNumericTypes::BalanceDivider;
         }

         const double availableBalance = GetTransactionSummary().availableBalance - \
            GetTransactionSummary().balanceToSpend;
         if (availableBalance >= fee) {
            maxAmount_ = availableBalance - fee;
         }
      } catch (const std::exception &e) {
         if (logger_) {
            logger_->error("[TransactionData::CalculateMaxAmount] failed to get fee for max val: {}", e.what());
         }
      }
   }
   return maxAmount_;
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

// A function equivalent to CoinSelectionInstance::decorateUTXOs() in Armory. We
// need it for proper initialization of the UTXO structs when computing TX sizes
// and fees.
// IN:  None
// OUT: None
// RET: A vector of fully initialized UTXO objects, one for each selected (and
//      non-filtered) input.
std::vector<UTXO> TransactionData::decorateUTXOs() const
{
   if (!selectedInputs_) {
      return {};
   }

   auto inputUTXOs = selectedInputs_->GetSelectedTransactions();

   if (utxoAdapter_ && reservedUTXO_.empty()) {
      utxoAdapter_->filter(selectedInputs_->GetWallet()->walletId(), inputUTXOs);
   }

#if 0 // since we don't have address entries and public keys now, we need to re-think this code
   for (auto& utxo : inputUTXOs) {
      // Prep the UTXOs for calculation.
      auto aefa = wallet_->getAddressEntryForAddr(utxo.getRecipientScrAddr());
      utxo.txinRedeemSizeBytes_ = 0;
      utxo.isInputSW_ = false;

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
#endif //0

   bs::Address::decorateUTXOs(inputUTXOs);
   return inputUTXOs;
}

// Frontend for UtxoSelection::computeSizeAndFee(). Necessary due to some
// nuances in how it's invoked.
// IN:  UTXO vector used to initialize UtxoSelection. (std::vector<UTXO>)
// OUT: None
// RET: A fully initialized UtxoSelection object, with size and fee data.
UtxoSelection TransactionData::computeSizeAndFee(const std::vector<UTXO>& inUTXOs
   , const PaymentStruct& inPS) const
{
   // When creating UtxoSelection object, initialize it with a copy of the
   // UTXO vector. Armory will "move" the data behind-the-scenes, and we
   // still need the data.
   usedUTXO_ = inUTXOs;
   auto usedUTXOCopy{ usedUTXO_ };
   UtxoSelection selection{ usedUTXOCopy };

   try {
      selection.computeSizeAndFee(inPS);
   }
   catch (const std::runtime_error &err) {
      if (logger_) {
         logger_->error("UpdateTransactionData - UtxoSelection exception: {}"
            , err.what());
      }
   }
   catch (...) {
      if (logger_) {
         logger_->error("UpdateTransactionData - UtxoSelection exception");
      }
   }

   return selection;
}

// A temporary private function that calculates the virtual size of an incoming
// UtxoSelection object. This needs to be removed when a particular PR
// (https://github.com/goatpig/BitcoinArmory/pull/538) is accepted upstream.
// Note that this function assumes SegWit will be used. It's fine for our
// purposes but it's a bad assumption in general.
size_t TransactionData::getVirtSize(const UtxoSelection& inUTXOSel) const
{
   size_t nonWitSize = inUTXOSel.size_ - inUTXOSel.witnessSize_;
   return std::ceil(static_cast<float>(3 * nonWitSize + inUTXOSel.size_) / 4.0f);
}

void TransactionData::setFeePerByte(float feePerByte)
{
   // Our fees estimation is not 100% accurate (we can't know how much witness size will have,
   // because we don't know signature(s) size in advance, it could be 73, 72, and 71).
   // As the result we might hit "min fee relay not meet" error (when actual fees is lower then 1 sat/bytes).
   // Let's add a workaround for this: don't allow feePerByte be less than 1.01f (that's just empirical estimate)
   const float minRelayFeeFixed = 1.01f;

   if (feePerByte >= 1.0f && feePerByte < minRelayFeeFixed) {
      feePerByte_ = minRelayFeeFixed;
   } else {
      feePerByte_ = feePerByte;
   }
   totalFee_ = 0;
   InvalidateTransactionData();
}

void TransactionData::setTotalFee(uint64_t fee, bool overrideFeePerByte)
{
   totalFee_ = fee;
   if (overrideFeePerByte) {
      feePerByte_ = 0;
   }
   InvalidateTransactionData();
}

float TransactionData::feePerByte() const
{
   if (!qFuzzyIsNull(feePerByte_) && (feePerByte_ > 0)) {
      return feePerByte_;
   }

   if (summary_.initialized) {
	   if (summary_.txVirtSize) {
		   return totalFee_ / summary_.txVirtSize;
	   }
   }

   return 0;
}

uint64_t TransactionData::totalFee() const
{
   if (totalFee_) {
      return totalFee_;
   }
   if (summary_.totalFee) {
      return summary_.totalFee;
   }
   if (summary_.txVirtSize) {
      return feePerByte_ * summary_.txVirtSize;
   }
   return 0;
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
      utxoAdapter_->reserve(wallet_->walletId(), reserveId, reservedUTXO_);
   }
}

void TransactionData::ReloadSelection(const std::vector<UTXO> &utxos)
{
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
   return wallet_ && selectedInputs_
      && summary_.usedTransactions != 0
      && (!qFuzzyIsNull(feePerByte_) || totalFee_ != 0 || summary_.totalFee != 0)
      && RecipientsReady();
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

std::vector<unsigned int> TransactionData::allRecipientIds() const
{
   std::vector<unsigned int> result;
   result.reserve(recipients_.size());
   for (const auto &recip : recipients_) {
      result.push_back(recip.first);
   }
   return result;
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

BTCNumericTypes::balance_type TransactionData::GetTotalRecipientsAmount() const
{
   BTCNumericTypes::balance_type result = 0;
   for (const auto &recip : recipients_) {
      result += recip.second->GetAmount();
   }
   return result;
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


void TransactionData::GetFallbackRecvAddress(std::function<void(const bs::Address&)> cb)
{
   if (!fallbackRecvAddress_.isNull() || !wallet_) {
      cb(fallbackRecvAddress_);
      return;
   }

   const auto &cbWrap = [this, cb = std::move(cb), handle = validityFlag_.handle()](const bs::Address &addr) {
      if (!handle.isValid()) {
         return;
      }
      fallbackRecvAddress_ = addr;
      cb(fallbackRecvAddress_);
   };
   wallet_->getNewExtAddress(cbWrap);
}

const bs::Address &TransactionData::GetFallbackRecvAddressIfSet() const
{
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

bs::core::wallet::TXSignRequest TransactionData::createUnsignedTransaction(bool isRBF, const bs::Address &changeAddress)
{
   if (!wallet_) {
      return {};
   }
   unsignedTxReq_ = wallet_->createTXRequest(inputs(), GetRecipientList(), summary_.totalFee, isRBF, changeAddress);
   if (!unsignedTxReq_.isValid()) {
      throw std::runtime_error("missing unsigned TX");
   }
   reservedUTXO_ = unsignedTxReq_.inputs;
   return unsignedTxReq_;
}

bs::core::wallet::TXSignRequest TransactionData::getSignTxRequest() const
{
   if (!unsignedTxReq_.isValid()) {
      throw std::runtime_error("missing unsigned TX");
   }
   return unsignedTxReq_;
}

bs::core::wallet::TXSignRequest TransactionData::createTXRequest(bool isRBF
   , const bs::Address &changeAddr, const uint64_t& origFee) const
{
   if (!wallet_ && !group_) {
      return {};
   }
   auto txReq = wallet_->createTXRequest(inputs(), GetRecipientList()
      , summary_.totalFee, isRBF, changeAddr, origFee);
   if (group_) {
      txReq.walletIds.clear();
      std::set<std::string> walletIds;
      const auto &leaves = group_->getAllLeaves();
      for (const auto &input : inputs()) {
         std::string inputLeafId;
         for (const auto &leaf : leaves) {
            if (leaf->containsAddress(bs::Address::fromUTXO(input))) {
               inputLeafId = leaf->walletId();
               break;
            }
         }
         if (inputLeafId.empty()) {
            throw std::runtime_error("orphaned input " + input.getTxHash().toHexStr(true)
               + " without wallet");
         }
         walletIds.insert(inputLeafId);
      }
      txReq.walletIds.insert(txReq.walletIds.end(), walletIds.cbegin(), walletIds.cend());
   }
   return txReq;
}

bs::core::wallet::TXSignRequest TransactionData::createPartialTXRequest(uint64_t spendVal, float feePerByte
   , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients, const BinaryData &prevData
   , const std::vector<UTXO> &utxos)
{
   if (!wallet_) {
      return {};
   }

   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };    //TODO: refactor this
   wallet_->getNewChangeAddress(cbAddr);
   auto txReq = wallet_->createPartialTXRequest(spendVal, utxos.empty() ? inputs() : utxos
      , futAddr.get(), feePerByte, recipients, prevData);
   txReq.populateUTXOs = true;
   return txReq;
}

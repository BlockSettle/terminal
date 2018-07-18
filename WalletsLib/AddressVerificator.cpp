#include "AddressVerificator.h"

#include "ArmoryConnection.h"
#include "BinaryData.h"
#include "FastLock.h"
#include "BlockDataManagerConfig.h"

#include <QDateTime>

#include <cassert>
#include <spdlog/spdlog.h>

enum class BSValidationAddressState
{
   NotRegistered,
   InProgress,
   Active,
   Revoked,
   Error
};

static constexpr int MaxAadressValidationErrorCount = 3;
static constexpr int MaxBSAddressValidationErrorCount = 3;

struct AddressVerificationData
{
   std::shared_ptr<AuthAddress>  address;
   AddressVerificationState      currentState;
   int                           addressValidationErrorCount;
   bs::Address                   bsFundingAddress;
   BSValidationAddressState      bsAddressValidationState;
   int                           bsAddressValidationErrorCount;

   BinaryData                    initialTxHash;
   BinaryData                    verificationTxHash;

   unsigned int   nbTransactions;
   bool           getInputFromBS;
   bool           isVerifiedByUser;
   int64_t        value = 0;
   std::vector<ClientClasses::LedgerEntry>   entries;
   std::vector<ClientClasses::LedgerEntry>   txOutEntries;
   std::map<BinaryData, Tx>   txs;
   std::set<BinaryData>       txHashSet;
};

AddressVerificator::AddressVerificator(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ArmoryConnection> &armory
   , const std::string& walletId, verification_callback callback)
   : QObject(nullptr)
   , logger_(logger)
   , armory_(armory)
   , walletId_(walletId)
   , userCallback_(callback)
   , stopExecution_(false)
{
   connect(armory_.get(), &ArmoryConnection::refresh, this, &AddressVerificator::OnRefresh);

   startCommandQueue();
}

AddressVerificator::~AddressVerificator() noexcept
{
   stopCommandQueue();
}

bool AddressVerificator::startCommandQueue()
{
   commandQueueThread_ = std::thread(&AddressVerificator::commandQueueThreadFunction, this);
   return true;
}

bool AddressVerificator::stopCommandQueue()
{
   {
      std::unique_lock<std::mutex> locker(dataMutex_);
      stopExecution_ = true;
      dataAvailable_.notify_all();
   }

   if (commandQueueThread_.joinable()) {
      commandQueueThread_.join();
   }
   return true;
}

void AddressVerificator::commandQueueThreadFunction()
{
   forever
   {
      ExecutionCommand nextCommand;

      {
         std::unique_lock<std::mutex>  locker(dataMutex_);
         dataAvailable_.wait(locker,
            [this] () {
               return stopExecution_.load() || !commandsQueue_.empty();
            }
         );

         if (stopExecution_) {
            break;
         }

         assert(!commandsQueue_.empty());
         nextCommand = commandsQueue_.front();
         commandsQueue_.pop();
      }

      // process command
      nextCommand();
   }
}

bool AddressVerificator::SetBSAddressList(const std::unordered_set<std::string>& addressList)
{
   for (const auto &addr : addressList) {
      bs::Address address(addr);
      bsAddressList_.emplace(address.prefixed());
   }
   return true;
}

bool AddressVerificator::StartAddressVerification(const std::shared_ptr<AuthAddress>& address)
{
   auto addressCopy = std::make_shared<AuthAddress>(*address);

   if (AddressWasRegistered(addressCopy)) {
      logger_->debug("[AddressVerificator::StartAddressVerification] adding verification command to queue: {}"
         , address->GetChainedAddress().display<std::string>());
      return AddCommandToQueue(CreateAddressValidationCommand(addressCopy));
   }

   return RegisterUserAddress(addressCopy);
}

bool AddressVerificator::AddCommandToQueue(ExecutionCommand&& command)
{
   std::unique_lock<std::mutex> locker(dataMutex_);
   commandsQueue_.emplace(std::move(command));
   dataAvailable_.notify_all();

   return true;
}

void AddressVerificator::AddCommandToWaitingUpdateQueue(ExecutionCommand&& command)
{
   FastLock locker(waitingForUpdateQueueFlag_);
   waitingForUpdateQueue_.emplace(std::move(command));
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateAddressValidationCommand(const std::shared_ptr<AuthAddress>& address)
{
   auto state = std::make_shared<AddressVerificationData>();

   state->address = address;
   state->currentState = AddressVerificationState::InProgress;
   state->addressValidationErrorCount = 0;
   state->bsAddressValidationState = BSValidationAddressState::InProgress;
   state->bsAddressValidationErrorCount = 0;

   return CreateAddressValidationCommand(state);
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state)
{
   return [this, state]() {
      this->ValidateAddress(state);
   };
}

void AddressVerificator::doValidateAddress(const std::shared_ptr<AddressVerificationData>& state)
{
   bool isVerified = false;
   for (const auto &entry : state->entries) {
      if (!state->getInputFromBS) {    // if we did not get initial transaction from BS we ignore all other
         if (IsInitialBsTransaction(entry, state, isVerified)) {
            state->address->SetInitialTransactionTxHash(entry.getTxHash());
            state->currentState = AddressVerificationState::PendingVerification;
            state->getInputFromBS = true;
         }
      }
      else {
         if (!state->isVerifiedByUser && IsVerificationTransaction(entry, state, isVerified)) {
            if (!isVerified) {
               state->currentState = AddressVerificationState::VerificationSubmitted;
               break;
            }
            if (HasRevokeOutputs(entry, state)) {
               state->currentState = AddressVerificationState::Revoked;
               break;
            }
            else {
               state->address->SetVerificationChangeTxHash(entry.getTxHash());
               state->currentState = AddressVerificationState::Verified;
               state->isVerifiedByUser = true;
               continue;
            }
         }
         if (IsRevokeTransaction(entry, state)) {
            state->currentState = AddressVerificationState::Revoked;
            break;
         }
      }
   }
   if (!state->getInputFromBS) {
      state->currentState = state->nbTransactions ? AddressVerificationState::Revoked : AddressVerificationState::NotSubmitted;
   }
   else if (state->value <= 0) {
      state->currentState = AddressVerificationState::Revoked;
   }

   addressRetries_.erase(state->address->GetChainedAddress().prefixed());
   ReturnValidationResult(state);
}

void AddressVerificator::ValidateAddress(const std::shared_ptr<AddressVerificationData>& state)
{   // if we are here, that means that address was added, and now it is time to try to validate it
   if (bsAddressList_.empty()) {
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   state->getInputFromBS = false;
   state->isVerifiedByUser = false;
   state->value = 0;
   state->entries.clear();

   if (!armory_ || (armory_->state() != ArmoryConnection::State::Ready)) {
      logger_->error("[AddressVerificator::ValidateAddress] invalid Armory state {}", (int)armory_->state());
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   const auto &cbCollectTXs = [this, state](std::vector<Tx> txs) {
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         state->txHashSet.erase(txHash);
         state->txs[txHash] = tx;
         if (state->txHashSet.empty()) {
            doValidateAddress(state);
         }
      }
   };
   const auto &cbTxOutTX = [this, state](Tx tx) {
      state->txHashSet.erase(tx.getThisHash());
      state->txs[tx.getThisHash()] = tx;
      if (state->txHashSet.empty()) {
         doValidateAddress(state);
      }
   };
   const auto &cbLedgerTxOut = [this, state, cbTxOutTX](std::vector<ClientClasses::LedgerEntry> entries) {
      state->txOutEntries = entries;
      for (const auto &entry : entries) {
         const auto &itTX = state->txs.find(entry.getTxHash());
         if (itTX == state->txs.end()) {
            state->txHashSet.insert(entry.getTxHash());
            armory_->getTxByHash(entry.getTxHash(), cbTxOutTX);
         }
      }
   };
   const auto &cbLedgerDelegateTxOut = [this, state, cbLedgerTxOut](AsyncClient::LedgerDelegate delegate) {
      delegate.getHistoryPage(0, cbLedgerTxOut);
   };
   const auto &cbCollectTX = [this, state, cbCollectTXs, cbLedgerDelegateTxOut](Tx tx) {
      state->txs[tx.getThisHash()] = tx;
      for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
         TxIn in = tx.getTxInCopy(i);
         OutPoint op = in.getOutPoint();
         state->txHashSet.insert(op.getTxHash());
      }
      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         auto txOut = tx.getTxOutCopy((int)i);
         const auto addr = bs::Address::fromTxOutScript(txOut.getScript());
         if (addr.prefixed() == state->address->GetChainedAddress().prefixed()) {
            continue;
         }
         armory_->getLedgerDelegateForAddress(walletId_, addr, cbLedgerDelegateTxOut);
      }
      armory_->getTXsByHash(state->txHashSet, cbCollectTXs);
   };
   const auto &cbLedger = [this, state, cbCollectTX](std::vector<ClientClasses::LedgerEntry> entries) {
      state->nbTransactions += entries.size();
      state->entries = entries;
      for (const auto &entry : entries) {
         state->value += entry.getValue();
         const auto &itTX = state->txs.find(entry.getTxHash());
         if (itTX == state->txs.end()) {
            armory_->getTxByHash(entry.getTxHash(), cbCollectTX);
         }
         else {
            cbCollectTX(itTX->second);
         }
      }
   };
   const auto &cbLedgerDelegate = [this, state, cbLedger](AsyncClient::LedgerDelegate delegate) {
      state->nbTransactions = 0;
      delegate.getHistoryPage(0, cbLedger);  //? should we use more than 0 pageId?
   };
   if (!armory_->getLedgerDelegateForAddress(walletId_, state->address->GetChainedAddress(), cbLedgerDelegate)) {
      const auto &prefixedAddress = state->address->GetChainedAddress().prefixed();
      if ((state->currentState == AddressVerificationState::InProgress) && (addressRetries_[prefixedAddress] < MaxAadressValidationErrorCount)) {
         logger_->debug("[AddressVerificator::ValidateAddress] Failed to get ledger for {} - retrying command", walletId_);
         AddCommandToWaitingUpdateQueue(CreateAddressValidationCommand(state));
         addressRetries_[prefixedAddress]++;    // reschedule validation since error occured
      }
      else {
         logger_->error("[AddressVerificator::ValidateAddress] Failed to validate address {}", state->address->GetChainedAddress().display<std::string>());
         state->currentState = AddressVerificationState::VerificationFailed;
         ReturnValidationResult(state);
         addressRetries_.erase(prefixedAddress);
      }
      return;
   }
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateBSAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state)
{
   return [this, state]() {
      this->CheckBSAddressState(state);
   };
}

void AddressVerificator::CheckBSAddressState(const std::shared_ptr<AddressVerificationData> &state)
{     // check that outgoing transactions are only to auth address
   const auto &cbCheckState = [this, state] {
      for (auto &entry : state->entries) {
         if (IsBSRevokeTranscation(entry, state)) {
            state->bsAddressValidationState = BSValidationAddressState::Revoked;
            state->currentState = AddressVerificationState::RevokedByBS;
            break;
         }
      }
      ReturnValidationResult(state);
   };
   const auto &cbCollectTXs = [this, state, cbCheckState](std::vector<Tx> txs) {
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         state->txHashSet.erase(txHash);
         state->txs[txHash] = tx;
         if (state->txHashSet.empty()) {
            cbCheckState();
         }
      }
   };
   const auto &cbLedger = [this, state, cbCollectTXs, cbCheckState](std::vector<ClientClasses::LedgerEntry> entries) {
      state->nbTransactions += entries.size();
      state->entries = entries;
      for (const auto &entry : entries) {
         state->value += entry.getValue();
         const auto &itTX = state->txs.find(entry.getTxHash());
         if (itTX == state->txs.end()) {
            state->txHashSet.insert(entry.getTxHash());
         }
      }
      if (!state->txHashSet.empty()) {
         armory_->getTXsByHash(state->txHashSet, cbCollectTXs);
      }
      else {
         cbCheckState();
      }
   };
   const auto &cbLedgerDelegate = [this, state, cbLedger](AsyncClient::LedgerDelegate delegate) {
      state->entries.clear();
      delegate.getHistoryPage(0, cbLedger);  //? should we use more than 0 pageId?
   };
   if (!armory_->getLedgerDelegateForAddress(walletId_, state->address->GetChainedAddress(), cbLedgerDelegate)) {
      logger_->error("[AddressVerificator::CheckBSAddressState] Could not validate address. Looks like armory is offline.");
      if (state->bsAddressValidationErrorCount >= MaxBSAddressValidationErrorCount) {
         logger_->error("[AddressVerificator::CheckBSAddressState] marking address as failed to validate {}", state->bsFundingAddress.display<std::string>());
         state->currentState = AddressVerificationState::VerificationFailed;
         ReturnValidationResult(state);
      } else {
         // reschedule validation since error occured
         AddCommandToQueue(CreateBSAddressValidationCommand(state));
      }
      return;
   }
}

void AddressVerificator::ReturnValidationResult(const std::shared_ptr<AddressVerificationData>& state)
{
   userCallback_(state->address, state->currentState);
}

bool AddressVerificator::IsInitialBsTransaction(const ClientClasses::LedgerEntry &entry
   , const std::shared_ptr<AddressVerificationData>& state, bool &isVerified)
{
   int64_t entryValue = (entry.getValue() < 0 ? -entry.getValue() : entry.getValue());
   if (entryValue <= (GetAuthAmount() * 2)) {
      return false;
   }

   bool sentToUs = false;
   bool sentByBS = false;

   const auto &tx = state->txs[entry.getTxHash()];
   if (!tx.isInitialized()) {
      return false;
   }
   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();

      Tx prevTx = state->txs[op.getTxHash()];
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
         bs::Address address(prevOut.getScrAddressStr());

         if (bsAddressList_.find(address.prefixed()) != bsAddressList_.end()) {
            state->bsFundingAddress = address;
            state->address->SetBSFundingAddress(address);
            sentByBS = true;
            break;
         }
      }
   }

   if (!sentByBS) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy(i);
      bs::Address address(out.getScrAddressStr(), state->address->GetChainedAddress().getType());
      if (address.prefixed() == state->address->GetChainedAddress().prefixed()) {
         sentToUs = true;
         break;
      }
   }

   if (!sentToUs) {
      return false;
   }

   isVerified = armory_->isTransactionConfirmed(entry);

   state->initialTxHash = entry.getTxHash();
   return true;
}

bool AddressVerificator::IsVerificationTransaction(const ClientClasses::LedgerEntry &entry
   , const std::shared_ptr<AddressVerificationData>& state, bool &isVerified)
{
   if (entry.getValue() <= (GetAuthAmount() * 2)) {
      return false;
   }

   assert(state->initialTxHash.getSize() != 0);

   bool sentByUs = false;
   bool sentToBS = false;

   const auto &tx = state->txs[entry.getTxHash()];
   if (!tx.isInitialized()) {
      return false;
   }
   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();

      if (state->initialTxHash != op.getTxHash()) {
         continue;
      }

      Tx prevTx = state->txs[op.getTxHash()];
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
         bs::Address address(prevOut.getScrAddressStr(), state->address->GetChainedAddress().getType());
         if (address.prefixed() == state->address->GetChainedAddress().prefixed()) {
            sentByUs = true;
            break;
         }
      }
   }

   if (!sentByUs) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy(i);
      const bs::Address address(out.getScrAddressStr());

      // it should be sent directly to funding bs address
      if (state->bsFundingAddress == address) {
         sentToBS = true;
         break;
      }
   }

   if (!sentToBS) {
      return false;
   }

   isVerified = armory_->isTransactionVerified(entry);
   state->verificationTxHash = entry.getTxHash();

   return true;
}

bool AddressVerificator::HasRevokeOutputs(const ClientClasses::LedgerEntry &entry
   , const std::shared_ptr<AddressVerificationData>& state)
{
   for (const auto &led : state->txOutEntries) {
      if ((led.getBlockNum() < entry.getBlockNum()) || (led.getTxHash() == entry.getTxHash())) {
         continue;
      }
      const auto succTx = state->txs[led.getTxHash()];
      if (!succTx.isInitialized()) {
         continue;   //? return true
      }
      for (size_t j = 0; j < succTx.getNumTxIn(); j++) {
         TxIn txIn = succTx.getTxInCopy((int)j);
         if (!txIn.isInitialized()) {
            return true;
         }
         OutPoint op = txIn.getOutPoint();
         if (op.getTxHash() == entry.getTxHash()) {
            return true;
         }
      }
   }
   return false;
}

//returns true if
// - coins from initial transaction sent in any amount to any address
// - anything is sent from verification transaction
bool AddressVerificator::IsRevokeTransaction(const ClientClasses::LedgerEntry &entry
   , const std::shared_ptr<AddressVerificationData>& state)
{
   const auto &tx = state->txs[entry.getTxHash()];
   if (!tx.isInitialized()) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();

      // any amount from initial transaction
      // this should not be verification transaction in this check
      if (state->initialTxHash == op.getTxHash()) {
         return true;
      }

      // any amount is sent from change of verification transaction
      if (state->verificationTxHash == op.getTxHash()) {
         return true;
      }
   }
   return false;
}

bool AddressVerificator::IsBSRevokeTranscation(const ClientClasses::LedgerEntry &entry, const std::shared_ptr<AddressVerificationData>& state)
{
   auto tx = state->txs[entry.getTxHash()];
   if (!tx.isInitialized()) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
      TxOut out = tx.getTxOutCopy(i);
      bs::Address address(out.getScrAddressStr(), state->address->GetChainedAddress().getType());
      if (address.prefixed() == state->address->GetChainedAddress().prefixed()) {
         return true;
      }
   }
   return false;
}

bool AddressVerificator::HaveBSAddressList() const
{
   return !bsAddressList_.empty();
}

bool AddressVerificator::AddressWasRegistered(const std::shared_ptr<AuthAddress>& address) const
{
   FastLock locker(authAddressSetFlag_);
   return authAddressSet_.find(address->GetChainedAddress().prefixed()) != authAddressSet_.end();
}

bool AddressVerificator::RegisterUserAddress(const std::shared_ptr<AuthAddress>& address)
{
   {
      FastLock locker(pendingRegAddressFlag_);
      pendingRegAddresses_.insert(address->GetChainedAddress().prefixed());
   }

   logger_->debug("[AddressVerificator::RegisterUserAddress] add address to update Q: {}"
      , address->GetChainedAddress().display<std::string>());

   AddCommandToWaitingUpdateQueue(CreateAddressValidationCommand(address));
   return true;
}

void AddressVerificator::RegisterBSAuthAddresses()
{
   FastLock locker(pendingRegAddressFlag_);
   pendingRegAddresses_.insert(bsAddressList_.begin(), bsAddressList_.end());
}

void AddressVerificator::RegisterAddresses()
{
   if (pendingRegAddresses_.empty()) {
      return;
   }

   pendingRegAddresses_.insert(authAddressSet_.begin(), authAddressSet_.end());
   if (pendingRegAddresses_ == authAddressSet_) {
      return;
   }

   std::vector<BinaryData> addresses;
   addresses.insert(addresses.end(), pendingRegAddresses_.begin(), pendingRegAddresses_.end());

   {
      FastLock locker(authAddressSetFlag_);
      authAddressSet_.swap(pendingRegAddresses_);
   }

   if (armory_ && (armory_->state() == ArmoryConnection::State::Ready)) {
      pendingRegAddresses_.clear();
      armory_->registerWallet(internalWallet_, walletId_, addresses, false);
      logger_->debug("[AddressVerificator::RegisterAddresses] registered {} addresses", addresses.size());
   }
   else {
      logger_->error("[AddressVerificator::RegisterAddresses] failed to get BDM");
   }
}

void AddressVerificator::OnRefresh()
{
   logger_->debug("[AddressVerificator::OnRefresh] get refresh command");

   ExecutionCommand command;
   {
      FastLock locker(waitingForUpdateQueueFlag_);
      if (waitingForUpdateQueue_.empty()) {
         logger_->debug("[AddressVerificator::OnRefresh] no pending commands for update");
         return;
      }

      // move all pending commands to processing queue
      while (!waitingForUpdateQueue_.empty()) {
         command = waitingForUpdateQueue_.front();
         waitingForUpdateQueue_.pop();
         AddCommandToQueue(std::move(command));
      }
   }
}

void AddressVerificator::GetVerificationInputs(std::function<void(std::vector<UTXO>)> cb) const
{
   auto result = new std::vector<UTXO>;
   const auto &cbInternal = [this, cb, result](std::vector<UTXO> utxos) {
      *result = utxos;
      const auto &cbZC = [this, cb, result](std::vector<UTXO> zcs) {
         result->insert(result->begin(), zcs.begin(), zcs.end());
         cb(*result);
         delete result;
      };
      internalWallet_->getSpendableZCList(cbZC);
   };
   internalWallet_->getSpendableZCList(cbInternal);
}

void AddressVerificator::GetRevokeInputs(std::function<void(std::vector<UTXO>)> cb) const
{
   const auto &cbInternal = [this, cb](std::vector<UTXO> utxos) {
      std::vector<UTXO> result;
      for (const auto &utxo : utxos) {
         if ((utxo.getValue() == GetAuthAmount()) && (utxo.getTxOutIndex() == 1)) {
            result.emplace_back(utxo);
         }
      }
      cb(result);
   };
   internalWallet_->getSpendableTxOutListForValue(UINT64_MAX, cbInternal);
}

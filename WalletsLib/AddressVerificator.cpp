#include "AddressVerificator.h"

#include "LedgerEntryData.h"

#include "PyBlockDataManager.h"
#include "BinaryData.h"
#include "FastLock.h"
#include "BlockDataManagerConfig.h"
#include "SafeBtcWallet.h"
#include "SafeLedgerDelegate.h"

#include <QDateTime>

#include <cassert>
#include <spdlog/spdlog.h>

class AddressVerificatorListener : public PyBlockDataListener
{
public:
   AddressVerificatorListener(AddressVerificator *verificator)
    : verificator_(verificator), refreshEnabled_(true)
   {}
   ~AddressVerificatorListener() noexcept override = default;

   void OnRefresh() override {
      // if (refreshEnabled_) {
         verificator_->OnRefresh();
      // }
   }

   void setRefreshEnabled(bool enabled = true) { refreshEnabled_ = enabled; }

private:
   AddressVerificator *verificator_;
   std::atomic_bool  refreshEnabled_;
};

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

struct AddressVarificationData
{
   std::shared_ptr<AuthAddress>  address;
   AddressVerificationState      currentState;
   int                           addressValidationErrorCount;
   bs::Address                   bsFundingAddress;
   BSValidationAddressState      bsAddressValidationState;
   int                           bsAddressValidationErrorCount;

   BinaryData                    initialTxHash;
   BinaryData                    verificationTxHash;
};

AddressVerificator::AddressVerificator(const std::shared_ptr<spdlog::logger>& logger, const std::string& walletId, verification_callback callback)
   : logger_(logger)
   , walletId_(walletId)
   , userCallback_(callback)
   , stopExecution_(false)
{
   registerInternalWallet();
   startCommandQueue();

   listener_ = std::make_shared<AddressVerificatorListener>(this);
   PyBlockDataManager::instance()->addListener(listener_.get());
}

AddressVerificator::~AddressVerificator() noexcept
{
   stopCommandQueue();
   const auto &bdm = PyBlockDataManager::instance();
   if (bdm) {
      bdm->removeListener(listener_.get());
   }
}

bool AddressVerificator::registerInternalWallet()
{   // register wallet with empty address list
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm || (bdm->GetState() != PyBlockDataManagerState::Ready)) {
      return false;
   }

   return true;
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
   bsAddressList_.reserve(addressList.size());
   for (const auto &addr : addressList) {
      bs::Address address(addr);
      bsAddressList_.insert(address.prefixed());
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
   auto state = std::make_shared<AddressVarificationData>();

   state->address = address;
   state->currentState = AddressVerificationState::InProgress;
   state->addressValidationErrorCount = 0;
   state->bsAddressValidationState = BSValidationAddressState::InProgress;
   state->bsAddressValidationErrorCount = 0;

   return CreateAddressValidationCommand(state);
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateAddressValidationCommand(const std::shared_ptr<AddressVarificationData>& state)
{
   return [this, state]() {
      this->ValidateAddress(state);
   };
}

void AddressVerificator::ValidateAddress(const std::shared_ptr<AddressVarificationData>& state)
{   // if we are here, that means that address was added, and now it is time to try to validate it
   if (bsAddressList_.empty()) {
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   bool getInputFromBS = false;
   bool isVerifiedByUser = false;
   int64_t value = 0;

   const auto prefixedAddress = state->address->GetChainedAddress().prefixed();
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      logger_->error("[AddressVerificator::ValidateAddress] failed to get BDM");
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }
   auto ledgerDelegate = bdm->getLedgerDelegateForScrAddr(walletId_, prefixedAddress);
   if (!ledgerDelegate) {
      if ((state->currentState == AddressVerificationState::InProgress) && (addressRetries_[prefixedAddress] < 3)) {
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

   int currentPageId = 0;
   unsigned int nbTransactions = 0;

   while (state->currentState == AddressVerificationState::InProgress) {
      const auto nextPage = ledgerDelegate->getHistoryPage(currentPageId);
      if (nextPage.empty()) {
         break;
      }
      nbTransactions += nextPage.size();

      for (auto &entry : nextPage) {
         bool isVerified = false;
         value += entry.getValue();

         if (!getInputFromBS) {    // if we did not get initial transaction from BS we ignore all other
            if (IsInitialBsTransaction(entry, state, isVerified)) {
               state->address->SetInitialTransactionTxHash(entry.getTxHash());
               state->currentState = AddressVerificationState::PendingVerification;
               getInputFromBS = true;
            }
         }
         else {
            if (!isVerifiedByUser && IsVerificationTransaction(entry, state, isVerified)) {
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
                  isVerifiedByUser = true;
                  continue;
               }
            }
            if (IsRevokeTransaction(entry, state)) {
               state->currentState = AddressVerificationState::Revoked;
               break;
            }
         }
      }
      currentPageId += 1;
   }

   if (!getInputFromBS) {
      state->currentState = nbTransactions ? AddressVerificationState::Revoked : AddressVerificationState::NotSubmitted;
   }
   else if (value <= 0) {
      state->currentState = AddressVerificationState::Revoked;
   }

   addressRetries_.erase(prefixedAddress);
   ReturnValidationResult(state);
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateBSAddressValidationCommand(const std::shared_ptr<AddressVarificationData>& state)
{
   return [this, state]() {
      this->CheckBSAddressState(state);
   };
}

void AddressVerificator::CheckBSAddressState(const std::shared_ptr<AddressVarificationData>& state)
{     // check that outgoing transactions are only to auth address
   const auto prefixedAddress = state->address->GetChainedAddress().prefixed();

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      logger_->error("[AddressVerificator::CheckBSAddressState] could not get proper BDM - looks like armory is offline");
      return;
   }
   auto ledgerDelegate = bdm->getLedgerDelegateForScrAddr(walletId_, prefixedAddress);
   if (ledgerDelegate == nullptr) {
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

   int currentPageId = 0;

   while (state->currentState == AddressVerificationState::InProgress) {
      auto nextPage = ledgerDelegate->getHistoryPage(currentPageId);
      if (nextPage.empty() ) {
         break;
      }
      for (auto &entry : nextPage) {
         if (IsBSRevokeTranscation(entry, state)) {
            state->bsAddressValidationState = BSValidationAddressState::Revoked;
            state->currentState = AddressVerificationState::RevokedByBS;
            break;
         }
      }
      currentPageId += 1;
   }

   ReturnValidationResult(state);
}

void AddressVerificator::ReturnValidationResult(const std::shared_ptr<AddressVarificationData>& state)
{
   // XXX
   userCallback_(state->address, state->currentState);
}

bool AddressVerificator::IsInitialBsTransaction(const LedgerEntryData& entry
   , const std::shared_ptr<AddressVarificationData>& state, bool &isVerified)
{
   int64_t entryValue = (entry.getValue() < 0 ? -entry.getValue() : entry.getValue());
   if (entryValue <= (GetAuthAmount() * 2)) {
      return false;
   }

   bool sentToUs = false;
   bool sentByBS = false;

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   auto tx = bdm->getTxByHash(entry.getTxHash());
   if (!tx.isInitialized()) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();

      const auto &bdm = PyBlockDataManager::instance();
      if (!bdm) {
         return false;
      }
      Tx prevTx = bdm->getTxByHash(op.getTxHash());
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

   isVerified = bdm ? bdm->IsTransactionConfirmed(entry) : false;

   state->initialTxHash = entry.getTxHash();
   return true;
}

bool AddressVerificator::IsVerificationTransaction(const LedgerEntryData& entry
   , const std::shared_ptr<AddressVarificationData>& state, bool &isVerified)
{
   if (entry.getValue() <= (GetAuthAmount() * 2)) {
      return false;
   }

   assert(state->initialTxHash.getSize() != 0);

   bool sentByUs = false;
   bool sentToBS = false;

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   auto tx = bdm->getTxByHash(entry.getTxHash());
   if (!tx.isInitialized()) {
      return false;
   }

   for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();

      if (state->initialTxHash != op.getTxHash()) {
         continue;
      }

      const auto &bdm = PyBlockDataManager::instance();
      if (!bdm) {
         break;
      }
      Tx prevTx = bdm->getTxByHash(op.getTxHash());
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

   isVerified = bdm ? bdm->IsTransactionVerified(entry) : false;
   state->verificationTxHash = entry.getTxHash();

   return true;
}

bool AddressVerificator::HasRevokeOutputs(const LedgerEntryData& entry
   , const std::shared_ptr<AddressVarificationData>& state)
{
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   const auto tx = bdm->getTxByHash(entry.getTxHash());
   for (size_t i = 0; i < tx.getNumTxOut(); i++) {
      auto txOut = tx.getTxOutCopy((int)i);
      const auto addr = bs::Address::fromTxOutScript(txOut.getScript());
      if (addr.prefixed() == state->address->GetChainedAddress().prefixed()) {
         continue;
      }

      auto ledgerDelegate = bdm->getLedgerDelegateForScrAddr(walletId_, addr.prefixed());
      if (!ledgerDelegate) {
         continue;
      }
      uint32_t pageId = 0;
      while (true) {
         const auto page = ledgerDelegate->getHistoryPage(pageId);
         if (page.empty()) {
            break;
         }
         for (auto &led : page) {
            if ((led.getBlockNum() < entry.getBlockNum()) || (led.getTxHash() == entry.getTxHash())) {
               continue;
            }
            const auto succTx = bdm->getTxByHash(led.getTxHash());
            if (!succTx.isInitialized()) {
               return true;
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
         pageId++;
      }
   }
   return false;
}

//returns true if
// - coins from initial transaction sent in any amount to any address
// - anything is sent from verification transaction
bool AddressVerificator::IsRevokeTransaction(const LedgerEntryData& entry
   , const std::shared_ptr<AddressVarificationData>& state)
{
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   auto tx = bdm->getTxByHash(entry.getTxHash());
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

bool AddressVerificator::IsBSRevokeTranscation(const LedgerEntryData& entry, const std::shared_ptr<AddressVarificationData>& state)
{
   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      return false;
   }
   auto tx = bdm->getTxByHash(entry.getTxHash());
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
      , address->GetChainedAddress().display<std::string()>);

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

   const auto &bdm = PyBlockDataManager::instance();
   if (bdm && (bdm->GetState() == PyBlockDataManagerState::Ready)) {
      listener_->setRefreshEnabled(false);
      bdm->registerWallet(internalWallet_, addresses, walletId_, false);
      listener_->setRefreshEnabled(true);
      logger_->debug("[AddressVerificator::RegisterAddresses] registered {} addresses", addresses.size());

      pendingRegAddresses_.clear();
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

std::vector<UTXO> AddressVerificator::GetVerificationInputs() const
{
   std::vector<UTXO> result = internalWallet_->getSpendableTxOutListForValue();
   const auto &zcInputs = internalWallet_->getSpendableZCList();
   result.insert(result.begin(), zcInputs.begin(), zcInputs.end());
   return result;
}

std::vector<UTXO> AddressVerificator::GetRevokeInputs() const
{
   std::vector<UTXO> result;
   for (const auto &utxo : internalWallet_->getSpendableTxOutListForValue()) {
      if ((utxo.getValue() == GetAuthAmount()) && (utxo.getTxOutIndex() == 1)) {
         result.push_back(utxo);
      }
   }
   return result;
}

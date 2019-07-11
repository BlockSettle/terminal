#include "AddressVerificator.h"

#include "ArmoryConnection.h"
#include "BinaryData.h"
#include "FastLock.h"
#include "BlockDataManagerConfig.h"

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
};

AddressVerificator::AddressVerificator(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ArmoryConnection> &armory
   , const std::string& walletId, verification_callback callback)
   : ArmoryCallbackTarget(armory.get())
   , logger_(logger)
   , walletId_(walletId)
   , userCallback_(callback)
   , stopExecution_(false)
{
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
   while(true) {
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
      logger_->debug("BS address: {}", address.display());
      bsAddressList_.emplace(address.prefixed());
   }
   return true;
}

bool AddressVerificator::StartAddressVerification(const std::shared_ptr<AuthAddress>& address)
{
   auto addressCopy = std::make_shared<AuthAddress>(*address);
   const auto addr = addressCopy->GetChainedAddress();

   if (bsAddressList_.empty() || (bsAddressList_.find(addr) != bsAddressList_.end())) {
      if (userCallback_) {
         userCallback_(address, AddressVerificationState::VerificationFailed);
      }
      return false;
   }

   if (AddressWasRegistered(addressCopy)) {
      logger_->debug("[AddressVerificator::StartAddressVerification] adding verification command to queue: {}"
         , addressCopy->GetChainedAddress().display());
      if (registered_) {
         AddCommandToQueue(CreateAddressValidationCommand(addressCopy));
      }
      else {
         AddCommandToWaitingUpdateQueue(addressCopy->GetChainedAddress().id().toBinStr()
            , CreateAddressValidationCommand(addressCopy));
      }
      return true;
   }

   return RegisterUserAddress(addressCopy);
}

void AddressVerificator::AddCommandToQueue(ExecutionCommand&& command)
{
   std::unique_lock<std::mutex> locker(dataMutex_);
   commandsQueue_.emplace(std::move(command));
   dataAvailable_.notify_all();
}

void AddressVerificator::AddCommandToWaitingUpdateQueue(const std::string &key, ExecutionCommand&& command)
{
   FastLock locker(waitingForUpdateQueueFlag_);
   if (waitingForUpdateQueue_.find(key) == waitingForUpdateQueue_.end()) {
      waitingForUpdateQueue_.emplace(key, std::move(command));
   }
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
      state->currentState = state->nbTransactions ? AddressVerificationState::VerificationFailed
         : AddressVerificationState::NotSubmitted;
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

   if (!armory_ || (armory_->state() != ArmoryState::Ready)) {
      logger_->error("[AddressVerificator::ValidateAddress] invalid Armory state {}", (int)armory_->state());
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   const auto &cbCollectOutTXs = [this, state](const std::vector<Tx> &txs) {
      for (const auto &tx : txs) {
         state->txs[tx.getThisHash()] = tx;
      }
      doValidateAddress(state);
   };
   const auto &cbCollectInitialTXs = [this, state, cbCollectOutTXs](const std::vector<Tx> &txs) {
      std::set<BinaryData> txOutHashes;
      for (const auto &tx : txs) {
         state->txs[tx.getThisHash()] = tx;
         for (size_t i = 0; i < tx.getNumTxIn(); ++i) {
            if (!tx.isInitialized()) {
               continue;
            }
            TxIn in = tx.getTxInCopy(i);
            if (!in.isInitialized()) {
               continue;
            }
            OutPoint op = in.getOutPoint();
            if (state->txs.find(op.getTxHash()) == state->txs.end()) {
               txOutHashes.insert(op.getTxHash());
            }
         }
      }
      if (txOutHashes.empty()) {
         doValidateAddress(state);
      }
      else {
         armory_->getTXsByHash(txOutHashes, cbCollectOutTXs);
      }
   };
   const auto &cbLedger = [this, state, cbCollectInitialTXs]
        (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
      try {
         const auto &le = entries.get();
         if (le.empty()) {
            state->currentState = AddressVerificationState::NotSubmitted;
            addressRetries_.erase(state->address->GetChainedAddress().prefixed());
            ReturnValidationResult(state);
            return;
         }
         state->nbTransactions += le.size();
         state->entries = le;
         for (const auto &tx : bsTXs_) {
            state->txs[tx.getThisHash()] = tx;
         }
         std::set<BinaryData> initialTxHashes;
         for (const auto &entry : le) {
            state->value += entry.getValue();
            const auto &itTX = state->txs.find(entry.getTxHash());
            if (itTX == state->txs.end()) {
               initialTxHashes.insert(entry.getTxHash());
            }
         }

         if (initialTxHashes.empty()) {
            cbCollectInitialTXs(bsTXs_);
         }
         else {
            armory_->getTXsByHash(initialTxHashes, cbCollectInitialTXs);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[AddressVerificator::ValidateAddress] Return data " \
            "error - {}", e.what());
      }
   };
   const auto &cbLedgerDelegate = [state, cbLedger](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      state->nbTransactions = 0;
      delegate->getHistoryPage(0, cbLedger);
   };
   const auto addr = state->address->GetChainedAddress();
   {
      std::unique_lock<std::mutex>  lock(cbMutex_);
      cbValidate_[addr] = cbLedgerDelegate;
   }
   if (!armory_->getLedgerDelegateForAddress(walletId_, addr)) {
      const auto &prefixedAddress = state->address->GetChainedAddress().id();
      if ((state->currentState == AddressVerificationState::InProgress) && (addressRetries_[prefixedAddress] < MaxAadressValidationErrorCount)) {
         logger_->debug("[AddressVerificator::ValidateAddress] Failed to get ledger for {} - retrying command", walletId_);
         AddCommandToWaitingUpdateQueue(prefixedAddress.toBinStr(), CreateAddressValidationCommand(state));
         addressRetries_[prefixedAddress]++;    // reschedule validation since error occured
      }
      else {
         logger_->error("[AddressVerificator::ValidateAddress] Failed to validate address {}", state->address->GetChainedAddress().display());
         state->currentState = AddressVerificationState::VerificationFailed;
         ReturnValidationResult(state);
         addressRetries_.erase(prefixedAddress);
      }
      return;
   }
}

void AddressVerificator::onLedgerForAddress(const bs::Address &addr, const std::shared_ptr<AsyncClient::LedgerDelegate> &ledger)
{
   if (!ledger) {
      logger_->error("[{}] no ledger for {} returned", __func__, addr.display());
      return;
   }
   const auto lbdInvokeCb = [this, addr, ledger]
   (std::map<bs::Address, std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>> &map) -> bool
   {
      std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> cb = nullptr;
      {
         std::unique_lock<std::mutex> lock(cbMutex_);
         const auto &itCb = map.find(addr);
         if (itCb == map.end()) {
            return false;
         }
         cb = itCb->second;
         map.erase(itCb);
      }
      if (cb) {
         cb(ledger);
      }
      return true;
   };
   lbdInvokeCb(cbValidate_) || lbdInvokeCb(cbBSstate_) || lbdInvokeCb(cbBSaddrs_);
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
   const auto &cbCollectTXs = [state, cbCheckState](const std::vector<Tx> &txs) {
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         state->txs[txHash] = tx;
      }
      cbCheckState();
   };
   const auto &cbLedger = [this, state, cbCollectTXs, cbCheckState]
      (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
      try {
         const auto &le = entries.get();
         state->nbTransactions += le.size();
         state->entries = le;
         std::set<BinaryData> txHashSet;
         for (const auto &entry : le) {
            state->value += entry.getValue();
            const auto &itTX = state->txs.find(entry.getTxHash());
            if (itTX == state->txs.end()) {
               txHashSet.insert(entry.getTxHash());
            }
         }

         if (!txHashSet.empty()) {
            armory_->getTXsByHash(txHashSet, cbCollectTXs);
         }
         else {
            cbCheckState();
         }
      }
      catch (const std::exception &e) {
         logger_->error("[AddressVerificator::CheckBSAddressState] Return " \
            "data error - {}", e.what());
      }
   };
   const auto &cbLedgerDelegate = [state, cbLedger](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      state->entries.clear();
      delegate->getHistoryPage(0, cbLedger);  //? should we use more than 0 pageId?
   };
   const auto addr = state->address->GetChainedAddress();
   {
      std::unique_lock<std::mutex> lock(cbMutex_);
      cbBSstate_[addr] = cbLedgerDelegate;
   }
   if (!armory_->getLedgerDelegateForAddress(walletId_, addr)) {
      logger_->error("[AddressVerificator::CheckBSAddressState] Could not validate address. Looks like armory is offline.");
      if (state->bsAddressValidationErrorCount >= MaxBSAddressValidationErrorCount) {
         logger_->error("[AddressVerificator::CheckBSAddressState] marking address as failed to validate {}", state->bsFundingAddress.display());
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
   if (entryValue <= (int64_t)(GetAuthAmount() * 2)) {
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
   assert(state->initialTxHash.getSize() != 0);

   bool sentByUs = false;
   bool sentToBS = false;

   auto tx = state->txs[entry.getTxHash()];
   if (!tx.isInitialized()) {
      logger_->error("[AddressVerificator::IsVerificationTransaction] tx for hash {} is not inited"
         , entry.getTxHash().toHexStr(true));
      return false;
   }
   if (tx.getSumOfOutputs() != (GetAuthAmount() * 2)) {  // should be 2 outputs: 1000 to self and 1000 to BS address
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
      logger_->error("Revoke TX is not inited ({})", entry.getTxHash().toHexStr(true));
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
      logger_->error("BS revoke TX is not inited ({})", entry.getTxHash().toHexStr(true));
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

   AddCommandToWaitingUpdateQueue(address->GetChainedAddress().id().toBinStr(), CreateAddressValidationCommand(address));
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

   if (armory_ && (armory_->state() == ArmoryState::Ready)) {
      pendingRegAddresses_.clear();
      internalWallet_ = armory_->instantiateWallet(walletId_);
      regId_ = armory_->registerWallet(internalWallet_, walletId_, walletId_, addresses
         , [](const std::string &) {}, true);
      logger_->debug("[AddressVerificator::RegisterAddresses] registered {} addresses in {} with {}", addresses.size(), walletId_, regId_);
   }
   else {
      logger_->error("[AddressVerificator::RegisterAddresses] Armory not ready");
   }
}

void AddressVerificator::onRefresh(const std::vector<BinaryData> &ids, bool)
{
   const auto &it = std::find(ids.begin(), ids.end(), regId_);
   if (it == ids.end()) {
      return;
   }

   if (bsAddressList_.empty()) {
      logger_->error("[AddressVerificator::OnRefresh] BS address list is empty");
      return;
   }

   const auto &cbTXs = [this](std::vector<Tx> txs) {
      bsTXs_ = txs;
      registered_ = true;
      logger_->debug("[AddressVerificator::OnRefresh] received {} BS TXs", txs.size());

      FastLock waitQlock(waitingForUpdateQueueFlag_);
      std::unique_lock<std::mutex> commandQlock(dataMutex_);
      for (const auto &waitCmd : waitingForUpdateQueue_) {
         commandsQueue_.emplace(std::move(waitCmd.second));
      }
      waitingForUpdateQueue_.clear();
      if (!commandsQueue_.empty()) {
         dataAvailable_.notify_all();
      }
   };
   auto pages = std::make_shared<std::map<bs::Address, uint64_t>>();
   auto txHashSet = std::make_shared<std::set<BinaryData>>();
   for (const auto &bsAddr : bsAddressList_) {
      (*pages)[bsAddr] = 1;
   }
   for (const auto &bsAddr : bsAddressList_) {
      const auto &cbDelegate = [this, cbTXs, pages, txHashSet, bsAddr]
                               (const std::shared_ptr<AsyncClient::LedgerDelegate> delegate)->void {
         const auto &cbPageCnt = [this, pages, bsAddr, delegate, txHashSet, cbTXs]
                                 (ReturnMessage<uint64_t> pageCnt)->void {
            try {
               const auto &inPageCnt = pageCnt.get();
               (*pages)[bsAddr] = inPageCnt;
               for (uint64_t i = 0; i < inPageCnt; ++i) {
                  const auto &cbLedger = [this, pages, bsAddr, txHashSet, cbTXs, i]
                     (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
                     try {
                        for (const auto &entry : entries.get()) {
                           txHashSet->insert(entry.getTxHash());
                        }
                     }
                     catch (const std::exception &e) {
                        logger_->error("[AddressVerificator::OnRefresh] Return " \
                           "data error (getHistoryPage) - {} - Page {}", e.what(), i);
                     }
                     (*pages)[bsAddr]--;
                     if (!(*pages)[bsAddr]) {
                        pages->erase(bsAddr);
                        if (pages->empty()) {
                           if (txHashSet->empty()) {
                              logger_->error("[AddressVerificator::OnRefresh] failed to collect TX hashes");
                           }
                           else {
                              armory_->getTXsByHash(*txHashSet, cbTXs);
                           }
                        }
                     }
                  };
                  delegate->getHistoryPage(i, cbLedger);
               }
            }
            catch (const std::exception &e) {
               logger_->error("[AddressVerificator::OnRefresh] Return data " \
                  "error (getPageCount) - {}", e.what());
            }
         };
         delegate->getPageCount(cbPageCnt);
      };
      {
         std::unique_lock<std::mutex>  lock(cbMutex_);
         cbBSaddrs_[bsAddr] = cbDelegate;
      }
      armory_->getLedgerDelegateForAddress(walletId_, bsAddr);
   }
}

void AddressVerificator::GetVerificationInputs(std::function<void(std::vector<UTXO>)> cb) const
{
   auto result = std::make_shared<std::vector<UTXO>>();
   const auto &cbInternal = [this, cb, result]
                            (ReturnMessage<std::vector<UTXO>> utxos)->void {
      try {
         const auto &inUTXOs = utxos.get();
         result->insert(result->end(), inUTXOs.begin(), inUTXOs.end());
      }
      catch (const std::exception &e) {
         logger_->error("[AddressVerificator::GetVerificationInputs] Return " \
            "data error (getSpendableZCList UTXOs) - {}", e.what());
      }

      const auto &cbZC = [this, cb, result]
                         (ReturnMessage<std::vector<UTXO>> zcs)->void {
         try {
            const auto &inZCUTXOs = zcs.get();
            result->insert(result->begin(), inZCUTXOs.begin(), inZCUTXOs.end());
         }
         catch (const std::exception &e) {
            logger_->error("[AddressVerificator::GetVerificationInputs] " \
               "Return data error (getSpendableZCList ZCs) - {}", e.what());
         }

         cb(*result);
      };
      internalWallet_->getSpendableZCList(cbZC);
   };
   internalWallet_->getSpendableTxOutListForValue(UINT64_MAX, cbInternal);
}

void AddressVerificator::GetRevokeInputs(std::function<void(std::vector<UTXO>)> cb) const
{
   const auto &cbInternal = [this, cb](ReturnMessage<std::vector<UTXO>> utxos) {
      std::vector<UTXO> result;

      try {
         for (const auto &utxo : utxos.get()) {
            if ((utxo.getValue() == GetAuthAmount()) && (utxo.getTxOutIndex() == 1)) {
               result.emplace_back(utxo);
            }
         }
      }
      catch (const std::exception &e) {
         logger_->error("[AddressVerificator::GetRevokeInputs] " \
            "Return data error - {}", e.what());
      }

      cb(result);
   };
   internalWallet_->getSpendableTxOutListForValue(UINT64_MAX, cbInternal);
}

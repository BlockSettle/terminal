#ifndef __AUTH_ADDRESS_VERIFICATOR_H__
#define __AUTH_ADDRESS_VERIFICATOR_H__

#include <atomic>
#include <condition_variable>
#include <queue>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "ArmoryConnection.h"
#include "AsyncClient.h"
#include "AuthAddress.h"

namespace spdlog {
   class logger;
}
namespace ClientClasses {
   class LedgerEntry;
}

struct AddressVerificationData;

// once we could connect to a super node we should not wait for refresh signals from armory
// we could just get info for address.
// Probably this could be saved for users if they want to use own armory but not in supernode mode
class AddressVerificator : ArmoryCallbackTarget
{
public:
   using verification_callback = std::function<void (const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)>;

private:
   using ExecutionCommand = std::function<void (void)>;
   struct AddressVarificationState;

public:
   AddressVerificator(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ArmoryConnection> &
      , const std::string& walletId, verification_callback callback);
   ~AddressVerificator() noexcept;

   AddressVerificator(const AddressVerificator&) = delete;
   AddressVerificator& operator = (const AddressVerificator&) = delete;

   AddressVerificator(AddressVerificator&&) = delete;
   AddressVerificator& operator = (AddressVerificator&&) = delete;

   bool HaveBSAddressList() const;
   bool SetBSAddressList(const std::unordered_set<std::string>& addressList);

   bool StartAddressVerification(const std::shared_ptr<AuthAddress>& address);
   void RegisterBSAuthAddresses();

   void RegisterAddresses();

   static uint64_t GetAuthAmount() { return 1000; }

   void GetVerificationInputs(std::function<void(std::vector<UTXO>)>) const;
   void GetRevokeInputs(std::function<void(std::vector<UTXO>)>) const;

private:
   bool startCommandQueue();
   bool stopCommandQueue();

   void commandQueueThreadFunction();

   void AddCommandToQueue(ExecutionCommand&& command);
   void AddCommandToWaitingUpdateQueue(const std::string &key, ExecutionCommand&& command);

   ExecutionCommand CreateAddressValidationCommand(const std::shared_ptr<AuthAddress>& address);
   ExecutionCommand CreateAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state);

   ExecutionCommand CreateBSAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state);

   bool AddressWasRegistered(const std::shared_ptr<AuthAddress>& address) const;
   bool RegisterUserAddress(const std::shared_ptr<AuthAddress>& address);

   void ValidateAddress(const std::shared_ptr<AddressVerificationData>& state);
   void doValidateAddress(const std::shared_ptr<AddressVerificationData>& state);
   void CheckBSAddressState(const std::shared_ptr<AddressVerificationData>& state);

   void ReturnValidationResult(const std::shared_ptr<AddressVerificationData>& state);

   bool IsInitialBsTransaction(const ClientClasses::LedgerEntry &, const std::shared_ptr<AddressVerificationData>& state, bool &isVerified);
   bool IsVerificationTransaction(const ClientClasses::LedgerEntry &, const std::shared_ptr<AddressVerificationData>& state, bool &isVerified);
   bool IsRevokeTransaction(const ClientClasses::LedgerEntry &, const std::shared_ptr<AddressVerificationData>& state);
   bool HasRevokeOutputs(const ClientClasses::LedgerEntry &, const std::shared_ptr<AddressVerificationData> &);

   bool IsBSRevokeTranscation(const ClientClasses::LedgerEntry &, const std::shared_ptr<AddressVerificationData>& state);

   void onLedgerForAddress(const bs::Address &, const std::shared_ptr<AsyncClient::LedgerDelegate> &) override;
   void onRefresh(const std::vector<BinaryData> &, bool) override;

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::string       walletId_;
   std::string       regId_;
   std::atomic_bool  registered_ = { false };
   verification_callback   userCallback_;

   //bsAddressList_ - list received from public bridge
   std::set<BinaryData>       bsAddressList_;
   std::vector<Tx>            bsTXs_;

   // addresses that were added to a wallet
   // user auth address list
   std::set<BinaryData>             authAddressSet_;
   mutable std::atomic_flag         authAddressSetFlag_ = ATOMIC_FLAG_INIT;

   std::set<BinaryData>             pendingRegAddresses_;
   mutable std::atomic_flag         pendingRegAddressFlag_ = ATOMIC_FLAG_INIT;

   std::unordered_map<std::string, ExecutionCommand>  waitingForUpdateQueue_;
   std::atomic_flag                 waitingForUpdateQueueFlag_ = ATOMIC_FLAG_INIT;

   // command queue
   std::thread                   commandQueueThread_;
   std::queue<ExecutionCommand>  commandsQueue_;
   std::condition_variable       dataAvailable_;
   mutable std::mutex            dataMutex_;
   std::atomic_bool              stopExecution_;

   std::shared_ptr<AsyncClient::BtcWallet>   internalWallet_;

   std::map<BinaryData, unsigned int> addressRetries_;

   std::mutex  cbMutex_;
   std::map<bs::Address, std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>>   cbBSaddrs_;
   std::map<bs::Address, std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>>   cbValidate_;
   std::map<bs::Address, std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>>   cbBSstate_;
};

#endif // __AUTH_ADDRESS_VERIFICATOR_H__

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
class ValidationAddressManager;

// once we could connect to a super node we should not wait for refresh signals from armory
// we could just get info for address.
// Probably this could be saved for users if they want to use own armory but not in supernode mode
class AddressVerificator : ArmoryCallbackTarget
{
public:
   using VerificationCallback = std::function<void (const bs::Address &, AddressVerificationState state)>;

private:
   using ExecutionCommand = std::function<void (void)>;

public:
   AddressVerificator(const std::shared_ptr<spdlog::logger>& logger, const std::shared_ptr<ArmoryConnection> &
      , VerificationCallback callback);
   ~AddressVerificator() noexcept override;

   AddressVerificator(const AddressVerificator&) = delete;
   AddressVerificator& operator = (const AddressVerificator&) = delete;
   AddressVerificator(AddressVerificator&&) = delete;
   AddressVerificator& operator = (AddressVerificator&&) = delete;

   bool HaveBSAddressList() const;
   bool SetBSAddressList(const std::unordered_set<std::string>& addressList);

   bool addAddress(const bs::Address &address);
   bool startAddressVerification();

   std::pair<bs::Address, UTXO> getRevokeData(const bs::Address &authAddr);

protected:
   void onNewBlock(unsigned int) override
   {
      refreshUserAddresses();
   }

   void onZCReceived(const std::vector<bs::TXEntry> &) override
   {
      refreshUserAddresses();
   }

private:
   bool startCommandQueue();
   bool stopCommandQueue();
   void commandQueueThreadFunction();

   void refreshUserAddresses();

   void AddCommandToQueue(ExecutionCommand&& command);
   void AddCommandToWaitingUpdateQueue(const std::string &key, ExecutionCommand&& command);

   ExecutionCommand CreateAddressValidationCommand(const bs::Address &address);
   ExecutionCommand CreateAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state);

   void validateAddress(const std::shared_ptr<AddressVerificationData>& state);

   void ReturnValidationResult(const std::shared_ptr<AddressVerificationData>& state);

private:
   std::shared_ptr<spdlog::logger>           logger_;
   std::unique_ptr<ValidationAddressManager> validationMgr_;
   VerificationCallback    userCallback_;

   //bsAddressList_ - list received from public bridge
   std::set<BinaryData>       bsAddressList_;

   std::unordered_map<std::string, ExecutionCommand>  waitingForUpdateQueue_;
   std::atomic_flag                 waitingForUpdateQueueFlag_ = ATOMIC_FLAG_INIT;

   // command queue
   std::thread                   commandQueueThread_;
   std::queue<ExecutionCommand>  commandsQueue_;
   std::condition_variable       dataAvailable_;
   mutable std::mutex            dataMutex_;
   std::atomic_bool              stopExecution_;

   std::set<bs::Address>   userAddresses_;
};

#endif // __AUTH_ADDRESS_VERIFICATOR_H__

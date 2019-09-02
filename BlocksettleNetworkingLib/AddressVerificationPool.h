#ifndef __ADDRESS_VERIFICATION_POOL_H__
#define __ADDRESS_VERIFICATION_POOL_H__

#include "AuthAddress.h"

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace spdlog {
   class logger;
}
class AddressVerificator;
class ArmoryConnection;

class AddressVerificationPool
{
public:
   using verificationCompletedCallback = std::function<void (AddressVerificationState state)>;
public:
   // pool Id will be used as wallet ID in verificator, as well as identifier in log
   AddressVerificationPool(const std::shared_ptr<spdlog::logger>& logger, const std::string& poolId
      , const std::shared_ptr<ArmoryConnection> &);

   ~AddressVerificationPool() noexcept = default;

   AddressVerificationPool(const AddressVerificationPool&) = delete;
   AddressVerificationPool& operator = (const AddressVerificationPool&) = delete;

   AddressVerificationPool(AddressVerificationPool&&) = delete;
   AddressVerificationPool& operator = (AddressVerificationPool&&) = delete;

   bool submitForVerification(const bs::Address &address
      , const verificationCompletedCallback& onCompleted);

   bool SetBSAddressList(const std::unordered_set<std::string>& addressList);

private:
   void completeVerification(const bs::Address &address, AddressVerificationState state);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::string                poolId_;

   using resultsCollection = std::unordered_map<std::string, std::queue<verificationCompletedCallback> >;

   std::atomic_flag  pendingLockerFlag_ = ATOMIC_FLAG_INIT;
   resultsCollection pendingResults_;

   std::shared_ptr<AddressVerificator>    verificator_;
};

#endif // __ADDRESS_VERIFICATION_POOL_H__

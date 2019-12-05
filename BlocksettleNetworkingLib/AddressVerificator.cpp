/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressVerificator.h"

#include "ArmoryConnection.h"
#include "AuthAddressLogic.h"
#include "BinaryData.h"
#include "BlockDataManagerConfig.h"
#include "FastLock.h"

#include <cassert>
#include <spdlog/spdlog.h>

struct AddressVerificationData
{
   bs::Address                   address;
   AddressVerificationState      currentState{};
};

AddressVerificator::AddressVerificator(const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ArmoryConnection> &armory, VerificationCallback callback)
   : ArmoryCallbackTarget()
   , logger_(logger)
   , validationMgr_(new ValidationAddressManager(armory))
   , userCallback_(std::move(callback))
   , stopExecution_(false)
{
   startCommandQueue();
   init(armory.get());
}

AddressVerificator::~AddressVerificator() noexcept
{
   cleanup();
   stopCommandQueue();
}

void AddressVerificator::startCommandQueue()
{
   commandQueueThread_ = std::thread(&AddressVerificator::commandQueueThreadFunction, this);
}

void AddressVerificator::stopCommandQueue()
{
   {
      std::unique_lock<std::mutex> locker(dataMutex_);
      stopExecution_ = true;
      dataAvailable_.notify_all();
   }

   if (commandQueueThread_.joinable()) {
      commandQueueThread_.join();
   }
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
         nextCommand = std::move(commandsQueue_.front());
         commandsQueue_.pop();
      }

      // process command
      nextCommand();
   }
}

bool AddressVerificator::SetBSAddressList(const std::unordered_set<std::string>& addressList)
{
   for (const auto &addr : addressList) {
      const auto bsAddr = bs::Address::fromAddressString(addr);
      if (bsAddressList_.find(bsAddr) != bsAddressList_.end()) {
         logger_->warn("[{}] BS address {} already exists in the list"
            , __func__, bsAddr.display());
         continue;
      }
      logger_->debug("[{}] BS address: {}", __func__, bsAddr.display());
      bsAddressList_.emplace(bsAddr.prefixed());
      validationMgr_->addValidationAddress(bsAddr);
   }
   return true;
}

bool AddressVerificator::addAddress(const bs::Address &address)
{
   if (bsAddressList_.empty() || (bsAddressList_.find(address) != bsAddressList_.end())) {
      if (userCallback_) {
         userCallback_(address, AddressVerificationState::VerificationFailed);
      }
      return false;
   }
   std::lock_guard<std::mutex> lock(userAddressesMutex_);
   userAddresses_.emplace(address);
   return true;
}

void AddressVerificator::startAddressVerification()
{
   if (bsAddressList_.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "BS address list is not set");
      return;
   }

   AddCommandToQueue([this] {
      try {
         auto rc = validationMgr_->goOnline();
         if (rc == 0) {
            SPDLOG_LOGGER_ERROR(logger_, "goOnline failed");
            return;
         }
         refreshUserAddresses();
      }
      catch (const std::exception &e) {
         logger_->error("[{}] failure: {}", __func__, e.what());
         return;
      }
   });
}

void AddressVerificator::refreshUserAddresses()
{
   std::lock_guard<std::mutex> lock(userAddressesMutex_);
   logger_->debug("[{}] updating {} user address[es]", __func__, userAddresses_.size());
   for (const auto &addr : userAddresses_) {
      AddCommandToQueue(CreateAddressValidationCommand(addr));
   }
}

void AddressVerificator::AddCommandToQueue(ExecutionCommand&& command)
{
   std::unique_lock<std::mutex> locker(dataMutex_);
   commandsQueue_.emplace(std::move(command));
   dataAvailable_.notify_all();
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateAddressValidationCommand(const bs::Address &address)
{
   auto state = std::make_shared<AddressVerificationData>();

   state->address = address;
   state->currentState = AddressVerificationState::InProgress;

   return CreateAddressValidationCommand(state);
}

AddressVerificator::ExecutionCommand AddressVerificator::CreateAddressValidationCommand(const std::shared_ptr<AddressVerificationData>& state)
{
   return [this, state]() {
      this->validateAddress(state);
   };
}

void AddressVerificator::validateAddress(const std::shared_ptr<AddressVerificationData> &state)
{   // if we are here, that means that address was added, and now it is time to try to validate it
   if (bsAddressList_.empty()) {
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   if (!armory_ || (armory_->state() != ArmoryState::Ready)) {
      logger_->error("[AddressVerificator::ValidateAddress] invalid Armory state {}", (int)armory_->state());
      state->currentState = AddressVerificationState::VerificationFailed;
      ReturnValidationResult(state);
      return;
   }

   try {
      state->currentState = AuthAddressLogic::getAuthAddrState(*validationMgr_, state->address);
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to validate state for {}: {}", __func__
         , state->address.display(), e.what());
      state->currentState = AddressVerificationState::VerificationFailed;
   }
   ReturnValidationResult(state);
}

void AddressVerificator::ReturnValidationResult(const std::shared_ptr<AddressVerificationData>& state)
{
   if (userCallback_) {
      userCallback_(state->address, state->currentState);
   }
}

bool AddressVerificator::HaveBSAddressList() const
{
   return !bsAddressList_.empty();
}

std::pair<bs::Address, UTXO> AddressVerificator::getRevokeData(const bs::Address &authAddr)
{
   return AuthAddressLogic::getRevokeData(*validationMgr_, authAddr);
}

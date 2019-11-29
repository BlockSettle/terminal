/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ADDRESS_VALIDATION_STATE_H__
#define __ADDRESS_VALIDATION_STATE_H__

#include "Address.h"

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>
#include <map>

class AddressState;

// used in CC leaf validation proc as a holder of current validation state
// simply counting address/history page/TX completion and calling CB once completed
class AddressValidationState
{
public:
   using onValidationComletedCB = std::function<void ()>;

public:
   explicit AddressValidationState(const onValidationComletedCB& cb);
   ~AddressValidationState() noexcept = default;

   AddressValidationState(const AddressValidationState&) = delete;
   AddressValidationState& operator = (const AddressValidationState&) = delete;

   AddressValidationState(AddressValidationState&&) = delete;
   AddressValidationState& operator = (AddressValidationState&&) = delete;

   void SetAddressList(const std::vector<bs::Address>& addressList);
   void SetAddressPagesCount(const bs::Address& address, const uint64_t pagesCount);
   void SetAddressPageTxCount(const bs::Address& address, const uint64_t pageId, const uint64_t txCount);
   void OnTxProcessed(const bs::Address& address, const uint64_t pageId);

   bool IsValidationStarted() const;
   bool IsValidationCompleted() const;

private:
   onValidationComletedCB                                cb_;
   std::map<bs::Address, std::shared_ptr<AddressState>>  addressStateMap_;
   std::atomic<uint64_t>                                 completedAddressesCount_{0};
};

#endif // __ADDRESS_VALIDATION_STATE_H__

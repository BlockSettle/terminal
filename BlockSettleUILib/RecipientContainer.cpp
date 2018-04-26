#include "RecipientContainer.h"

#include "ScriptRecipient.h"
#include "BlockDataManagerConfig.h"
#include "BTCNumericTypes.h"
#include "Wallets.h"


constexpr double InvalidAmount = 0;

RecipientContainer::RecipientContainer()
   : amount_(InvalidAmount)
{}

bool RecipientContainer::IsReady() const
{
   return (amount_ != InvalidAmount) && (!address_.isNull() || (addrEntry_ != nullptr));
}

bool RecipientContainer::SetAddress(const bs::Address &address)
{
   address_ = address;
   addrEntry_ = nullptr;
   return true;
}

bool RecipientContainer::SetAddressEntry(const std::shared_ptr<AddressEntry> &address)
{
   addrEntry_ = address;
   address_.clear();
   return true;
}

void RecipientContainer::ResetAddress()
{
   address_.clear();
   addrEntry_ = nullptr;
}

bs::Address RecipientContainer::GetAddress() const
{
   if (addrEntry_ != nullptr) {
      return addrEntry_->getPrefixedHash();
   }
   if (!address_.isNull()) {
      return address_;
   }
   return bs::Address();
}

bool RecipientContainer::SetAmount(double amount, bool isMax)
{
   amount_ = amount;
   isMax_ = isMax;
   return true;
}

std::shared_ptr<ScriptRecipient> RecipientContainer::GetScriptRecipient() const
{
   if (addrEntry_ != nullptr) {
      return addrEntry_->getRecipient(amount_ * BTCNumericTypes::BalanceDivider);
   }
   if (address_.isNull()) {
      return nullptr;
   }
   return address_.getRecipient(amount_);
}

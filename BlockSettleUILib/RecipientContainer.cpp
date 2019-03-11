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
   return (amount_ != InvalidAmount) && !address_.isNull();
}

bool RecipientContainer::SetAddress(const bs::Address &address)
{
   address_ = address;
   return true;
}

void RecipientContainer::ResetAddress()
{
   address_.clear();
}

bs::Address RecipientContainer::GetAddress() const
{
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
   if (address_.isNull()) {
      return nullptr;
   }
   return address_.getRecipient(amount_);
}

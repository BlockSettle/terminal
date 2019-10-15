#include "RecipientContainer.h"

#include "ScriptRecipient.h"
#include "BlockDataManagerConfig.h"
#include "BTCNumericTypes.h"
#include "Wallets.h"

RecipientContainer::RecipientContainer()
{}

bool RecipientContainer::IsReady() const
{
   return !xbtAmount_.isZero() && !address_.isNull();
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
   xbtAmount_.SetValue(amount);
   isMax_ = isMax;
   return true;
}

double RecipientContainer::GetAmount() const
{
   return xbtAmount_.GetValueBitcoin();
}

std::shared_ptr<ScriptRecipient> RecipientContainer::GetScriptRecipient() const
{
   if (!IsReady()) {
      return nullptr;
   }
   return address_.getRecipient(xbtAmount_);
}

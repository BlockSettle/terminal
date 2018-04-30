#ifndef __RECIPIENT_CONTAINER_H__
#define __RECIPIENT_CONTAINER_H__

#include <string>
#include <memory>
#include "Address.h"


class ScriptRecipient;
class BinaryData;
class AddressEntry;

class RecipientContainer
{
public:
   RecipientContainer();
   ~RecipientContainer() noexcept = default;

   RecipientContainer(const RecipientContainer&) = delete;
   RecipientContainer& operator = (const RecipientContainer&) = delete;

   RecipientContainer(RecipientContainer&&) = delete;
   RecipientContainer& operator = (RecipientContainer&&) = delete;

public:
   bool IsReady() const;

   bool SetAddressEntry(const std::shared_ptr<AddressEntry> &ae);
   bool SetAddress(const bs::Address& address);
   bs::Address GetAddress() const;
   void ResetAddress();

   bool SetAmount(double amount, bool isMax = false);
   double GetAmount() const { return amount_; }
   bool IsMaxAmount() const { return isMax_; }

   std::shared_ptr<ScriptRecipient> GetScriptRecipient() const;

private:
   std::shared_ptr<AddressEntry> addrEntry_;
   bs::Address address_;
   double      amount_;
   bool        isMax_;
};

#endif // __RECIPIENT_CONTAINER_H__

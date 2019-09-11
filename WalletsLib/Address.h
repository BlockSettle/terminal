#ifndef __BS_ADDRESS_H__
#define __BS_ADDRESS_H__

#include <string>

#include "Addresses.h"
#include "BinaryData.h"
#include "BTCNumericTypes.h"
#include "TxClasses.h"


namespace bs {
   class Address : public BinaryData
   {
   public:
      enum Format {
         Auto,
         Base58,
         Hex,
         Bech32,
         Binary
      };

      Address(AddressEntryType aet = AddressEntryType_Default) : BinaryData(), aet_(aet) {}
      Address(const BinaryData &data, AddressEntryType aet = AddressEntryType_Default);
      Address(const BinaryDataRef &data, AddressEntryType aet = AddressEntryType_Default);
      Address(const std::string &data, const Format &f = Auto, AddressEntryType aet = AddressEntryType_Default);

      bool operator==(const Address &) const;
      bool operator!=(const Address &addr) const { return !((*this) == addr); }
      bool operator<(const Address &addr) const { return (id() < addr.id()); }
      bool operator>(const Address &addr) const { return (id() > addr.id()); }

      AddressEntryType getType() const { return aet_; }
      Format format() const { return format_; }
      bool isValid() const;
      void clear();
      std::string display(Format format = Auto) const;
      BinaryData prefixed() const;
      BinaryData unprefixed() const;
      BinaryData id() const;
      BinaryData hash160() const;
      BinaryData getWitnessScript() const;
      std::shared_ptr<ScriptRecipient> getRecipient(uint64_t amount) const;
      std::shared_ptr<ScriptRecipient> getRecipient(double amount) const;

      size_t getInputSize() const;
      size_t getWitnessDataSize() const;  // returns UINT32_MAX if irrelevant

      static bs::Address fromPubKey(const BinaryData &data, AddressEntryType aet = AddressEntryType_Default) {
         //does not work with MS scripts nor P2PK (does not operate on hashes), neither P2WSH (uses hash256)
         return Address(BtcUtils::getHash160(data), aet);
      }
      static bs::Address fromPubKey(const std::string &data, AddressEntryType aet = AddressEntryType_Default) {
         return fromPubKey(BinaryData::CreateFromHex(data), aet);
      }
      static bs::Address fromHash(const BinaryData &hash, AddressEntryType aet);
      static bs::Address fromTxOutScript(const BinaryData &);
      static bs::Address fromTxOut(const TxOut &);
      static bs::Address fromUTXO(const UTXO &);
      static bs::Address fromRecipient(const std::shared_ptr<ScriptRecipient> &);
      static AddressEntryType guessAddressType(const BinaryData &addr);

      static size_t getPayoutWitnessDataSize();

      // Try to initialize txinRedeemSizeBytes_, witnessDataSizeBytes_ and isInputSW_.
      // Usually used to calculate TX vsize.
      static void decorateUTXOs(std::vector<UTXO> &utxos);
      static std::vector<UTXO> decorateUTXOsCopy(const std::vector<UTXO> &utxos);

   private:
      Format               format_ = Format::Binary;
      AddressEntryType     aet_;
      mutable BinaryData   prefixed_ = {};
      mutable BinaryData   witnessScr_ = {};

      bool isProperHash() const;
      BinaryData getWitnessH160() const;
   };

}  //namespace bs

#endif //__BS_ADDRESS_H__

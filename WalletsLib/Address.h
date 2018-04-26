#ifndef __BS_ADDRESS_H__
#define __BS_ADDRESS_H__

#include <string>

#include <QByteArray>

#include "BinaryData.h"
#include "BTCNumericTypes.h"
#include "Wallets.h"

namespace bs {
   class Address : public BinaryData
   {
   public:
      enum Format {
         Auto,
         Base58,
         Hex,
         Bech32
      };

      Address(AddressEntryType aet = AddressEntryType_Default) : BinaryData(), aet_(aet) {}
      Address(const BinaryData &data, AddressEntryType aet = AddressEntryType_Default);
      Address(const BinaryDataRef &data, AddressEntryType aet = AddressEntryType_Default);
      Address(const QByteArray &data, AddressEntryType aet = AddressEntryType_Default);
      Address(const QString &data, Format f = Auto, AddressEntryType aet = AddressEntryType_Default);
      Address(const std::string &data, Format f = Auto, AddressEntryType aet = AddressEntryType_Default);

      bool operator==(const Address &) const;
      bool operator!=(const Address &addr) const { return !((*this) == addr); }
      bool operator<(const Address &addr) const { return (id() < addr.id()); }
      bool operator>(const Address &addr) const { return (id() > addr.id()); }

      AddressEntryType getType() const { return aet_; }
      bool isValid() const;
      template<typename TRetVal = QString> TRetVal display(Format format = Auto) const;
      BinaryData prefixed() const;
      BinaryData unprefixed() const;
      BinaryData id() const;
      BinaryData hash160() const;
      BinaryData getWitnessScript() const;
      std::shared_ptr<ScriptRecipient> getRecipient(uint64_t amount) const;
      std::shared_ptr<ScriptRecipient> getRecipient(double amount) const;

      static bs::Address fromPubKey(const BinaryData &data, AddressEntryType aet = AddressEntryType_Default) {
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

   private:
      AddressEntryType     aet_;
      mutable BinaryData   prefixed_ = {};
      mutable BinaryData   witnessScr_ = {};

      bool isProperHash() const;
      BinaryData getWitnessH160() const;
   };

}  //namespace bs

#endif //__BS_ADDRESS_H__

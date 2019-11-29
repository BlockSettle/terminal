/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_ADDRESS_H__
#define __BS_ADDRESS_H__

#include <string>

#include "Addresses.h"
#include "BinaryData.h"
#include "BTCNumericTypes.h"
#include "TxClasses.h"
#include "XBTAmount.h"

namespace bs {
   class Address : public BinaryData
   {
   private:
      Address(const BinaryDataRef& data, AddressEntryType aet);
      Address(const BinaryDataRef& data);
   public:
      static AddressEntryType mapTxOutScriptType(TXOUT_SCRIPT_TYPE scrType);

   public:
      enum Format {
         Uninitialized,
         Base58,
         Hex,
         Bech32,
         Binary
      };

      Address(void) : BinaryData()
      {}

      Address(const Address&) = default;

      bool operator==(const Address &) const;
      bool operator!=(const Address &addr) const { return !((*this) == addr); }
      bool operator<(const Address &addr) const { return (id() < addr.id()); }
      bool operator>(const Address &addr) const { return (id() > addr.id()); }

      AddressEntryType getType() const { return aet_; }
      Format format() const { return format_; }
      bool isValid() const;
      void clear();
      std::string display() const;
      BinaryData prefixed() const;
      BinaryData unprefixed() const;
      BinaryData id() const;

      std::shared_ptr<ScriptRecipient> getRecipient(const XBTAmount& amount) const;

      size_t getInputSize() const;
      size_t getWitnessDataSize() const;  // returns UINT32_MAX if irrelevant

      static bs::Address fromHash(
         const BinaryData &hash);

      static bs::Address fromPubKey(const BinaryData &data, AddressEntryType aet);
      static bs::Address fromTxOut(const TxOut &);
      static bs::Address fromUTXO(const UTXO &);
      static bs::Address fromRecipient(const std::shared_ptr<ScriptRecipient> &);
      static bs::Address fromScript(const BinaryData&);
      static bs::Address fromAddressString(const std::string&);
      static bs::Address fromAddressEntry(const AddressEntry&);
      static bs::Address fromMultisigScript(const BinaryData&, AddressEntryType);

      static size_t getPayoutWitnessDataSize();

      static uint64_t getNativeSegwitDustAmount();
      static uint64_t getNestedSegwitDustAmount();

      // Try to initialize txinRedeemSizeBytes_, witnessDataSizeBytes_ and isInputSW_.
      // Usually used to calculate TX vsize.
      static void decorateUTXOs(std::vector<UTXO> &utxos);
      static std::vector<UTXO> decorateUTXOsCopy(const std::vector<UTXO> &utxos);

      // Compute fee for max value (copied from CoinSelection::getFeeForMaxVal).
      // See OtcClient::estimatePayinFeeWithoutChange for usage example.
      static uint64_t getFeeForMaxVal(const std::vector<UTXO> &utxos, size_t txOutSize, float feePerByte);


   private:
      Format               format_ = Format::Uninitialized;
      AddressEntryType     aet_ = AddressEntryType_Default;
   };

}  //namespace bs

#endif //__BS_ADDRESS_H__

#include "Address.h"
#include "BlockDataManagerConfig.h"
#include <bech32.h>


bs::Address::Address(const BinaryData &data, AddressEntryType aet)
   : BinaryData(data), aet_(aet)
{
   if (aet == AddressEntryType_Default) {
      aet_ = guessAddressType(*this);
   }
}

bs::Address::Address(const BinaryDataRef &data, AddressEntryType aet)
   : BinaryData(data), aet_(aet)
{
   if (aet == AddressEntryType_Default) {
      aet_ = guessAddressType(*this);
   }
}

/*!bs::Address::Address(const QByteArray &data, AddressEntryType aet)
   : BinaryData((uint8_t*)(data.data()), data.size()), aet_(aet)
{
   if (aet == AddressEntryType_Default) {
      aet_ = guessAddressType(*this);
   }
}

bs::Address::Address(const QString &data, const Format &f, AddressEntryType aet)
   : Address(data.toStdString(), f, aet)
{}*/

bs::Address::Address(const std::string& data, const Format &format, AddressEntryType aet)
   : BinaryData((uint8_t*)(data.data()), data.size()), format_(format), aet_(aet)
{
   BinaryData parsedData;
   if (format_ == Format::Auto) {
      if (data.empty()) {
         return;
      }
      const auto &prefix = data.substr(0, 2);
      if ((prefix == SEGWIT_ADDRESS_MAINNET_HEADER) || (prefix == SEGWIT_ADDRESS_TESTNET_HEADER)) {
         format_ = Format::Bech32;
      }
      else {
         try {
            BinaryData base58In(data);
            base58In.append('\0'); // Remove once base58toScrAddr() is fixed.
            parsedData = BtcUtils::base58toScrAddr(base58In);
            format_ = Format::Base58;
         }
         catch (const std::exception &) {
            try {
               parsedData = BinaryData::CreateFromHex(data);
               if (!parsedData.isNull()) {
                  format_ = Format::Hex;
               }
            }
            catch (const std::exception &) {}
         }
      }
   }
   if (format_ == Format::Auto) {
      throw std::invalid_argument("can't detect input format");
   }

   if (!parsedData.isNull()) {
      copyFrom(parsedData);
   }
   else {
      switch (format_) {
      case Format::Base58:
         try {
            BinaryData base58In(data);
            base58In.append('\0'); // Remove once base58toScrAddr() is fixed.
            copyFrom(BtcUtils::base58toScrAddr(base58In));
         }
         catch (const std::runtime_error &) {}
         break;

      case Format::Bech32:
         try {
            copyFrom(BtcUtils::segWitAddressToScrAddr(data));
            if (getSize() == 20) {
               aet_ = aet = AddressEntryType_P2WPKH;
            }
            else if (getSize() == 32) {
               aet_ = aet = AddressEntryType_P2WSH;
            }
         }
         catch (const std::exception &) {}
         break;

      case Format::Hex:
         try {
            copyFrom(BinaryData::CreateFromHex(data));
         }
         catch (const std::exception &) {}
         break;
      }
   }

   if ((aet == AddressEntryType_Default) && !isNull()) {
      aet_ = guessAddressType(*this);
   }
}

static AddressEntryType mapTxOutScriptType(TXOUT_SCRIPT_TYPE scrType)
{
   auto aet = AddressEntryType_Default;
   switch (scrType) {
   case TXOUT_SCRIPT_STDHASH160:
      aet = AddressEntryType_P2PKH;
      break;

   case TXOUT_SCRIPT_P2SH:
   case TXOUT_SCRIPT_NONSTANDARD:
      aet = AddressEntryType_P2SH;
      break;

   case TXOUT_SCRIPT_P2WPKH:
      aet = AddressEntryType_P2WPKH;
      break;

   case TXOUT_SCRIPT_P2WSH:
      aet = AddressEntryType_P2WSH;
      break;

   default: break;
   }
   return aet;
}

bs::Address bs::Address::fromHash(const BinaryData &hash, AddressEntryType aet)
{
   if ((aet == AddressEntryType_P2SH) && (hash.getSize() == 20)) {
      BinaryData binAddr;
      binAddr.append(NetworkConfig::getScriptHashPrefix());
      binAddr.append(hash);
      return bs::Address(binAddr, aet);
   }
   return bs::Address(hash, aet);
}

bs::Address bs::Address::fromTxOutScript(const BinaryData &script)
{
   const auto scrType = BtcUtils::getTxOutScriptType(script);
   const auto aet = mapTxOutScriptType(scrType);
   return bs::Address::fromHash(BtcUtils::getTxOutRecipientAddr(script, scrType), aet);
}

bs::Address bs::Address::fromTxOut(const TxOut &out)
{
   const auto scrType = out.getScriptType();
   const auto binData = out.getScrAddressStr();
   switch (scrType) {
   case TXOUT_SCRIPT_P2WPKH:
      return bs::Address(binData.getSliceCopy(1, 20), mapTxOutScriptType(scrType));

   case TXOUT_SCRIPT_P2WSH:
      return bs::Address(binData.getSliceCopy(1, 32), mapTxOutScriptType(scrType));

   default: break;
   }
   return bs::Address(binData, mapTxOutScriptType(scrType));
}

bs::Address bs::Address::fromUTXO(const UTXO &utxo)
{
   return fromTxOutScript(utxo.getScript());
}

bs::Address bs::Address::fromRecipient(const std::shared_ptr<ScriptRecipient> &recip)
{
   BinaryRefReader brr(recip->getSerializedScript());
   brr.get_uint64_t();  // skip value
   const auto script = brr.get_BinaryDataRef(brr.getSizeRemaining());

   BinaryRefReader brr_script(script);
   auto byte0 = brr_script.get_uint8_t();
   brr_script.get_uint8_t();
   brr_script.get_uint8_t();

   switch (byte0)
   {
   case 25:
      brr_script.get_uint8_t();
      return bs::Address(brr_script.get_BinaryData(20), AddressEntryType_P2PKH);

   case 22:
      return bs::Address(brr_script.get_BinaryData(20), AddressEntryType_P2WPKH);

   case 23:
      return bs::Address(brr_script.get_BinaryData(20), AddressEntryType_P2SH);

   case 34:
      return bs::Address(brr_script.get_BinaryData(32), AddressEntryType_P2WSH);

   default:    break;
   }
   return {};
}

bool bs::Address::isValid() const
{
   if ((aet_ == AddressEntryType_Default) || !isProperHash()) {
      return false;
   }
   return true;
}

void bs::Address::clear()
{
   BinaryData::clear();
   aet_ = AddressEntryType_Default;
}

bool bs::Address::isProperHash() const
{
   if (isNull() || ((getSize() != 20) && (getSize() != 21) && (getSize() != 32) && (getSize() != 33))) {
      return false;
   }
   return true;
}

AddressEntryType bs::Address::guessAddressType(const BinaryData &addr)
{
   //this shouldn't be used to fill in for default address type!
   if (addr.getSize() == 21) {
      const auto prefix = addr[0];
      if (prefix == NetworkConfig::getPubkeyHashPrefix()) {
         return AddressEntryType_P2PKH;
      }
      else if (prefix == NetworkConfig::getScriptHashPrefix()) {
         return static_cast<AddressEntryType>(AddressEntryType_P2SH + AddressEntryType_P2WPKH);
      }
      else if (prefix == SCRIPT_PREFIX_P2WPKH) {
         return AddressEntryType_P2WPKH;
      }
   }
   else if (addr.getSize() == 33) {
      const auto prefix = addr[0];
      if (prefix == SCRIPT_PREFIX_P2WSH) {
         return AddressEntryType_P2WSH;
      }
   }
   else if (addr.getSize() >= 32) {
      return AddressEntryType_P2WSH;
   }
   else if (addr.getSize() == 20) {
      return AddressEntryType_P2WPKH;
   }
   return AddressEntryType_Default;
}

// static
size_t bs::Address::getPayoutWitnessDataSize()
{
   // Payout TX has more complicated witness data (because it uses 1 of 2 signatures).
   return 148;
}

std::string bs::Address::display(Format format) const
{
   if (!isProperHash()) {
      return {};
   }

   const auto fullAddress = prefixed();
   std::string result;

   switch (format)
   {
   case Base58:
      try {
         result = BtcUtils::scrAddrToBase58(fullAddress).toBinStr();
         break;
      }
      catch (const std::exception &) {
         return {};
      }

   case Hex:
      return fullAddress.toHexStr();

   case Bech32:
      try {
         result = BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();
         break;
      }
      catch (const std::exception &) {
         return {};
      }

   case Auto:
   case Binary:
      switch (aet_) {
      case AddressEntryType_P2PKH:
      case AddressEntryType_P2SH:
      case (AddressEntryType_P2SH + AddressEntryType_P2WPKH):
         result = BtcUtils::scrAddrToBase58(fullAddress).toBinStr();
         break;

      case AddressEntryType_P2WPKH:
      case AddressEntryType_P2WSH:
         result = BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();
         break;

      default:
         return fullAddress.toHexStr();
      }
      break;
   default:
      throw std::logic_error("unsupported address format");
   }

   if (*result.rbegin() == 0) {
      result.resize(result.size() - 1);
   }
   return result;
}

BinaryData bs::Address::prefixed() const
{
   if ((getSize() == 20) || (getSize() == 32)) {   // Missing the prefix, we have to add it
      if (prefixed_.isNull()) {
         /***
         Nested address types are a bit mask on native address types, they cannot be switched
         on as is. Prefixes precede human readable addresses, a false positive will lead to 
         loss of coins as the address defines the output script to send the coins to. Any
         failure to produce a valid prefix should lead to a critical failure, hence the throws.
         ***/

         auto prefix = AddressEntry::getPrefixByte(aet_);
         prefixed_.append(prefix);
         prefixed_.append(unprefixed());
      }
      return prefixed_;
   }
   return *this;
}

BinaryData bs::Address::hash160() const
{
   if (!isProperHash()) {
      return BinaryData();
   }
   if (getSize() == 21) {
      return getSliceRef(1, 20);
   }
   return *this;
}

BinaryData bs::Address::unprefixed() const
{
   if (!isProperHash()) {
      return BinaryData{};
   }
   switch (aet_) {
   case AddressEntryType_P2SH:
      return (getSize() > 21) ? BinaryData{} : getWitnessH160();

   default: break;
   }

   if (getSize() == 21) {
      return getSliceRef(1, 20);
   }
   else if (getSize() == 33) {
      return getSliceRef(1, 32);
   }
   return *this;
}

BinaryData bs::Address::id() const
{
   return prefixed();
}

bool bs::Address::operator==(const bs::Address &addr) const
{
   /*
   This is the correct comparator (as opposed to checking for 
   size first, as the carried data may or may not be prefixed
   in the first place, leading to false negative size checks.
   */

   return prefixed() == addr.prefixed();
}

std::shared_ptr<ScriptRecipient> bs::Address::getRecipient(uint64_t value) const
{
   try {
      auto type = getType() & ~ADDRESS_COMPRESSED_MASK;
      auto nestedType = type & ADDRESS_NESTED_MASK;

      if (nestedType == 0)
      {
         switch (type) 
         {
         case AddressEntryType_P2PKH:
            return std::make_shared<Recipient_P2PKH>(unprefixed(), value);

         case AddressEntryType_P2WPKH:
            return std::make_shared<Recipient_P2WPKH>(unprefixed(), value);

         default:
            return nullptr;
         }
      }
      else
      {
         switch (nestedType)
         {
         case AddressEntryType_P2WSH:
            return std::make_shared<Recipient_P2WSH>(unprefixed(), value);

         case AddressEntryType_P2SH:
            return std::make_shared<Recipient_P2SH>(unprefixed(), value);

         default:
            return nullptr;
         }
      }
   }
   catch (...) {
      return nullptr;
   }
}

BinaryData bs::Address::getWitnessH160() const
{
   if (getSize() == 21) {
      return getSliceCopy(1, 20);
   }
   return BtcUtils::getHash160(getWitnessScript());
}

BinaryData bs::Address::getWitnessScript() const
{
   if (witnessScr_.isNull()) {
      const BinaryData data = (getSize() == 21) ? getSliceCopy(1, 20) : (BinaryData)*this;
      Recipient_P2WPKH recipient(data, 0);
      auto& script = recipient.getSerializedScript();
      witnessScr_ = script.getSliceCopy(9, script.getSize() - 9);
   }
   return witnessScr_;
}

std::shared_ptr<ScriptRecipient> bs::Address::getRecipient(double amount) const
{
   return getRecipient((uint64_t)(amount * BTCNumericTypes::BalanceDivider));
}

size_t bs::Address::getInputSize() const
{  //borrowed from Armory's Addresses mainly
   switch (getType()) {
   case AddressEntryType_P2PKH:     return 114 + 33;
   case AddressEntryType_P2WSH:     return 41;
   case (AddressEntryType_P2SH + AddressEntryType_P2WPKH):
   case AddressEntryType_P2SH:      return 22 + 40;   //Treat P2SH only as nested P2SH-P2WPKH
   case AddressEntryType_P2WPKH:    return 40;
   default:       return 0;
   }
}

size_t bs::Address::getWitnessDataSize() const
{
   switch (getType()) {
   case AddressEntryType_P2WSH:     return 34;  //based on getP2WSHOutputScript()
   case AddressEntryType_P2WPKH:    return 108; // Armory's AddressEntry_P2WPKH
   case (AddressEntryType_P2SH + AddressEntryType_P2WPKH):
   case AddressEntryType_P2SH:      return 108; //Treat P2SH only as nested P2SH-P2WPKH
   default:       return UINT32_MAX;
   }
}

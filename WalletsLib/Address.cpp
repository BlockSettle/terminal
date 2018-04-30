#include "Address.h"
#include "BlockDataManagerConfig.h"
#include <QString>
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

bs::Address::Address(const QByteArray &data, AddressEntryType aet)
   : BinaryData((uint8_t*)(data.data()), data.size()), aet_(aet)
{
   if (aet == AddressEntryType_Default) {
      aet_ = guessAddressType(*this);
   }
}

bs::Address::Address(const QString &data, Format f, AddressEntryType aet)
   : Address(data.toStdString(), f, aet)
{}

bs::Address::Address(const std::string& data, Format format, AddressEntryType aet)
   : BinaryData((uint8_t*)(data.data()), data.size()), aet_(aet)
{
   BinaryData parsedData;
   if (format == Format::Auto) {
      if (data.empty()) {
         return;
      }
      const auto &prefix = data.substr(0, 2);
      if ((prefix == SEGWIT_ADDRESS_MAINNET_HEADER) || (prefix == SEGWIT_ADDRESS_TESTNET_HEADER)) {
         format = Format::Bech32;
      }
      else {
         try {
            parsedData = BtcUtils::base58toScrAddr(data);
            format = Format::Base58;
         }
         catch (const std::exception &) {
            try {
               parsedData = BinaryData::CreateFromHex(data);
               if (!parsedData.isNull()) {
                  format = Format::Hex;
               }
            }
            catch (const std::exception &) {}
         }
      }
   }
   if (format == Format::Auto) {
      throw std::invalid_argument("can't detect input format");
   }

   if (!parsedData.isNull()) {
      copyFrom(parsedData);
   }
   else {
      switch (format) {
      case Format::Base58:
         try {
            copyFrom(BtcUtils::base58toScrAddr(data));
         }
         catch (const std::runtime_error &) {}
         break;

      case Format::Bech32:
         try {
            copyFrom(BtcUtils::segWitAddressToScrAddr(data));
            aet_ = aet = AddressEntryType_P2WPKH;
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
      binAddr.append(BlockDataManagerConfig::getScriptHashPrefix());
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

bool bs::Address::isProperHash() const
{
   if (isNull() || ((getSize() != 20) && (getSize() != 21) && (getSize() != 32) && (getSize() != 33))) {
      return false;
   }
   return true;
}

AddressEntryType bs::Address::guessAddressType(const BinaryData &addr)
{
   if (addr.getSize() == 21) {
      const auto prefix = addr[0];
      if (prefix == BlockDataManagerConfig::getPubkeyHashPrefix()) {
         return AddressEntryType_P2PKH;
      }
      else if (prefix == BlockDataManagerConfig::getScriptHashPrefix()) {
         return AddressEntryType_P2SH;
      }
      else if (prefix == SCRIPT_PREFIX_P2WSH) {
         return AddressEntryType_P2WSH;
      }
      else if (prefix == SCRIPT_PREFIX_P2WPKH) {
         return AddressEntryType_P2WPKH;
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

namespace bs {
   template<> std::string Address::display(Format format) const
   {
      if (!isProperHash()) {
         return {};
      }

      const auto &fullAddress = prefixed();

      switch (format)
      {
      case Base58:
         try {
            return BtcUtils::scrAddrToBase58(fullAddress).toBinStr();
         }
         catch (const std::exception &) {
            return {};
         }

      case Hex:
         return fullAddress.toHexStr();

      case Bech32:
         try {
            return BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();
         }
         catch (const std::exception &) {
            return {};
         }

      case Auto:
         switch (aet_) {
         case AddressEntryType_P2SH:
         case AddressEntryType_P2PKH:
            return BtcUtils::scrAddrToBase58(fullAddress).toBinStr();

         case AddressEntryType_P2WPKH:
         case AddressEntryType_P2WSH:
            return BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();

         default:
            return fullAddress.toHexStr();
         }

      default:
         throw std::logic_error("unsupported address format");
      }
   }

   template<> QString Address::display(Format format) const
   {
      return QString::fromStdString(display<std::string>(format));
   }
}

BinaryData bs::Address::prefixed() const
{
   if ((getSize() == 20) || (getSize() == 32)) {   // Missing the prefix, we have to add it
      if (prefixed_.isNull()) {
         auto prefix = BlockDataManagerConfig::getPubkeyHashPrefix();
         switch (aet_) {
         case AddressEntryType_P2SH:
            prefix = BlockDataManagerConfig::getScriptHashPrefix();
            break;
         case AddressEntryType_Multisig:
            prefix = SCRIPT_PREFIX_MULTISIG;
            break;
         case AddressEntryType_P2WSH:
            prefix = SCRIPT_PREFIX_P2WSH;
            break;
         case AddressEntryType_P2WPKH:
            prefix = SCRIPT_PREFIX_P2WPKH;
            break;
         default: break;
         }
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
   if (getSize() != addr.getSize()) {
      return false;
   }
   return (id() == addr.id());
}

shared_ptr<ScriptRecipient> bs::Address::getRecipient(uint64_t value) const
{
   switch (getType()) {
   case AddressEntryType_P2PKH:
      return std::make_shared<Recipient_P2PKH>(unprefixed(), value);

   case AddressEntryType_P2WSH:
      return std::make_shared<Recipient_PW2SH>(unprefixed(), value);

   case AddressEntryType_P2SH:
      return std::make_shared<Recipient_P2SH>(unprefixed(), value);

   case AddressEntryType_P2WPKH:
      return std::make_shared<Recipient_P2WPKH>(unprefixed(), value);

   default:
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

shared_ptr<ScriptRecipient> bs::Address::getRecipient(double amount) const
{
   return getRecipient((uint64_t)(amount * BTCNumericTypes::BalanceDivider));
}

#include "Address.h"
#include "BlockDataManagerConfig.h"
#include <bech32.h>

bs::Address::Address(const BinaryDataRef& data) :
   BinaryData(data)
{
   /* Builds from prefixed scrAddr */

   if (data.getSize() == 0)
      throw std::runtime_error("invalid address data");

   switch (getPtr()[0])
   {
   case SCRIPT_PREFIX_HASH160:
   case SCRIPT_PREFIX_HASH160_TESTNET:
      if (data.getSize() != 21)
         throw std::runtime_error("invalid data size");

      if (getPtr()[0] != NetworkConfig::getPubkeyHashPrefix())
         throw std::runtime_error("network mismatch!");

      aet_ = AddressEntryType_P2PKH;
      format_ = Format::Base58;
      break;

   case SCRIPT_PREFIX_P2SH:
   case SCRIPT_PREFIX_P2SH_TESTNET:
      if (data.getSize() != 21)
         throw std::runtime_error("invalid data size");

      if (getPtr()[0] != NetworkConfig::getScriptHashPrefix())
         throw std::runtime_error("network mismatch!");

      aet_ = AddressEntryType_P2SH;
      format_ = Format::Base58;
      break;

   case SCRIPT_PREFIX_P2WPKH:
      if (data.getSize() != 21)
         throw std::runtime_error("invalid data size");
      aet_ = AddressEntryType_P2WPKH;
      format_ = Format::Bech32;
      break;

   case SCRIPT_PREFIX_P2WSH:
      if(data.getSize() != 33)
         throw std::runtime_error("invalid data size");
      aet_ = AddressEntryType_P2WSH;
      format_ = Format::Bech32;
      break;

   default:
      throw std::runtime_error("unabled to resolve address type");
   }
}

bs::Address::Address(const BinaryDataRef& data, AddressEntryType aet) :
   aet_(aet)
{
   switch (aet)
   {
   case AddressEntryType_P2PKH:
   case AddressEntryType_P2SH:
      if (data.getSize() != 20)
         throw std::runtime_error("invalid data length");
      format_ = Format::Base58;
      break;

   case AddressEntryType_P2WPKH:
      if(data.getSize() != 20)
         throw std::runtime_error("invalid data length");
      format_ = Format::Bech32;
      break;

   case AddressEntryType_P2WSH:
      if (data.getSize() != 32)
         throw std::runtime_error("invalid data length");
      format_ = Format::Bech32;
      break;

   default:
      throw std::runtime_error("unexpected address type");
   }

   if (format_ == Format::Uninitialized)
      throw std::runtime_error("unable to resolve address format");

   append(AddressEntry::getPrefixByte(aet));
   append(data);
}

bs::Address bs::Address::fromAddressString(const std::string& data)
{
   if (data.empty()) {
      throw std::runtime_error("empty string");
   }
   const auto &prefix = data.substr(0, 2);
   if ((prefix == SEGWIT_ADDRESS_MAINNET_HEADER) || (prefix == SEGWIT_ADDRESS_TESTNET_HEADER)) {

      auto&& scrAddr = BtcUtils::segWitAddressToScrAddr(data);
      if (scrAddr.getSize() == 20) {
         return bs::Address(scrAddr, AddressEntryType_P2WPKH);
      }
      else if (scrAddr.getSize() == 32) {
         return bs::Address(scrAddr, AddressEntryType_P2WSH);
      }
   }
   else {
      BinaryData base58In(data);
      base58In.append('\0'); // Remove once base58toScrAddr() is fixed.
      auto&& scrAddr = BtcUtils::base58toScrAddr(base58In);

      if (scrAddr.getPtr()[0] == NetworkConfig::getPubkeyHashPrefix())
      {
         return bs::Address(
            scrAddr.getSliceRef(1, scrAddr.getSize() - 1), AddressEntryType_P2PKH);
      }
      else if (scrAddr.getPtr()[0] == NetworkConfig::getScriptHashPrefix())
      {
         return bs::Address(
            scrAddr.getSliceRef(1, scrAddr.getSize() - 1), AddressEntryType_P2SH);
      }
   }

   throw std::runtime_error("failed to decode address string");
}


static AddressEntryType mapTxOutScriptType(TXOUT_SCRIPT_TYPE scrType)
{
   auto aet = AddressEntryType_Default;
   switch (scrType) {
   case TXOUT_SCRIPT_STDHASH160:
      aet = AddressEntryType_P2PKH;
      break;

   case TXOUT_SCRIPT_P2SH:
      aet = AddressEntryType_P2SH;
      break;

   case TXOUT_SCRIPT_P2WPKH:
      aet = AddressEntryType_P2WPKH;
      break;

   case TXOUT_SCRIPT_P2WSH:
      aet = AddressEntryType_P2WSH;
      break;

   default: 
      throw std::runtime_error("not a hash");
   }
   return aet;
}

bs::Address bs::Address::fromHash(const BinaryData &hash)
{
   return bs::Address(hash);
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

   default:
      throw std::runtime_error("could not resolve address from recipient");
   }
}

bs::Address bs::Address::fromTxOut(const TxOut &out)
{
   return bs::Address(out.getScrAddressStr());
}

bs::Address bs::Address::fromUTXO(const UTXO &utxo)
{
   return fromScript(utxo.getScript());
}

bs::Address bs::Address::fromAddressEntry(const AddressEntry& aeObj)
{
   return bs::Address(aeObj.getPrefixedHash());
}

bool bs::Address::isValid() const
{
   if ((aet_ == AddressEntryType_Default) || 
      getSize() == 0 || format_ == Format::Uninitialized) {
      return false;
   }
   return true;
}

void bs::Address::clear()
{
   BinaryData::clear();
   aet_ = AddressEntryType_Default;
}

// static
size_t bs::Address::getPayoutWitnessDataSize()
{
   // Payout TX has more complicated witness data (because it uses 1 of 2 signatures).
   return 148;
}

uint64_t bs::Address::getNativeSegwitDustAmount()
{
   return 294;
}

uint64_t bs::Address::getNestedSegwitDustAmount()
{
   return 546;
}

void bs::Address::decorateUTXOs(std::vector<UTXO> &utxos)
{
   for (auto &utxo : utxos) {  // some kind of decoration code to replace the code above
      const bs::Address recipAddr(utxo.getRecipientScrAddr());
      utxo.txinRedeemSizeBytes_ = unsigned(recipAddr.getInputSize());
      utxo.witnessDataSizeBytes_ = unsigned(recipAddr.getWitnessDataSize());
      utxo.isInputSW_ = (recipAddr.getWitnessDataSize() != UINT32_MAX);
   }
}

std::vector<UTXO> bs::Address::decorateUTXOsCopy(const std::vector<UTXO> &utxos)
{
   auto result = utxos;
   decorateUTXOs(result);
   return result;
}

uint64_t bs::Address::getFeeForMaxVal(const std::vector<UTXO> &utxos, size_t txOutSize, float feePerByte)
{
   //version, locktime, txin & txout count + outputs size
   size_t txSize = 10 + txOutSize;
   size_t witnessSize = 0;

   for (const auto& utxo : utxos) {
      txSize += utxo.getInputRedeemSize();
      if (utxo.isSegWit()) {
         witnessSize += utxo.getWitnessDataSize();
      }
   }

   if (witnessSize != 0) {
      txSize += 2;
      txSize += utxos.size();
   }

   uint64_t fee = uint64_t(feePerByte * float(txSize));
   fee += uint64_t(float(witnessSize) * 0.25f * feePerByte);
   return fee;
}

std::string bs::Address::display() const
{
   const auto fullAddress = prefixed();
   std::string result;

   switch (format_)
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

   case Binary:
   {
      auto nestedFlag = aet_ & ADDRESS_NESTED_MASK;

      if (nestedFlag != 0)
      {
         /*
         This is a nested address, the nested type defines the
         address string type
         */

         switch (nestedFlag)
         {
         case AddressEntryType_P2SH:
            result = BtcUtils::scrAddrToBase58(fullAddress).toBinStr();
            break;

         case AddressEntryType_P2WSH:
            result = BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();
            break;

         default:
            throw std::logic_error("unexpected nested type");
         }

         //we have an address string, break out of the parent case 
         break;
      }

      //address isn't nested if we got this far
      switch (aet_) {
      case AddressEntryType_P2PKH:
         result = BtcUtils::scrAddrToBase58(fullAddress).toBinStr();
         break;

      case AddressEntryType_P2WPKH:
         result = BtcUtils::scrAddrToSegWitAddress(unprefixed()).toBinStr();
         break;

      default:
         return fullAddress.toHexStr();
      }
      break;
   }

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
   return *this;
}

BinaryData bs::Address::unprefixed() const
{
   auto&& prefixedHash = prefixed();
   return prefixedHash.getSliceCopy(1, prefixedHash.getSize() - 1);
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

std::shared_ptr<ScriptRecipient> bs::Address::getRecipient(const XBTAmount& amount) const
{
   const uint64_t value = amount.GetValue();

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

bs::Address bs::Address::fromPubKey(const BinaryData &data, AddressEntryType aet)
{
   //check pubkey is valid
   if (data.getSize() != 33)
      throw std::runtime_error("pubkey isn't compressed");

   if (!CryptoECDSA().VerifyPublicKeyValid(data))
      throw std::runtime_error("invalid pubkey");

   bs::Address addr;
   addr.aet_ = aet;
   addr.append(AddressEntry::getPrefixByte(aet));

   auto nestedType = aet & ADDRESS_NESTED_MASK;
   auto baseType = aet & ADDRESS_TYPE_MASK;

   if (baseType == 0)
      throw std::runtime_error("invalid aet");

   BinaryData script;
   BinaryData hash;
   bool isSegWit = false;
   switch (baseType)
   {
   case AddressEntryType_P2PKH:
      addr.format_ = Format::Base58;
      hash = BtcUtils::getHash160(data);
      script = BtcUtils::getP2PKHScript(hash);
      break;

   case AddressEntryType_P2WPKH:
      addr.format_ = Format::Bech32;
      hash = BtcUtils::getHash160(data);
      script = BtcUtils::getP2WPKHOutputScript(hash);
      isSegWit = true;
      break;

   case AddressEntryType_P2PK:
      script = BtcUtils::getP2PKScript(data);
      break;

   default:
      throw std::runtime_error("invalid aet");
   }

   if (nestedType != 0)
   {
      switch (nestedType)
      {
      case AddressEntryType_P2SH:
         hash = BtcUtils::getHash160(script);
         addr.format_ = Format::Base58;
         break;

      case AddressEntryType_P2WSH:
         if (!isSegWit)
            throw std::runtime_error("cannot nest non SW into P2WSH");

         hash = BtcUtils::getSha256(script);
         addr.format_ = Format::Bech32;
         break;

      default:
         throw std::runtime_error("invalid aet");
      }
   }

   if (hash.getSize() == 0)
      throw std::runtime_error("failed to generate prefixed hash");

   addr.append(hash);
   return addr;
}

bs::Address bs::Address::fromScript(const BinaryData& data)
{
   const auto scrType = BtcUtils::getTxOutScriptType(data);
   const auto aet = mapTxOutScriptType(scrType);
   BinaryData prefixed;
   prefixed.append(AddressEntry::getPrefixByte(aet));
   prefixed.append(BtcUtils::getTxOutRecipientAddr(data, scrType));

   return bs::Address(prefixed);
}

bs::Address bs::Address::fromMultisigScript(const BinaryData& data, AddressEntryType aet)
{
   if (BtcUtils::getTxOutScriptType(data) != TXOUT_SCRIPT_MULTISIG)
      throw std::runtime_error("this isn't a multisig script");

   BinaryData hash;
   switch (aet)
   {
   case AddressEntryType_P2SH:
      hash = BtcUtils::getHash160(data);
      break;

   case AddressEntryType_P2WSH:
      hash = BtcUtils::getSha256(data);
      break;

   default:
      throw std::runtime_error("only naked nested types allowed for MS scripts");
   }

   return bs::Address(hash, aet);
}
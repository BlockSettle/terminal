////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2011-2015, Armory Technologies, Inc.                        //
//  Distributed under the GNU Affero General Public License (AGPL v3)         //
//  See LICENSE-ATI or http://www.gnu.org/licenses/agpl.html                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "BtcUtils.h"
#include "EncryptionUtils.h"
#include "BlockDataManagerConfig.h"
#include "bech32/ref/c++/segwit_addr.h"

using namespace std;

const BinaryData BtcUtils::BadAddress_ = BinaryData::CreateFromHex("0000000000000000000000000000000000000000");
const BinaryData BtcUtils::EmptyHash_  = BinaryData::CreateFromHex("0000000000000000000000000000000000000000000000000000000000000000");
const string BtcUtils::base64Chars_ = string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

////////////////////////////////////////////////////////////////////////////////
const map<char, uint8_t> BtcUtils::base64Vals_ = {
   { 'A', 0 }, { 'B', 1 }, { 'C', 2 }, { 'D', 3 }, { 'E', 4 }, { 'F', 5 },
   { 'G', 6 }, { 'H', 7 }, { 'I', 8 }, { 'J', 9 }, { 'K', 10 }, { 'L', 11 },
   { 'M', 12 }, { 'N', 13 }, { 'O', 14 }, { 'P', 15 }, { 'Q', 16 }, { 'R', 17 },
   { 'S', 18 }, { 'T', 19 }, { 'U', 20 }, { 'V', 21 }, { 'W', 22 }, { 'X', 23 },
   { 'Y', 24 }, { 'Z', 25 }, { 'a', 26 }, { 'b', 27 }, { 'c', 28 }, { 'd', 29 },
   { 'e', 30 }, { 'f', 31 }, { 'g', 32 }, { 'h', 33 }, { 'i', 34 }, { 'j', 35 },
   { 'k', 36 }, { 'l', 37 }, { 'm', 38 }, { 'n', 39 }, { 'o', 40 }, { 'p', 41 },
   { 'q', 42 }, { 'r', 43 }, { 's', 44 }, { 't', 45 }, { 'u', 46 }, { 'v', 47 }, 
   { 'w', 48 }, { 'x', 49 }, { 'y', 50 }, { 'z', 51 }, { '0', 52 }, { '1', 53 }, 
   { '2', 54 }, { '3', 55 }, { '4', 56 }, { '5', 57 }, { '6', 58 }, { '7', 59 },
   { '8', 60 }, { '9', 61 }, { '+', 62 }, { '/', 63 }
};

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::computeID(const SecureBinaryData& pubkey)
{
   BinaryDataRef bdr(pubkey);
   auto&& h160 = getHash160(bdr);
   
   BinaryWriter bw;
   bw.put_uint8_t(NetworkConfig::getPubkeyHashPrefix());
   bw.put_BinaryDataRef(h160.getSliceRef(0, 5));

   //now reverse it
   auto& data = bw.getData();
   auto ptr = data.getPtr();
   BinaryWriter bwReverse;
   for (unsigned i = 0; i < data.getSize(); i++)
   {
      bwReverse.put_uint8_t(ptr[data.getSize() - 1 - i]);
   }

   auto&& b58_5bytes = BtcUtils::base58_encode(bwReverse.getDataRef());

   return b58_5bytes;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::getHMAC256(const SecureBinaryData& key,
   const SecureBinaryData& message)
{
   BinaryData digest;
   digest.resize(32);
   
   getHMAC256(key.getPtr(), key.getSize(), 
      message.getCharPtr(), message.getSize(),
      digest.getPtr());

   return digest;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::getHMAC512(const SecureBinaryData& key,
   const SecureBinaryData& message)
{
   BinaryData digest;
   digest.resize(64);

   getHMAC512(key.getPtr(), key.getSize(),
      message.getCharPtr(), message.getSize(),
      digest.getPtr());

   return digest;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::getHMAC256(const BinaryData& key,
   const string& message)
{
   BinaryData digest;
   digest.resize(32);
   
   getHMAC256(key.getPtr(), key.getSize(), 
      message.c_str(), message.size(),
      digest.getPtr());

   return digest;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::getHMAC512(const BinaryData& key,
   const string& message)
{
   BinaryData digest;
   digest.resize(64);

   getHMAC512(key.getPtr(), key.getSize(),
      message.c_str(), message.size(),
      digest.getPtr());

   return digest;
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData BtcUtils::getHMAC512(const string& key,
   const SecureBinaryData& message)
{
   SecureBinaryData digest;
   digest.resize(64);

   getHMAC512(key.c_str(), key.size(),
      message.getPtr(), message.getSize(),
      digest.getPtr());

   return digest;
}


////////////////////////////////////////////////////////////////////////////////
void BtcUtils::getHMAC256(const uint8_t* keyptr, size_t keylen,
   const char* msgptr, size_t msglen, uint8_t* digest)
{
   BinaryDataRef key_bdr(keyptr, keylen);
   BinaryDataRef msg_bdr((uint8_t*)msgptr, msglen);

   CryptoSHA2::getHMAC256(key_bdr, msg_bdr, digest);
}

////////////////////////////////////////////////////////////////////////////////
void BtcUtils::getHMAC512(const void* keyptr, size_t keylen,
   const void* msgptr, size_t msglen, void* digest)
{
   BinaryDataRef key_bdr((uint8_t*)keyptr, keylen);
   BinaryDataRef msg_bdr((uint8_t*)msgptr, msglen);

   CryptoSHA2::getHMAC512(key_bdr, msg_bdr, (uint8_t*)digest);
}

////////////////////////////////////////////////////////////////////////////////
SecureBinaryData BtcUtils::computeChainCode_Armory135(
   const SecureBinaryData& privateRoot)
{
   /*
   Armory 1.35c defines the chaincode as HMAC<SHA256> with:
   key: double SHA256 of the root key
   message: 'Derive Chaincode from Root Key'
   */

   auto&& hmacKey = BtcUtils::hash256(privateRoot);
   string hmacMsg("Derive Chaincode from Root Key");
   SecureBinaryData chainCode(32);

   getHMAC256(hmacKey.getPtr(), hmacKey.getSize(),
      hmacMsg.c_str(), hmacMsg.size(), chainCode.getPtr());

   return chainCode;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::computeDataId(const SecureBinaryData& data,
   const string& message)
{
   if (data.getSize() == 0)
      throw runtime_error("cannot compute id for empty data");

   if (message.size() == 0)
      throw runtime_error("cannot compute id for empty message");

   //hmac the hash256 of the data with message
   auto&& hmacKey = BtcUtils::hash256(data);
   BinaryData id(32);

   getHMAC256(hmacKey.getPtr(), hmacKey.getSize(),
      message.c_str(), message.size(), id.getPtr());

   //return last 16 bytes
   return id.getSliceCopy(16, 16);
}

#ifndef LIBBTC_ONLY
////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::rsToDerSig(BinaryDataRef bdr)
{
   if (bdr.getSize() != 64)
      throw runtime_error("unexpected rs sig length");

   //split r and s
   auto r_bdr = bdr.getSliceRef(0, 32);
   auto s_bdr = bdr.getSliceRef(32, 32);

   //trim r
   unsigned trim = 0;
   auto ptr = r_bdr.getPtr();
   while (trim < r_bdr.getSize())
   {
      if (ptr[trim] != 0)
         break;

      trim++;
   }

   auto r_trim = bdr.getSliceRef(trim, 32 - trim);
   
   //prepend 0 for negative values
   BinaryWriter bwR;
   if (*r_trim.getPtr() > 0x7f)
      bwR.put_uint8_t(0);
   bwR.put_BinaryDataRef(r_trim);

   //get lowS
   auto&& lowS = CryptoECDSA::computeLowS(s_bdr);

   //prepend 0 for negative values
   BinaryWriter bwS;
   if (*lowS.getPtr() > 0x7f)
      bwS.put_uint8_t(0);
   bwS.put_BinaryData(lowS);

   BinaryWriter bw;

   //code byte
   bw.put_uint8_t(0x30);

   //size
   bw.put_uint8_t(4 + bwR.getSize() + bwS.getSize());

   //r code byte
   bw.put_uint8_t(0x02);

   //r size
   bw.put_uint8_t(bwR.getSize());

   //r
   bw.put_BinaryDataRef(bwR.getDataRef());

   //s code byte
   bw.put_uint8_t(0x02);

   //s size
   bw.put_uint8_t(bwS.getSize());

   //s
   bw.put_BinaryDataRef(bwS.getDataRef());

   return bw.getData();
}
#endif

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::getTxOutScrAddr(BinaryDataRef script,
   TXOUT_SCRIPT_TYPE type)
{
   BinaryWriter bw;
   if (type == TXOUT_SCRIPT_NONSTANDARD)
      type = getTxOutScriptType(script);

   auto h160Prefix = NetworkConfig::getPubkeyHashPrefix();
   auto scriptPrefix = NetworkConfig::getScriptHashPrefix();

   switch (type)
   {
      case(TXOUT_SCRIPT_STDHASH160) :
         bw.put_uint8_t(h160Prefix);
         bw.put_BinaryData(script.getSliceCopy(3, 20));
         return bw.getData();
      case(TXOUT_SCRIPT_P2WPKH) :
         bw.put_uint8_t(SCRIPT_PREFIX_P2WPKH);
         bw.put_BinaryData(script.getSliceCopy(2, 20));
         return bw.getData();
      case(TXOUT_SCRIPT_P2WSH) :
         bw.put_uint8_t(SCRIPT_PREFIX_P2WSH);
         bw.put_BinaryData(script.getSliceCopy(2, 32));
         return bw.getData();
      case(TXOUT_SCRIPT_STDPUBKEY65) :
         bw.put_uint8_t(h160Prefix);
         bw.put_BinaryData(getHash160(script.getSliceRef(1, 65)));
         return bw.getData();
      case(TXOUT_SCRIPT_STDPUBKEY33) :
         bw.put_uint8_t(h160Prefix);
         bw.put_BinaryData(getHash160(script.getSliceRef(1, 33)));
         return bw.getData();
      case(TXOUT_SCRIPT_P2SH) :
         bw.put_uint8_t(scriptPrefix);
         bw.put_BinaryData(script.getSliceCopy(2, 20));
         return bw.getData();
      case(TXOUT_SCRIPT_NONSTANDARD) :
         bw.put_uint8_t(SCRIPT_PREFIX_NONSTD);
         bw.put_BinaryData(getHash160(script));
         return bw.getData();
      case(TXOUT_SCRIPT_MULTISIG) :
         bw.put_uint8_t(SCRIPT_PREFIX_MULTISIG);
         bw.put_BinaryData(getMultisigUniqueKey(script));
         return bw.getData();
      case(TXOUT_SCRIPT_OPRETURN) :
      {
         bw.put_uint8_t(SCRIPT_PREFIX_NONSTD);
         unsigned msg_pos = 1;
         if (script.getSize() > 77)
            msg_pos += 2;
         else if (script.getSize() > 1)
            ++msg_pos;

         bw.put_BinaryData(
            script.getSliceRef(msg_pos, script.getSize() - msg_pos));
         return bw.getData();
      }

      default:
         LOGERR << "What kind of TxOutScript did we get?";
         return BinaryData(0);
   }
}

/////////////////////////////////////////////////////////////////////////////
//no copy version, the regular one is too slow for scanning operations
TxOutScriptRef BtcUtils::getTxOutScrAddrNoCopy(BinaryDataRef script)
{
   TxOutScriptRef outputRef;

   auto p2pkh_prefix = 
      SCRIPT_PREFIX(NetworkConfig::getPubkeyHashPrefix());
   auto p2sh_prefix = 
      SCRIPT_PREFIX(NetworkConfig::getScriptHashPrefix());
   
   auto type = getTxOutScriptType(script);
   switch (type)
   {
   case(TXOUT_SCRIPT_STDHASH160) :
   {
      outputRef.type_ = p2pkh_prefix;
      outputRef.scriptRef_ = move(script.getSliceRef(3, 20));
      break;
   }

   case(TXOUT_SCRIPT_P2WPKH) :
   {
      outputRef.type_ = SCRIPT_PREFIX_P2WPKH;
      outputRef.scriptRef_ = move(script.getSliceRef(2, 20));
      break;
   }

   case(TXOUT_SCRIPT_P2WSH) :
   {
      outputRef.type_ = SCRIPT_PREFIX_P2WSH;
      outputRef.scriptRef_ = move(script.getSliceRef(2, 32));
      break;
   }

   case(TXOUT_SCRIPT_STDPUBKEY65) :
   {
      outputRef.type_ = p2pkh_prefix;
      outputRef.scriptCopy_ = move(getHash160(script.getSliceRef(1, 65)));
      outputRef.scriptRef_.setRef(outputRef.scriptCopy_);
      break;
   }

   case(TXOUT_SCRIPT_STDPUBKEY33) :
   {
      outputRef.type_ = p2pkh_prefix;
      outputRef.scriptCopy_ = move(getHash160(script.getSliceRef(1, 33)));
      outputRef.scriptRef_.setRef(outputRef.scriptCopy_);
      break;
   }

   case(TXOUT_SCRIPT_P2SH) :
   {
      outputRef.type_ = p2sh_prefix;
      outputRef.scriptRef_ = move(script.getSliceRef(2, 20));
      break;
   }

   case(TXOUT_SCRIPT_NONSTANDARD) :
   {
      outputRef.type_ = SCRIPT_PREFIX_NONSTD;
      outputRef.scriptCopy_ = move(getHash160(script));
      outputRef.scriptRef_.setRef(outputRef.scriptCopy_);
      break;
   }

   case(TXOUT_SCRIPT_MULTISIG) :
   {
      outputRef.type_ = SCRIPT_PREFIX_MULTISIG;
      outputRef.scriptCopy_ = move(getMultisigUniqueKey(script));
      outputRef.scriptRef_.setRef(outputRef.scriptCopy_);
      break;
   }

   case(TXOUT_SCRIPT_OPRETURN) :
   {
      outputRef.type_ = SCRIPT_PREFIX_OPRETURN;
      auto size = script.getSize();
      size_t pos = 1;
      if (size > 77)
         pos += 2;
      if (size > 1)
         ++pos;

      outputRef.scriptRef_ = script.getSliceRef(pos, size - pos);
      break;
   }

   default:
      LOGERR << "What kind of TxOutScript did we get?";
   }

   return outputRef;
}

////////////////////////////////////////////////////////////////////////////////
string BtcUtils::base64_encode(const string& in)
{
   size_t main_count = in.size() / 3;
   string result;
   result.reserve(main_count * 4 + 5);

   auto ptr = (const uint8_t*)in.c_str();
   for (unsigned i = 0; i < main_count; i++)
   {
      uint32_t bits24 = ptr[i * 3] << 24 | ptr[i*3+1] << 16 | ptr[i*3+2] << 8;
      for (unsigned y = 0; y < 4; y++)
      {
         unsigned val = (bits24 & 0xFC000000) >> 26;
         result.append(1, base64Chars_.c_str()[val]);
         bits24 <<= 6;
      }
   }

   //padding
   size_t left_over = in.size() - main_count * 3;
   if (left_over == 0)
      return result;

   uint32_t bits24;
   if (left_over == 1)
      bits24 = ptr[main_count * 3] << 24;
   else
      bits24 = ptr[main_count * 3] << 24 | ptr[main_count * 3 + 1] << 16;

   for (unsigned i = 0; i <= left_over; i++)
   {
      unsigned val = (bits24 & 0xFC000000) >> 26;
      result.append(1, base64Chars_.c_str()[val]);
      bits24 <<= 6;
   }

   result.append(3 - left_over, '=');
   
   return result;
}

////////////////////////////////////////////////////////////////////////////////
string BtcUtils::base64_decode(const string& in)
{
   size_t count = in.size() / 4;
   string result;
   result.resize(count * 3 + 4);
   auto ptr = in.c_str();
   auto result_ptr = (uint8_t*)result.c_str();

   size_t len = 0;
   for (unsigned i = 0; i < count; i++)
   {
      uint32_t val = 0;
      for (unsigned y = 0; y < 4; y++)
      {
         auto val8 = ptr[i * 4 + y];
         auto iter = base64Vals_.find(val8);
         if (iter == base64Vals_.end())
         {
            if (val8 == '=')
               break;
            throw runtime_error("invalid b64 character");
         }

         uint32_t bits = iter->second << (26 - (6 * y));
         val |= bits;
         ++len;
      }

      result_ptr[i * 3] = (val & 0xFF000000) >> 24;
      result_ptr[i * 3 + 1] = (val & 0x00FF0000) >> 16;
      result_ptr[i * 3 + 2] = (val & 0x0000FF00) >> 8;
   }

   result_ptr[len] = 0;
   result.resize(len+1);
   return result;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::scrAddrToSegWitAddress(const BinaryData& scrAddr)
{
   //hardcoded for version 0 witness programs for now
   string header;

   if (NetworkConfig::getPubkeyHashPrefix() == SCRIPT_PREFIX_HASH160)
      header = move(string(SEGWIT_ADDRESS_MAINNET_HEADER));
   else if (NetworkConfig::getPubkeyHashPrefix() == SCRIPT_PREFIX_HASH160_TESTNET &&
      NetworkConfig::getPubkeyHashPrefix() == SCRIPT_PREFIX_HASH160_TESTNET)
      header = move(string(SEGWIT_ADDRESS_TESTNET_HEADER));
   else
      throw runtime_error("invalid network for segwit address");

   auto&& result = segwit_addr::encode(
      header, 0, scrAddr.getDataVector());

   if (result.size() == 0)
      throw runtime_error("failed to encode to sw address!");

   return BinaryData(result);
}

////////////////////////////////////////////////////////////////////////////////
BinaryData BtcUtils::segWitAddressToScrAddr(const BinaryData& swAddr)
{
   string header;

   if (NetworkConfig::getPubkeyHashPrefix() == SCRIPT_PREFIX_HASH160)
      header = move(string(SEGWIT_ADDRESS_MAINNET_HEADER));
   else if (NetworkConfig::getPubkeyHashPrefix() == SCRIPT_PREFIX_HASH160_TESTNET)
      header = move(string(SEGWIT_ADDRESS_TESTNET_HEADER));
   else
      throw runtime_error("invalid network for segwit address");

   string swAddr_str(swAddr.getCharPtr(), swAddr.getSize());
   auto&& result = segwit_addr::decode(header, swAddr_str);

   if (result.first == -1)
      throw runtime_error("failed to decode sw address!");

   if (result.second.size() == 0)
      throw runtime_error("empty sw program buffer");

   if (result.first != 0)
      throw runtime_error("only supporting sw version 0 for now");

   BinaryData scrAddr(&result.second[0], result.second.size());
   return scrAddr;
}

////////////////////////////////////////////////////////////////////////////////
int BtcUtils::get_varint_len(const int64_t& value)
{
   if (value < 0xFD)
      return 1;
   else if (value <= 0xFFFF)
      return 3;
   else if (value <= 0xFFFFFFFF)
      return 5;

   return 9;
}

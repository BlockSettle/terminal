////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "BIP32_Serialization.h"
#include "BlockDataManagerConfig.h"

////////////////////////////////////////////////////////////////////////////////
BinaryData BIP32_Serialization::computeFingerprint(const SecureBinaryData& key)
{
   auto compute_fingerprint = [](const SecureBinaryData& key)->BinaryData
   {
      return BtcUtils::hash160(key).getSliceCopy(0, 4);
   };

   bool ispriv = false;

   if (key.getSize() == 32)
   {
      ispriv = true;
   }
   else if (key.getSize() == 33)
   {
      if (key.getPtr()[0] == 0)
         ispriv = true;
   }

   if (ispriv)
   {
      BinaryDataRef privkey = key.getRef();
      if (privkey.getSize() == 33)
         privkey = key.getSliceRef(1, 32);

      auto&& pubkey = CryptoECDSA().ComputePublicKey(privkey);
      auto&& compressed_pub = CryptoECDSA().CompressPoint(pubkey);

      return compute_fingerprint(compressed_pub);
   }
   else
   {
      if (key.getSize() != 33)
      {
         auto&& compressed_pub = CryptoECDSA().CompressPoint(key);
         return compute_fingerprint(compressed_pub);
      }
      else
      {
         return compute_fingerprint(key);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
void BIP32_Serialization::encode()
{
   bool ispriv = false;

   if (key_.getSize() == 32)
   {
      ispriv = true;
   }
   else if (key_.getSize() == 33)
   {
      if (key_.getPtr()[0] == 0)
         ispriv = true;
   }

   if (chaincode_.getSize() != 32)
      throw runtime_error("invalid chaincode for BIP32 ser");

   fingerprint_ = move(computeFingerprint(key_));

   BinaryWriter bw;

   if (BlockDataManagerConfig::getPubkeyHashPrefix() ==
      SCRIPT_PREFIX_HASH160)
   {
      if (ispriv)
         bw.put_uint32_t(BIP32_SER_VERSION_MAIN_PRV, BE);
      else
         bw.put_uint32_t(BIP32_SER_VERSION_MAIN_PUB, BE);
   }
   else if (BlockDataManagerConfig::getPubkeyHashPrefix() ==
      SCRIPT_PREFIX_HASH160_TESTNET)
   {
      if (ispriv)
         bw.put_uint32_t(BIP32_SER_VERSION_TEST_PRV, BE);
      else
         bw.put_uint32_t(BIP32_SER_VERSION_TEST_PUB, BE);
   }
   else
   {
      throw runtime_error("invalid network");
   }

   bw.put_uint8_t(depth_);
   bw.put_BinaryData(fingerprint_);
   bw.put_uint32_t(leaf_id_, BE);

   bw.put_BinaryData(chaincode_);

   if (key_.getSize() == 32 && ispriv)
      bw.put_uint8_t(0);
   else
      throw runtime_error("invalid key for BIP32 ser");
   bw.put_BinaryData(key_);

   auto&& hash = BtcUtils::getSha256(bw.getData()).getSliceCopy(0, 4);
   bw.put_BinaryData(hash);

   auto&& b58_data = BtcUtils::base58_encode(bw.getData());
   b58_string_ = b58_data.toBinStr();
}

////////////////////////////////////////////////////////////////////////////////
void BIP32_Serialization::decode()
{
   //b58 decode 
   auto&& bin_data = BtcUtils::base58_decode(b58_string_);
   if (bin_data.getSize() != 82)
      throw runtime_error("invalid bip32 serialized string");

   //checksum
   BinaryRefReader brr(bin_data);
   auto bdr_val = brr.get_BinaryDataRef(bin_data.getSize() - 4);
   auto bdr_chksum = brr.get_BinaryDataRef(4);

   auto&& hash = BtcUtils::getSha256(bdr_val);
   if (hash.getSliceRef(0, 4) != bdr_chksum)
      throw runtime_error("bip32 checksum failure");

   ////
   BinaryRefReader brr_val(bdr_val);

   //version
   auto verbytes = brr_val.get_uint32_t(BE);

   bool ispriv = false;
   switch (verbytes)
   {
   case BIP32_SER_VERSION_MAIN_PRV:
      ispriv = true;

   case BIP32_SER_VERSION_MAIN_PUB:
   {
      if (BlockDataManagerConfig::getPubkeyHashPrefix() !=
         SCRIPT_PREFIX_HASH160)
         throw runtime_error("bip32 string is for wrong network");
      break;
   }

   case BIP32_SER_VERSION_TEST_PRV:
      ispriv = true;

   case BIP32_SER_VERSION_TEST_PUB:
   {
      if (BlockDataManagerConfig::getPubkeyHashPrefix() !=
         SCRIPT_PREFIX_HASH160_TESTNET)
         throw runtime_error("bip32 string is for wrong network");
      break;
   }

   default:
      throw runtime_error("unexpected bip32 string version");
   }

   depth_ = brr_val.get_uint8_t();
   fingerprint_ = brr_val.get_BinaryData(4);
   leaf_id_ = brr_val.get_uint32_t(BE);

   chaincode_ = brr_val.get_BinaryData(32);
   key_ = brr_val.get_BinaryData(33);

   if (ispriv && key_.getPtr()[0] != 0)
      throw runtime_error("bip32 string invalid key type");

   auto&& fingerprint = computeFingerprint(key_);
   if (fingerprint != fingerprint_)
      throw runtime_error("bip32 string fingerprint mismatch");
}

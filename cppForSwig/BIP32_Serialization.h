////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _BIP32_SERIALIZATION_H
#define _BIP32_SERIALIZATION_H

#include "BtcUtils.h"
#include "EncryptionUtils.h"

class BIP32_Serialization
{
private:
   unsigned version_;
   uint8_t depth_;
   BinaryData fingerprint_;
   unsigned leaf_id_;

   SecureBinaryData chaincode_;
   SecureBinaryData key_;

   string b58_string_;

private:
   void encode(void);
   void decode(void);

public:
   BIP32_Serialization(const string& b58_string) :
      b58_string_(b58_string)
   {
      decode();
   }

   BIP32_Serialization(
      uint8_t depth, unsigned leaf_id,
      const SecureBinaryData& chaincode,
      const SecureBinaryData& key) :
      depth_(depth), leaf_id_(leaf_id),
      chaincode_(chaincode)
   {
      encode();
   }

   static BinaryData computeFingerprint(const SecureBinaryData& key);

   //gets
   const string& getBase58(void) const { return b58_string_; }
   unsigned getVersion(void) const { return version_; }
   uint8_t getDepth(void) const { return depth_; }
   const BinaryData& getFingerPrint(void) const { return fingerprint_; }
   unsigned getLeafID(void) const { return leaf_id_; }

   const SecureBinaryData& getChaincode(void) const { return chaincode_; }
   const SecureBinaryData& getKey(void) const { return key_; }
};

#endif
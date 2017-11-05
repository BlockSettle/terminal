////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_DERIVATION_SCHEME
#define _H_DERIVATION_SCHEME

#include <vector>
#include <set>
#include <memory>

#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "Assets.h"

class DecryptedDataContainer;

#define DERIVATIONSCHEME_LEGACY     0xA0
#define DERIVATIONSCHEME_BIP32      0xA1
#define DERIVATIONSCHEME_MULTISIG   0xA2

#define DERIVATIONSCHEME_KEY  0x00000004

#define DERIVATION_LOOKUP        100


class DerivationSchemeDeserException : public runtime_error
{
public:
   DerivationSchemeDeserException(const string& msg) : runtime_error(msg)
   {}
};

////////////////////////////////////////////////////////////////////////////////
struct DerivationScheme
{
public:
   //tors
   virtual ~DerivationScheme(void) = 0;

   //virtual
   virtual vector<shared_ptr<AssetEntry>> extendPublicChain(
      shared_ptr<AssetEntry>, unsigned) = 0;
   virtual vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned) = 0;
   virtual BinaryData serialize(void) const = 0;

   virtual const SecureBinaryData& getChaincode(void) const = 0;

   //static
   static shared_ptr<DerivationScheme> deserialize(BinaryDataRef);
};

////////////////////////////////////////////////////////////////////////////////
struct DerivationScheme_ArmoryLegacy : public DerivationScheme
{
   friend class AssetWallet_Single;

private:
   SecureBinaryData chainCode_;

public:
   //tors
   DerivationScheme_ArmoryLegacy(SecureBinaryData& chainCode) :
      chainCode_(move(chainCode))
   {}

   //locals
   shared_ptr<AssetEntry_Single> computeNextPrivateEntry(
      shared_ptr<DecryptedDataContainer>,
      const SecureBinaryData& privKey, unique_ptr<Cypher>,
      const BinaryData& full_id, unsigned index);
   
   shared_ptr<AssetEntry_Single> computeNextPublicEntry(
      const SecureBinaryData& pubKey,
      const BinaryData& full_id, unsigned index);

   //virtuals
   vector<shared_ptr<AssetEntry>> extendPublicChain(
      shared_ptr<AssetEntry>, unsigned);
   vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned);

   BinaryData serialize(void) const;

   const SecureBinaryData& getChaincode(void) const { return chainCode_; }
};

class AssetWallet;
class AssetWallet_Single;

#endif
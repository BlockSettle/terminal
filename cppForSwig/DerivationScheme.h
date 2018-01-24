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

#define DERIVATIONSCHEME_KEY  0x00000004

#define DERIVATION_LOOKUP        100

enum DerivationSchemeType
{
   DerSchemeType_ArmoryLegacy,
   DerSchemeType_BIP32
};


class DerivationSchemeException : public runtime_error
{
public:
   DerivationSchemeException(const string& msg) : runtime_error(msg)
   {}
};

////////////////////////////////////////////////////////////////////////////////
struct DerivationScheme
{
   /*in extend methods, the end argument is inclusive for all schemes*/

private:
   const DerivationSchemeType type_;

public:
   //tors
   DerivationScheme(DerivationSchemeType type) :
      type_(type)
   {}

   virtual ~DerivationScheme(void) = 0;

   //local
   DerivationSchemeType getType(void) const { return type_; }

   //virtual
   virtual vector<shared_ptr<AssetEntry>> extendPublicChain(
      shared_ptr<AssetEntry>, unsigned start, unsigned end) = 0;
   virtual vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned start, unsigned end) = 0;
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
      DerivationScheme(DerSchemeType_ArmoryLegacy),
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
      shared_ptr<AssetEntry>, unsigned start, unsigned end);
   vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned start, unsigned end);

   BinaryData serialize(void) const;

   const SecureBinaryData& getChaincode(void) const { return chainCode_; }
};

////////////////////////////////////////////////////////////////////////////////
struct DerivationScheme_BIP32 : public DerivationScheme
{
   friend class AssetWallet_Single;

private:
   SecureBinaryData chainCode_;

public:
   //tors
   DerivationScheme_BIP32(SecureBinaryData& chainCode) :
      DerivationScheme(DerSchemeType_BIP32),
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
      shared_ptr<AssetEntry>, unsigned start, unsigned end);
   vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned start, unsigned end);

   BinaryData serialize(void) const;

   const SecureBinaryData& getChaincode(void) const { return chainCode_; }
};

#endif
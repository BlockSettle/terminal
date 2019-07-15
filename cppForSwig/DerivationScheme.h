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
#include "lmdbpp.h"

class DecryptedDataContainer;

#define DERIVATIONSCHEME_LEGACY        0xA0
#define DERIVATIONSCHEME_BIP32         0xA1
#define DERIVATIONSCHEME_BIP32_SALTED  0xA2
#define DERIVATIONSCHEME_BIP32_ECDH    0xA3

#define DERIVATIONSCHEME_KEY  0x00000004

#define DERIVATION_LOOKUP        100

enum DerivationSchemeType
{
   DerSchemeType_ArmoryLegacy,
   DerSchemeType_BIP32,
   DerSchemeType_ECDH
};


class DerivationSchemeException : public std::runtime_error
{
public:
   DerivationSchemeException(const std::string& msg) : std::runtime_error(msg)
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
   virtual std::vector<std::shared_ptr<AssetEntry>> extendPublicChain(
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end) = 0;
   virtual std::vector<std::shared_ptr<AssetEntry>> extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end) = 0;
   virtual BinaryData serialize(void) const = 0;

   virtual const SecureBinaryData& getChaincode(void) const = 0;

   //static
   static std::shared_ptr<DerivationScheme> deserialize(
      BinaryDataRef, LMDB*);
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
      chainCode_(std::move(chainCode))
   {}

   //locals
   std::shared_ptr<AssetEntry_Single> computeNextPrivateEntry(
      std::shared_ptr<DecryptedDataContainer>,
      const SecureBinaryData& privKey, std::unique_ptr<Cipher>,
      const BinaryData& full_id, unsigned index);
   
   std::shared_ptr<AssetEntry_Single> computeNextPublicEntry(
      const SecureBinaryData& pubKey,
      const BinaryData& full_id, unsigned index);

   //virtuals
   std::vector<std::shared_ptr<AssetEntry>> extendPublicChain(
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end);
   std::vector<std::shared_ptr<AssetEntry>> extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end);

   BinaryData serialize(void) const;

   const SecureBinaryData& getChaincode(void) const { return chainCode_; }
};

////////////////////////////////////////////////////////////////////////////////
struct DerivationScheme_BIP32 : public DerivationScheme
{
   friend class AssetWallet_Single;

private:
   SecureBinaryData chainCode_;
   const unsigned depth_;
   const unsigned leafId_;

public:
   //tors
   DerivationScheme_BIP32(SecureBinaryData& chainCode,
      unsigned depth, unsigned leafId) :
      DerivationScheme(DerSchemeType_BIP32),
      chainCode_(std::move(chainCode)),
      depth_(depth), leafId_(leafId)
   {}

   //locals
   virtual std::shared_ptr<AssetEntry_Single> computeNextPrivateEntry(
      std::shared_ptr<DecryptedDataContainer>,
      const SecureBinaryData& privKey, std::unique_ptr<Cipher>,
      const BinaryData& full_id, unsigned index);

   virtual std::shared_ptr<AssetEntry_Single> computeNextPublicEntry(
      const SecureBinaryData& pubKey,
      const BinaryData& full_id, unsigned index);

   //virtuals
   std::vector<std::shared_ptr<AssetEntry>> extendPublicChain(
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end);
   std::vector<std::shared_ptr<AssetEntry>> extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end);

   virtual BinaryData serialize(void) const;

   const SecureBinaryData& getChaincode(void) const 
   { return chainCode_; }

   unsigned getDepth(void) const { return depth_; }
   unsigned getLeafId(void) const { return leafId_; }
};

////////////////////////////////////////////////////////////////////////////////
class DerivationScheme_BIP32_Salted : public DerivationScheme_BIP32
{
private:
   const SecureBinaryData salt_;

public:
   DerivationScheme_BIP32_Salted(
      SecureBinaryData& salt,
      SecureBinaryData& chainCode,
      unsigned depth, unsigned leafId) :
   DerivationScheme_BIP32(chainCode, depth, leafId),
      salt_(std::move(salt))
   {}

   //virtuals
   std::shared_ptr<AssetEntry_Single> computeNextPrivateEntry(
      std::shared_ptr<DecryptedDataContainer>,
      const SecureBinaryData& privKey, std::unique_ptr<Cipher>,
      const BinaryData& full_id, unsigned index) override;

   std::shared_ptr<AssetEntry_Single> computeNextPublicEntry(
      const SecureBinaryData& pubKey,
      const BinaryData& full_id, unsigned index) override;

   BinaryData serialize(void) const override;
};

////////////////////////////////////////////////////////////////////////////////
class DerivationScheme_ECDH : public DerivationScheme
{
private:
   const BinaryData id_;
   std::map<SecureBinaryData, unsigned> saltMap_;
   unsigned topSaltIndex_ = 0;
   std::mutex saltMutex_;

private:
   std::shared_ptr<AssetEntry_Single> computeNextPublicEntry(
      const SecureBinaryData& pubKey,
      const BinaryData& full_id, unsigned index);

   std::shared_ptr<AssetEntry_Single> computeNextPrivateEntry(
      std::shared_ptr<DecryptedDataContainer>,
      const SecureBinaryData& privKey, std::unique_ptr<Cipher>,
      const BinaryData& full_id, unsigned index);

   void putSalt(unsigned, const SecureBinaryData&, LMDB*);

public:
   DerivationScheme_ECDH(void) :
      DerivationScheme(DerSchemeType_ECDH),
      id_(CryptoPRNG::generateRandom(8))
   {}

   DerivationScheme_ECDH(const BinaryData& id,
      std::map<SecureBinaryData, unsigned> saltMap);

   //virtuals
   std::vector<std::shared_ptr<AssetEntry>> extendPublicChain(
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end) override;
   std::vector<std::shared_ptr<AssetEntry>> extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>, unsigned start, unsigned end) override;
   BinaryData serialize(void) const override;

   const SecureBinaryData& getChaincode(void) const override;

   //locals
   unsigned addSalt(const SecureBinaryData&, LMDB*);
   void putAllSalts(LMDB*);
   unsigned getSaltIndex(const SecureBinaryData&);
};

#endif
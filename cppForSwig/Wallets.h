////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _WALLETS_H
#define _WALLETS_H

#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <set>
#include <map>
#include <string>

#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "lmdb/lmdbpp.h"
#include "Script.h"
#include "Signer.h"
#include "ReentrantLock.h"

using namespace std;

#define PUBKEY_UNCOMPRESSED_BYTE 0x80
#define PUBKEY_COMPRESSED_BYTE   0x81
#define PRIVKEY_BYTE             0x82
#define ENCRYPTIONKEY_BYTE       0x83


#define WALLETTYPE_KEY        0x00000001
#define PARENTID_KEY          0x00000002
#define WALLETID_KEY          0x00000003
#define DERIVATIONSCHEME_KEY  0x00000004
#define ADDRESSENTRYTYPE_KEY  0x00000005
#define TOPUSEDINDEX_KEY      0x00000006
#define ROOTASSET_KEY         0x00000007

#define MASTERID_KEY          0x000000A0
#define MAINWALLET_KEY        0x000000A1

#define WALLETMETA_PREFIX     0xB0
#define ASSETENTRY_PREFIX     0xAA

#define DERIVATIONSCHEME_LEGACY     0xA0
#define DERIVATIONSCHEME_BIP32      0xA1
#define DERIVATIONSCHEME_MULTISIG   0xA2
#define DERIVATION_LOOKUP           100

#define ENCRYPTIONKEY_PREFIX        0xC0
#define ENCRYPTIONKEY_PREFIX_TEMP   0xCC

#define KDF_PREFIX                  0xC1
#define KDF_ROMIX_PREFIX            0xC100

#define CYPHER_BYTE                 0xB2


#define WALLETMETA_DBNAME "WalletHeader"
#define HMAC_KEY_ENCRYPTIONKEYS "EncyrptionKey"
#define HMAC_KEY_PRIVATEKEYS "PrivateKey"

#define VERSION_MAJOR      2
#define VERSION_MINOR      0
#define VERSION_REVISION   0
 
class WalletException : public runtime_error
{
public:
   WalletException(const string& msg) : runtime_error(msg)
   {}
};

class NoEntryInWalletException
{};

class AssetUnavailableException
{};

class AssetDeserException : public runtime_error
{
public:
   AssetDeserException(const string& msg) : runtime_error(msg)
   {}
};

class DerivationSchemeDeserException : public runtime_error
{
public:
   DerivationSchemeDeserException(const string& msg) : runtime_error(msg)
   {}
};

class CypherException : public runtime_error
{
public:
   CypherException(const string& msg) : runtime_error(msg)
   {}
};

class DecryptedDataContainerException : public runtime_error
{
public:
   DecryptedDataContainerException(const string& msg) : runtime_error(msg)
   {}
};

class EncryptedDataMissing : public runtime_error
{
public:
   EncryptedDataMissing() : runtime_error("")
   {}
};


////////////////////////////////////////////////////////////////////////////////
enum WalletMetaType
{
   WalletMetaType_Single,
   WalletMetaType_Multisig,
   WalletMetaType_Subwallet
};

////
struct WalletMeta
{
   shared_ptr<LMDBEnv> dbEnv_;
   WalletMetaType type_;
   BinaryData parentID_;
   BinaryData walletID_;
   string dbName_;

   uint8_t versionMajor_ = 0;
   uint16_t versionMinor_ = 0;
   uint16_t revision_ = 0;

   SecureBinaryData defaultEncryptionKey_;
   SecureBinaryData defaultEncryptionKeyId_;

   //tors
   WalletMeta(shared_ptr<LMDBEnv> env, WalletMetaType type) :
      dbEnv_(env), type_(type)
   {
      versionMajor_ = VERSION_MAJOR;
      versionMinor_ = VERSION_MINOR;
      revision_ = VERSION_REVISION;
   }

   virtual ~WalletMeta(void) = 0;
   
   //local
   BinaryData getDbKey(void);
   const BinaryData& getWalletID(void) const { return walletID_; }
   string getWalletIDStr(void) const
   {
      if (walletID_.getSize() == 0)
         throw WalletException("empty wallet id");

      string idStr(walletID_.getCharPtr(), walletID_.getSize());
      return idStr;
   }

   BinaryData serializeVersion(void) const;
   void unseralizeVersion(BinaryRefReader&);

   BinaryData serializeEncryptionKey(void) const;
   void unserializeEncryptionKey(BinaryRefReader&);
   
   const SecureBinaryData& getDefaultEncryptionKey(void) const 
   { return defaultEncryptionKey_; }
   const BinaryData& getDefaultEncryptionKeyId(void) const
   { return defaultEncryptionKeyId_; }

   //virtual
   virtual BinaryData serialize(void) const = 0;
   virtual bool shouldLoad(void) const = 0;

   //static
   static shared_ptr<WalletMeta> deserialize(shared_ptr<LMDBEnv> env,
      BinaryDataRef key, BinaryDataRef val);
};

////
struct WalletMeta_Single : public WalletMeta
{
   //tors
   WalletMeta_Single(shared_ptr<LMDBEnv> dbEnv) :
      WalletMeta(dbEnv, WalletMetaType_Single)
   {}

   //virtual
   BinaryData serialize(void) const;
   bool shouldLoad(void) const;
};

////
struct WalletMeta_Multisig : public WalletMeta
{
   //tors
   WalletMeta_Multisig(shared_ptr<LMDBEnv> dbEnv) :
      WalletMeta(dbEnv, WalletMetaType_Multisig)
   {}

   //virtual
   BinaryData serialize(void) const;
   bool shouldLoad(void) const;
};

////
struct WalletMeta_Subwallet : public WalletMeta
{
   //tors
   WalletMeta_Subwallet(shared_ptr<LMDBEnv> dbEnv) :
      WalletMeta(dbEnv, WalletMetaType_Subwallet)
   {}

   //virtual
   BinaryData serialize(void) const;
   bool shouldLoad(void) const;
};


////////////////////////////////////////////////////////////////////////////////
enum AssetType
{
   AssetType_EncryptedData,
   AssetType_PublicKey,
   AssetType_PrivateKey,
   AssetType_MetaData
};

////
enum AssetEntryType
{
   AssetEntryType_Single = 0x01,
   AssetEntryType_Multisig
};

////
enum AddressEntryType
{
   AddressEntryType_Default=0,
   AddressEntryType_P2PKH,
   AddressEntryType_P2WPKH,
   AddressEntryType_P2WSH,
   AddressEntryType_Nested_P2WPKH,
   AddressEntryType_Nested_P2WSH,
   AddressEntryType_Nested_P2PK,
   AddressEntryType_Nested_Multisig
};

enum CypherType
{
   CypherType_AES,
   CypherType_Serpent
};

enum ScriptHashType
{
   ScriptHash_P2PKH_Uncompressed,
   ScriptHash_P2PKH_Compressed,
   ScriptHash_P2WPKH,
   ScriptHash_Nested_P2PK
};

////////////////////////////////////////////////////////////////////////////////
struct KeyDerivationFunction
{
private:

public:
   KeyDerivationFunction(void) 
   {}

   virtual ~KeyDerivationFunction(void) = 0;
   virtual SecureBinaryData deriveKey(
      const SecureBinaryData& rawKey) const = 0;
   virtual bool isSame(KeyDerivationFunction* const) const = 0;

   bool operator<(const KeyDerivationFunction& rhs)
   {
      return getId() < rhs.getId();
   }

   virtual const BinaryData& getId(void) const = 0;
   virtual BinaryData serialize(void) const = 0;
   static shared_ptr<KeyDerivationFunction>
      deserialize(const BinaryDataRef&);
};

////
struct KeyDerivationFunction_Romix : public KeyDerivationFunction
{
private:
   mutable BinaryData id_;
   unsigned iterations_;
   unsigned memTarget_;
   const BinaryData salt_;

private:
   BinaryData computeID(void) const;
   BinaryData initialize(void);

public:
   KeyDerivationFunction_Romix() :
      KeyDerivationFunction(),
      salt_(move(initialize()))
   {}

   KeyDerivationFunction_Romix(unsigned iterations, unsigned memTarget,
      SecureBinaryData& salt) :
      KeyDerivationFunction(),
      iterations_(iterations), memTarget_(memTarget), salt_(salt)
   {}

   SecureBinaryData deriveKey(const SecureBinaryData& rawKey) const;
   bool isSame(KeyDerivationFunction* const) const;
   BinaryData serialize(void) const;
   const BinaryData& getId(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct DecryptedEncryptionKey;

class Cypher
{
private:
   const CypherType type_;

protected:
   const BinaryData kdfId_;
   const BinaryData encryptionKeyId_;
   mutable SecureBinaryData iv_;

public:

   //tors
   Cypher(CypherType type, const BinaryData& kdfId, 
      const BinaryData encryptionKeyId) :
      type_(type), kdfId_(kdfId), encryptionKeyId_(encryptionKeyId)
   {}

   virtual ~Cypher(void) = 0;

   //locals
   CypherType getType(void) const { return type_; }
   const BinaryData& getKdfId(void) const { return kdfId_; }
   const BinaryData& getEncryptionKeyId(void) const { return encryptionKeyId_; }
   const SecureBinaryData& getIV(void) const { return iv_; }

   //virtuals
   virtual BinaryData serialize(void) const = 0;
   virtual unique_ptr<Cypher> getCopy(void) const = 0;
   virtual unique_ptr<Cypher> getCopy(const BinaryData& keyId) const = 0;

   virtual SecureBinaryData encrypt(const SecureBinaryData& key, 
      const SecureBinaryData& data) const = 0;
   virtual SecureBinaryData encrypt(DecryptedEncryptionKey* const key,
      const BinaryData& kdfId, const SecureBinaryData& data) const = 0;

   virtual SecureBinaryData decrypt(const SecureBinaryData& key, 
      const SecureBinaryData& data) const = 0;

   virtual bool isSame(Cypher* const) const = 0;

   //statics
   static unique_ptr<Cypher> deserialize(BinaryRefReader& brr);
};

////
class Cypher_AES : public Cypher
{
public:
   //tors
   Cypher_AES(const BinaryData& kdfId, const BinaryData& encryptionKeyId) :
      Cypher(CypherType_AES, kdfId, encryptionKeyId)
   {
      //init IV
      iv_ = move(SecureBinaryData().GenerateRandom(BTC_AES::BLOCKSIZE));
   }

   Cypher_AES(const BinaryData& kdfId, const BinaryData& encryptionKeyId, 
      SecureBinaryData& iv) :
      Cypher(CypherType_AES, kdfId, encryptionKeyId)
   {
      if (iv.getSize() != BTC_AES::BLOCKSIZE)
         throw CypherException("invalid iv length");

      iv_ = move(iv);
   }

   //virtuals
   BinaryData serialize(void) const;
   unique_ptr<Cypher> getCopy(void) const;
   unique_ptr<Cypher> getCopy(const BinaryData& keyId) const;

   SecureBinaryData encrypt(const SecureBinaryData& key,
      const SecureBinaryData& data) const;
   SecureBinaryData encrypt(DecryptedEncryptionKey* const key,
      const BinaryData& kdfId, const SecureBinaryData& data) const;


   SecureBinaryData decrypt(const SecureBinaryData& key, 
      const SecureBinaryData& data) const;

   bool isSame(Cypher* const) const;
};

////////////////////////////////////////////////////////////////////////////////
struct Asset
{
   const AssetType type_;

   Asset(AssetType type) : 
      type_(type)
   {}

   /*TODO: create a mlocked binarywriter class*/

   virtual ~Asset(void) = 0;
   virtual BinaryData serialize(void) const = 0;
};

////////////////////////////////////////////////////////////////////////////////
struct Asset_PublicKey : public Asset
{
public:
   SecureBinaryData uncompressed_;
   SecureBinaryData compressed_;

public:
   Asset_PublicKey(SecureBinaryData& pubkey) :
      Asset(AssetType_PublicKey)
   {
      switch (pubkey.getSize())
      {
      case 33:
      {
         uncompressed_ = move(CryptoECDSA().UncompressPoint(pubkey));
         compressed_ = move(pubkey);
         break;
      }

      case 65:
      {
         compressed_ = move(CryptoECDSA().CompressPoint(pubkey));
         uncompressed_ = move(pubkey);
         break;
      }

      default:
         throw WalletException("cannot compress/decompress pubkey of that size");
      }
   }

   Asset_PublicKey(SecureBinaryData& uncompressedKey, 
      SecureBinaryData& compressedKey) :
      Asset(AssetType_PublicKey), 
      uncompressed_(move(uncompressedKey)),
      compressed_(move(compressedKey))
   {
      if (uncompressed_.getSize() != 65 ||
         compressed_.getSize() != 33)
         throw WalletException("invalid pubkey size");
   }

   const SecureBinaryData& getUncompressedKey(void) const { return uncompressed_; }
   const SecureBinaryData& getCompressedKey(void) const { return compressed_; }

   BinaryData serialize(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct Asset_EncryptedData : public Asset
{
   friend class DecryptedDataContainer;

protected:
   const SecureBinaryData data_;
   unique_ptr<Cypher> cypher_;

public:
   Asset_EncryptedData(SecureBinaryData& data, unique_ptr<Cypher> cypher)
      : Asset(AssetType_EncryptedData), data_(move(data))
   {
      if (data_.getSize() == 0)
         return;

      if (cypher == nullptr)
         throw WalletException("null cypher for privkey");

      cypher_ = move(cypher);
   }

   //virtual
   virtual ~Asset_EncryptedData(void) = 0;
   virtual bool isSame(Asset_EncryptedData* const) const = 0;
   
   //local
   bool hasData(void) const 
   { return (data_.getSize() != 0); }
   
   unique_ptr<Cypher> copyCypher(void) const
   {
      if (cypher_ == nullptr)
         return nullptr;

      return cypher_->getCopy();
   }

   const SecureBinaryData& getIV(void) const
   {
      return cypher_->getIV();
   }

   const SecureBinaryData& getEncryptedData(void) const
   {
      return data_;
   }

   const BinaryData& getEncryptionKeyID(void) const
   {
      return cypher_->getEncryptionKeyId();
   }

   //static
   static shared_ptr<Asset_EncryptedData> deserialize(const BinaryDataRef&);
   static shared_ptr<Asset_EncryptedData> deserialize(
      size_t len, const BinaryDataRef&);
};

////////////////////////////////////////////////////////////////////////////////
struct DecryptedEncryptionKey
{
   friend class DecryptedDataContainer;
   friend class Cypher_AES;
   friend class AssetWallet_Single;

private:
   const SecureBinaryData rawKey_;
   map<BinaryData, SecureBinaryData> derivedKeys_;

private:
   BinaryData computeId(const SecureBinaryData& key) const;
   const SecureBinaryData& getData(void) const { return rawKey_; }
   const SecureBinaryData& getDerivedKey(const BinaryData& id) const;

public:
   DecryptedEncryptionKey(SecureBinaryData& key) :
      rawKey_(move(key))
   {}
   
   void deriveKey(shared_ptr<KeyDerivationFunction> kdf);
   BinaryData getId(const BinaryData& kdfid) const;

   unique_ptr<DecryptedEncryptionKey> copy(void) const;
   bool hasData(void) const { return rawKey_.getSize() != 0; }
};

////////////////////////////////////////////////////////////////////////////////
struct DecryptedPrivateKey
{
private:
   const unsigned id_;
   const SecureBinaryData privateKey_;

private:
   const SecureBinaryData& getData(void) const { return privateKey_; }

public:
   DecryptedPrivateKey(unsigned id, SecureBinaryData& key) :
      id_(id), privateKey_(move(key))
   {}

   bool hasData(void) const { return privateKey_.getSize() != 0; }
   const unsigned& getId(void) const { return id_; }
   const SecureBinaryData& getDataRef(void) const { return privateKey_; }
};

////////////////////////////////////////////////////////////////////////////////
struct Asset_EncryptionKey : public Asset_EncryptedData
{
public:
   const BinaryData id_;

private:
   unique_ptr<DecryptedEncryptionKey> decrypt(
      const SecureBinaryData& key) const;

public:
   Asset_EncryptionKey(BinaryData& id, SecureBinaryData& data, 
      unique_ptr<Cypher> cypher) :
      Asset_EncryptedData(data, move(cypher)), id_(move(id))
   {}

   BinaryData serialize(void) const;
   const BinaryData& getId(void) const { return id_; }

   bool isSame(Asset_EncryptedData* const) const;
};

////////////////////////////////////////////////////////////////////////////////
struct Asset_PrivateKey : public Asset_EncryptedData
{
   friend class DecryptedDataContainer;

public:
   const int id_;

private:
   unique_ptr<DecryptedPrivateKey> decrypt(
      const SecureBinaryData& key) const;

public:
   Asset_PrivateKey(int id, 
      SecureBinaryData& data, unique_ptr<Cypher> cypher) :
      Asset_EncryptedData(data, move(cypher)), id_(id)
   {}
   
   BinaryData serialize(void) const;
   unsigned getId(void) const { return id_; }

   bool isSame(Asset_EncryptedData* const) const;
};

////////////////////////////////////////////////////////////////////////////////
class AssetEntry
{
protected:
   const int index_;
   AssetEntryType type_;
   AddressEntryType addressType_ = AddressEntryType_Default;

   bool needsCommit_ = true;

public:
   //tors
   AssetEntry(AssetEntryType type, int id) :
      type_(type), index_(id)
   {}

   virtual ~AssetEntry(void) = 0;

   //local
   int getId(void) const { return index_; }
   const AssetEntryType getType(void) const { return type_; }
   const AddressEntryType getAddrType(void) const { return addressType_; }
   bool setAddressEntryType(AddressEntryType type);
   bool needsCommit(void) const { return needsCommit_; }
   void doNotCommit(void) { needsCommit_ = false; }
   BinaryData getDbKey(void) const;

   //virtual
   virtual BinaryData serialize(void) const = 0;
   virtual AddressEntryType getAddressTypeForHash(BinaryDataRef) const = 0;
   virtual bool hasPrivateKey(void) const = 0;
   virtual const BinaryData& getPrivateEncryptionKeyId(void) const = 0;


   //static
   static shared_ptr<AssetEntry> deserialize(
      BinaryDataRef key, BinaryDataRef value);
   static shared_ptr<AssetEntry> deserDBValue(
      int index, BinaryDataRef value);
};

////
class AssetEntry_Single : public AssetEntry
{
private:
   shared_ptr<Asset_PublicKey> pubkey_;
   shared_ptr<Asset_PrivateKey> privkey_;

   mutable BinaryData h160Uncompressed_;
   mutable BinaryData h160Compressed_;

   mutable BinaryData witnessScript_;
   mutable BinaryData witnessScriptH160_;

   mutable BinaryData p2pkScript_;
   mutable BinaryData p2pkScriptH160_;

public:
   //tors
   AssetEntry_Single(int id,
      SecureBinaryData& pubkey, 
      shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id), privkey_(privkey)
   {
      pubkey_ = make_shared<Asset_PublicKey>(pubkey);
   }

   AssetEntry_Single(int id,
      SecureBinaryData& pubkeyUncompressed,
      SecureBinaryData& pubkeyCompressed, 
      shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id), privkey_(privkey)
   {
      pubkey_ = make_shared<Asset_PublicKey>(
         pubkeyUncompressed, pubkeyCompressed);
   }

   AssetEntry_Single(int id,
      shared_ptr<Asset_PublicKey> pubkey,
      shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id),
      pubkey_(pubkey), privkey_(privkey)
   {}

   //local
   shared_ptr<Asset_PublicKey> getPubKey(void) const { return pubkey_; }
   shared_ptr<Asset_PrivateKey> getPrivKey(void) const { return privkey_; }

   const BinaryData& getHash160Uncompressed(void) const;
   const BinaryData& getHash160Compressed(void) const;

   const BinaryData& getWitnessScript(void) const;
   const BinaryData& getWitnessScriptH160(void) const;

   const BinaryData& getP2PKScript(void) const;
   const BinaryData& getP2PKScriptH160(void) const;

   map<ScriptHashType, BinaryDataRef> getScriptHashMap(void) const;

   //virtual
   BinaryData serialize(void) const;
   AddressEntryType getAddressTypeForHash(BinaryDataRef) const;
   bool hasPrivateKey(void) const;
   const BinaryData& getPrivateEncryptionKeyId(void) const;
};

////
class AssetEntry_Multisig : public AssetEntry
{
   friend class ResolvedFeed_AssetWalletMS;

private:
   //map<AssetWalletID, AssetEntryPtr>
   //ordering by wallet ids guarantees the ms script hash can be 
   //reconstructed deterministically
   const map<BinaryData, shared_ptr<AssetEntry>> assetMap_;
   
   const unsigned m_;
   const unsigned n_;

   mutable BinaryData multisigScript_;
   mutable BinaryData h160_;
   mutable BinaryData h256_;

   mutable BinaryData p2wshScript_;
   mutable BinaryData p2wshScriptH160_;

public:
   //tors
   AssetEntry_Multisig(int id,
      const map<BinaryData, shared_ptr<AssetEntry>>& assetMap,
      unsigned m, unsigned n) :
      AssetEntry(AssetEntryType_Multisig, id), 
      assetMap_(assetMap), m_(m), n_(n)
   {
      if (assetMap.size() != n)
         throw WalletException("asset count mismatch in multisig entry");
   }
   
   //local
   const BinaryData& getScript(void) const;
   const BinaryData& getHash160(void) const;
   const BinaryData& getHash256(void) const;

   const BinaryData& getP2WSHScript(void) const;
   const BinaryData& getP2WSHScriptH160(void) const;

   unsigned getM(void) const { return m_; }
   unsigned getN(void) const { return n_; }

   //virtual
   BinaryData serialize(void) const 
   { 
      throw AssetDeserException("no serialization for MS assets"); 
   }

   AddressEntryType getAddressTypeForHash(BinaryDataRef) const;
   bool hasPrivateKey(void) const;
   const BinaryData& getPrivateEncryptionKeyId(void) const;
};

class DecryptedDataContainer;

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
      shared_ptr<AssetEntry>, unsigned) = 0;
   virtual BinaryData serialize(void) const = 0;

   //static
   static shared_ptr<DerivationScheme> deserialize(BinaryDataRef);
};

////
struct DerivationScheme_ArmoryLegacy : public DerivationScheme
{
   friend class AssetWallet_Single;

private:
   SecureBinaryData chainCode_;
   shared_ptr<DecryptedDataContainer> decryptedDataContainer_;

private:
   void setDecryptedDataContainerPtr(shared_ptr<DecryptedDataContainer> ddc)
   {
      decryptedDataContainer_ = ddc;
   }

public:
   //tors
   DerivationScheme_ArmoryLegacy(SecureBinaryData& chainCode,
      shared_ptr<DecryptedDataContainer> ddc) :
      chainCode_(move(chainCode)), decryptedDataContainer_(ddc)
   {}

   //virtuals
   vector<shared_ptr<AssetEntry>> extendPublicChain(
      shared_ptr<AssetEntry>, unsigned);
   vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<AssetEntry>, unsigned);

   BinaryData serialize(void) const;

   const SecureBinaryData& getChainCode(void) const { return chainCode_; }
};

class AssetWallet;
class AssetWallet_Single;

////
struct DerivationScheme_Multisig : public DerivationScheme
{
private:
   const unsigned n_;
   const unsigned m_;

   set<BinaryData> walletIDs_;
   map<BinaryData, shared_ptr<AssetWallet_Single>> wallets_;


public:
   //tors
   DerivationScheme_Multisig(
      const map<BinaryData, shared_ptr<AssetWallet_Single>>& wallets,
      unsigned n, unsigned m) :
      n_(n), m_(m), wallets_(wallets)
   {
      for (auto& wallet : wallets)
         walletIDs_.insert(wallet.first);
   }

   DerivationScheme_Multisig(const set<BinaryData>& walletIDs,
      unsigned n, unsigned m) :
      n_(n), m_(m), walletIDs_(walletIDs)
   {}

   //local
   shared_ptr<AssetEntry_Multisig> getAssetForIndex(unsigned) const;
   unsigned getN(void) const { return n_; }
   const set<BinaryData>& getWalletIDs(void) const { return walletIDs_; }
   void setSubwalletPointers(
      map<BinaryData, shared_ptr<AssetWallet_Single>> ptrMap);

   shared_ptr<AssetWallet_Single> getSubWalletPtr(const BinaryData& id) const;

   //virtual
   BinaryData serialize(void) const;
   vector<shared_ptr<AssetEntry>> extendPublicChain(
      shared_ptr<AssetEntry>, unsigned);
   vector<shared_ptr<AssetEntry>> extendPrivateChain(
      shared_ptr<AssetEntry>, unsigned);
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry
{
protected:
   const AddressEntryType type_;
   const shared_ptr<AssetEntry> asset_;

   mutable BinaryData address_;
   mutable BinaryData hash_;

public:
   //tors
   AddressEntry(AddressEntryType aetype, shared_ptr<AssetEntry> asset) :
      asset_(asset), type_(aetype)
   {
      asset_->setAddressEntryType(aetype);
   }

   virtual ~AddressEntry(void) = 0;

   //local
   AddressEntryType getType(void) const { return type_; }
   int getIndex(void) const { return asset_->getId(); }

   //virtual
   virtual const BinaryData& getAddress() const = 0;
   virtual shared_ptr<ScriptRecipient> getRecipient(uint64_t) const = 0;
   virtual const BinaryData& getPrefixedHash(void) const = 0;

   //accounts for txhash + id as well as input script size
   virtual size_t getInputSize(void) const = 0;
   
   //throw by default, SW types will overload
   virtual size_t getWitnessDataSize(void) const 
   { throw runtime_error("no witness data"); }
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_P2PKH : public AddressEntry
{
public:
   //tors
   AddressEntry_P2PKH(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_P2PKH, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;
   
   //size
   size_t getInputSize(void) const { return 180; }
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_P2WPKH : public AddressEntry
{
public:
   //tors
   AddressEntry_P2WPKH(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_P2WPKH, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;
   
   //size
   size_t getInputSize(void) const { return 41; }
   size_t getWitnessDataSize(void) const { return 108; }
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_Nested_Multisig : public AddressEntry
{
public:
   //tors
   AddressEntry_Nested_Multisig(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_Nested_Multisig, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;

   //size
   size_t getInputSize(void) const;
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_P2WSH : public AddressEntry
{
public:
   //tors
   AddressEntry_P2WSH(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_P2WSH, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;

   //size
   size_t getInputSize(void) const { return 41; }
   size_t getWitnessDataSize(void) const;
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_Nested_P2WPKH : public AddressEntry
{
public:
   //tors
   AddressEntry_Nested_P2WPKH(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_Nested_P2WPKH, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;

   //size
   size_t getInputSize(void) const { return 63; }
   size_t getWitnessDataSize(void) const { return 108; }
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_Nested_P2WSH : public AddressEntry
{
public:
   //tors
   AddressEntry_Nested_P2WSH(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_Nested_P2WSH, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;

   //size
   size_t getInputSize(void) const { return 75; }
   size_t getWitnessDataSize(void) const;
};

////////////////////////////////////////////////////////////////////////////////
class AddressEntry_Nested_P2PK : public AddressEntry
{
public:
   //tors
   AddressEntry_Nested_P2PK(shared_ptr<AssetEntry> asset) :
      AddressEntry(AddressEntryType_Nested_P2PK, asset)
   {}

   //virtual
   const BinaryData& getAddress() const;
   const BinaryData& getPrefixedHash() const;
   shared_ptr<ScriptRecipient> getRecipient(uint64_t) const;

   //size
   size_t getInputSize(void) const { return 147; }
};

////////////////////////////////////////////////////////////////////////////////
struct HashMaps
{
   map<BinaryDataRef, int> hashCompressed_;
   map<BinaryDataRef, int> hashUncompressed_;
   map<BinaryDataRef, int> hashP2WSH_;
   map<BinaryDataRef, int> hashNestedP2WPKH_;
   map<BinaryDataRef, int> hashNestedP2WSH_;
   map<BinaryDataRef, int> hashNestedP2PK_;
   map<BinaryDataRef, int> hashNestedMultisig_;

   void clear(void)
   {
      hashCompressed_.clear();
      hashUncompressed_.clear();
      hashP2WSH_.clear();
      hashNestedP2WPKH_.clear();
      hashNestedP2WSH_.clear();
      hashNestedP2PK_.clear();
      hashNestedMultisig_.clear();
   }
};

////////////////////////////////////////////////////////////////////////////////
class DecryptedDataContainer : public Lockable
{
   struct DecryptedData
   {
      map<BinaryData, unique_ptr<DecryptedEncryptionKey>> encryptionKeys_;
      map<unsigned, unique_ptr<DecryptedPrivateKey>> privateKeys_;
   };

private:
   map<BinaryData, shared_ptr<KeyDerivationFunction>> kdfMap_;
   unique_ptr<DecryptedData> lockedDecryptedData_ = nullptr;

   struct OtherLockedContainer
   {
      shared_ptr<DecryptedDataContainer> container_;
      shared_ptr<ReentrantLock> lock_;

      OtherLockedContainer(shared_ptr<DecryptedDataContainer> obj)
      {
         if (obj == nullptr)
            throw runtime_error("emtpy DecryptedDataContainer ptr");

         lock_ = make_unique<ReentrantLock>(obj.get());
      }
   };

   vector<OtherLockedContainer> otherLocks_;
   
   LMDBEnv* dbEnv_;
   LMDB* dbPtr_;

   /*
   The default encryption key is used to encrypt the master encryption in 
   case no passphrase was provided at wallet creation. This is to prevent
   for the master key being written in plain text on disk. It is encryption 
   but does not effectively result in the wallet being protected by encryption,
   since the default encryption is written on disk in plain text.

   This is mostly to allow for the entire container to be encrypted head to toe
   without implementing large caveats to handle unencrypted use cases.
   */
   const SecureBinaryData defaultEncryptionKey_;
   const SecureBinaryData defaultEncryptionKeyId_;

protected:
   map<BinaryData, shared_ptr<Asset_EncryptedData>> encryptionKeyMap_;

private:
   function<SecureBinaryData(
      const BinaryData&)> getPassphraseLambda_;
   
private:
   unique_ptr<DecryptedEncryptionKey> deriveEncryptionKey(
      unique_ptr<DecryptedEncryptionKey>, const BinaryData& kdfid) const;

   unique_ptr<DecryptedEncryptionKey> promptPassphrase(
      const BinaryData&, const BinaryData&) const;

   void initAfterLock(void);
   void cleanUpBeforeUnlock(void);

public:
   DecryptedDataContainer(LMDBEnv* dbEnv, LMDB* dbPtr,
      const SecureBinaryData& defaultEncryptionKey,
      const BinaryData& defaultEncryptionKeyId) :
         dbEnv_(dbEnv), dbPtr_(dbPtr),
         defaultEncryptionKey_(defaultEncryptionKey),
         defaultEncryptionKeyId_(defaultEncryptionKeyId)
   {
      auto emptyPassphraseLambda = [](const BinaryData&)->
         SecureBinaryData
      {
         return SecureBinaryData();
      };

      getPassphraseLambda_ = emptyPassphraseLambda;
   }

   const SecureBinaryData& getDecryptedPrivateKey(
      shared_ptr<Asset_PrivateKey> data);
   SecureBinaryData encryptData(
      Cypher* const cypher, const SecureBinaryData& data);


   void populateEncryptionKey(
      const BinaryData& keyid, const BinaryData& kdfid);

   void addKdf(shared_ptr<KeyDerivationFunction> kdfPtr)
   {
      kdfMap_.insert(make_pair(kdfPtr->getId(), kdfPtr));
   }

   void addEncryptionKey(shared_ptr<Asset_EncryptionKey> keyPtr)
   {
      encryptionKeyMap_.insert(make_pair(keyPtr->getId(), keyPtr));
   }

   void updateOnDisk(void);
   void readFromDisk(void);

   void updateKeyOnDiskNoPrefix(
      const BinaryData&, shared_ptr<Asset_EncryptedData>);
   void updateKeyOnDisk(
      const BinaryData&, shared_ptr<Asset_EncryptedData>);

   void deleteKeyFromDisk(const BinaryData& key);

   void setPassphrasePromptLambda(
      function<SecureBinaryData(const BinaryData&)> lambda)
   {
      getPassphraseLambda_ = lambda;
   }

   void encryptEncryptionKey(const BinaryData&, const SecureBinaryData&);
   void lockOther(shared_ptr<DecryptedDataContainer> other);
};

////////////////////////////////////////////////////////////////////////////////
class AssetWallet : protected Lockable
{
   friend class ResolvedFeed_AssetWalletSingle;
   friend class ResolvedFeed_AssetWalletMS;

private:
   virtual void initAfterLock(void) {}
   virtual void cleanUpBeforeUnlock(void) {}

protected:
   shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;
   const string dbName_;

   atomic<int> highestUsedAddressIndex_;

   shared_ptr<AssetEntry> root_;
   map<int, shared_ptr<AssetEntry>> assets_;
   map<int, shared_ptr<AddressEntry>> addresses_;
   HashMaps hashMaps_;
   mutable int lastKnownIndex_ = -1;
   mutable int lastAssetMapSize_ = 0;

   shared_ptr<DecryptedDataContainer> decryptedData_;
   shared_ptr<DerivationScheme> derScheme_;
   AddressEntryType default_aet_;

   ////
   BinaryData parentID_;
   BinaryData walletID_;
   
protected:
   //tors
   AssetWallet(shared_ptr<WalletMeta> metaPtr) :
      dbEnv_(metaPtr->dbEnv_), dbName_(metaPtr->dbName_)
   {
      db_ = new LMDB(dbEnv_.get(), dbName_);
      decryptedData_ = make_shared<DecryptedDataContainer>(
         dbEnv_.get(), db_,
         metaPtr->getDefaultEncryptionKey(),
         metaPtr->getDefaultEncryptionKeyId());
   }

   static shared_ptr<LMDBEnv> getEnvFromFile(
      const string& path, unsigned dbCount = 3)
   {
      auto env = make_shared<LMDBEnv>(dbCount);
      env->open(path);

      return env;
   }

   //local
   void writeAssetEntry(shared_ptr<AssetEntry>);
   void deleteAssetEntry(shared_ptr<AssetEntry>);
   BinaryDataRef getDataRefForKey(const BinaryData& key) const;

   void putData(const BinaryData& key, const BinaryData& data);
   void putData(BinaryWriter& key, BinaryWriter& data);

   void extendPrivateChain(shared_ptr<AssetEntry>, unsigned);
   unsigned getAndBumpHighestUsedIndex(void);

   //virtual
   virtual void putHeaderData(const BinaryData& parentID,
      const BinaryData& walletID,
      shared_ptr<DerivationScheme>,
      AddressEntryType,
      int topUsedIndex);

   virtual shared_ptr<AddressEntry> getAddressEntryForAsset(
      shared_ptr<AssetEntry>, 
      AddressEntryType) = 0;

   virtual void fillHashIndexMap(void) = 0;

   //static
   static BinaryDataRef getDataRefForKey(const BinaryData& key, LMDB* db);
   static unsigned getDbCountAndNames(
      shared_ptr<LMDBEnv> dbEnv, 
      map<BinaryData, shared_ptr<WalletMeta>>&, 
      BinaryData& masterID, 
      BinaryData& mainWalletID);
   static void putDbName(LMDB* db, shared_ptr<WalletMeta>);
   static void setMainWallet(LMDB* db, shared_ptr<WalletMeta>);
   static void putData(LMDB* db, const BinaryData& key, const BinaryData& data);
   static void initWalletMetaDB(shared_ptr<LMDBEnv>, const string&);

public:
   //tors
   virtual ~AssetWallet() = 0;

   //local
   shared_ptr<AddressEntry> getNewAddress();
   string getID(void) const; 
   virtual ReentrantLock lockDecryptedContainer(void);
   bool isDecryptedContainerLocked(void) const;
   
   shared_ptr<AssetEntry> getAssetForIndex(unsigned) const;
   const BinaryData& getNestedSWAddrForIndex(unsigned chainIndex);
   const BinaryData& getNestedP2PKAddrForIndex(unsigned chainIndex);
   const BinaryData& getP2PKHAddrForIndex(unsigned chainIndex);
   size_t getAssetCount(void) const { return assets_.size(); }
   int getLastComputedIndex(void) const;
   
   void extendPublicChain(unsigned);
   void extendPublicChain(shared_ptr<AssetEntry>, unsigned);
   bool extendPublicChainToIndex(unsigned);
   
   void extendPrivateChain(unsigned);
   void extendPrivateChainToIndex(unsigned);

   bool hasScrAddr(const BinaryData& scrAddr);
   int getAssetIndexForAddr(const BinaryData& scrAddr);
   AddressEntryType getAddrTypeForIndex(int index);
   shared_ptr<AddressEntry> getAddressEntryForIndex(int);
   AddressEntryType getDefaultAddressType(void) const { return default_aet_; }
   void updateOnDiskAssets(void);
   void deleteImports(const vector<BinaryData>&);
   const string& getFilename(void) const;

   void setPassphrasePromptLambda(
      function<SecureBinaryData(const BinaryData&)> lambda)
   {
      decryptedData_->setPassphrasePromptLambda(lambda);
   }

   void changeMasterPassphrase(const SecureBinaryData&);

   //virtual
   virtual set<BinaryData> getAddrHashSet() = 0;
   virtual bool setImport(int importID, const SecureBinaryData& pubkey) = 0;
   shared_ptr<AssetEntry> getLastAssetWithPrivateKey(void) const;

   const BinaryData& getP2SHScriptForHash(const BinaryData&);

   virtual const SecureBinaryData& getDecryptedValue(
      shared_ptr<Asset_PrivateKey>) = 0;

   //static
   static shared_ptr<AssetWallet> loadMainWalletFromFile(const string& path);
   static int convertToImportIndex(int importID);
   static int convertFromImportIndex(int importID);
};

////
class AssetWallet_Single : public AssetWallet
{
   friend class AssetWallet;
   friend class AssetWallet_Multisig;

protected:
   //locals
   void readFromFile(void);

   //virtual
   void putHeaderData(const BinaryData& parentID,
      const BinaryData& walletID,
      shared_ptr<DerivationScheme>,
      AddressEntryType,
      int topUsedIndex);

   shared_ptr<AddressEntry> getAddressEntryForAsset(
      shared_ptr<AssetEntry>,
      AddressEntryType);

   void fillHashIndexMap();

   //static
   static shared_ptr<AssetWallet_Single> initWalletDb(
      shared_ptr<WalletMeta>,
      shared_ptr<KeyDerivationFunction> masterKdf,
      DecryptedEncryptionKey& masterEncryptionKey,
      unique_ptr<Cypher>,
      const SecureBinaryData& passphrase,
      AddressEntryType, 
      const SecureBinaryData& privateRoot,
      unsigned lookup);

   static shared_ptr<AssetWallet_Single> initWalletDbFromPubRoot(
      shared_ptr<WalletMeta>,
      AddressEntryType,
      SecureBinaryData& pubRoot,
      SecureBinaryData& chainCode,
      unsigned lookup);

   static BinaryData computeWalletID(
      shared_ptr<DerivationScheme>,
      shared_ptr<AssetEntry>);

public:
   //tors
   AssetWallet_Single(shared_ptr<WalletMeta> metaPtr) :
      AssetWallet(metaPtr)
   {}

   //virtual
   set<BinaryData> getAddrHashSet();
   bool setImport(int importID, const SecureBinaryData& pubkey);
   const SecureBinaryData& getDecryptedValue(
      shared_ptr<Asset_PrivateKey>);

   //static
   static shared_ptr<AssetWallet_Single> createFromPrivateRoot_Armory135(
      const string& folder,
      AddressEntryType,
      const SecureBinaryData& privateRoot,
      const SecureBinaryData& passphrase,
      unsigned lookup);

   static shared_ptr<AssetWallet_Single> createFromPublicRoot_Armory135(
      const string& folder,
      AddressEntryType,
      SecureBinaryData& privateRoot,
      SecureBinaryData& chainCode,
      unsigned lookup);

   //local
   const SecureBinaryData& getPublicRoot(void) const;
   const SecureBinaryData& getChainCode(void) const;
};

////
class AssetWallet_Multisig : public AssetWallet
{
   friend class AssetWallet;

private:
   atomic<unsigned> chainLength_;

protected:
   //local
   void readFromFile(void);

   //virtual
   void fillHashIndexMap(void);
   const SecureBinaryData& getDecryptedValue(
      shared_ptr<Asset_PrivateKey>);

public:
   //tors
   AssetWallet_Multisig(shared_ptr<WalletMeta> metaPtr) :
      AssetWallet(metaPtr)
   {}

   //virtual
   set<BinaryData> getAddrHashSet();
   bool setImport(int importID, const SecureBinaryData& pubkey);
   virtual ReentrantLock lockDecryptedContainer(void);

   shared_ptr<AddressEntry> getAddressEntryForAsset(
      shared_ptr<AssetEntry>,
      AddressEntryType);

   //static
   static shared_ptr<AssetWallet_Multisig> createFromPrivateRoot(
      const string& folder,
      AddressEntryType aet,
      unsigned M, unsigned N,
      SecureBinaryData& privateRoot,
      const SecureBinaryData& passphrase,
      unsigned lookup = UINT32_MAX
      );

   static shared_ptr<AssetWallet> createFromWallets(
      vector<shared_ptr<AssetWallet>> wallets,
      unsigned M,
      unsigned lookup = UINT32_MAX);

   //local

   //for unit tests
   BinaryData getPrefixedHashForIndex(unsigned) const;
};

////////////////////////////////////////////////////////////////////////////////
class ResolvedFeed_AssetWalletSingle : public ResolverFeed
{
private:
   shared_ptr<AssetWallet> wltPtr_;

protected:
   map<BinaryDataRef, BinaryDataRef> hash_to_preimage_;
   map<BinaryDataRef, shared_ptr<AssetEntry_Single>> pubkey_to_asset_;

public:
   //tors
   ResolvedFeed_AssetWalletSingle(shared_ptr<AssetWallet_Single> wltPtr) :
      wltPtr_(wltPtr)
   {
      for (auto& entry : wltPtr->assets_)
      {
         auto assetSingle = 
            dynamic_pointer_cast<AssetEntry_Single>(entry.second);
         if (assetSingle == nullptr)
            throw WalletException("unexpected asset entry type in single wallet");

         //pubkeys
         auto h160UncompressedRef = BinaryDataRef(assetSingle->getHash160Uncompressed());
         auto h160CompressedRef = BinaryDataRef(assetSingle->getHash160Compressed());
         auto pubkeyUncompressedRef = 
            BinaryDataRef(assetSingle->getPubKey()->getUncompressedKey());
         auto pubkeyCompressedRef =
            BinaryDataRef(assetSingle->getPubKey()->getCompressedKey());

         hash_to_preimage_.insert(make_pair(h160UncompressedRef, pubkeyUncompressedRef));
         hash_to_preimage_.insert(make_pair(h160CompressedRef, pubkeyCompressedRef));

         //p2wpkh
         auto witnessScript = BinaryDataRef(assetSingle->getWitnessScript());
         auto&& witnessScriptH160 = BinaryDataRef(assetSingle->getWitnessScriptH160());

         hash_to_preimage_.insert(make_pair(witnessScriptH160, witnessScript));

         //nested p2pk
         auto p2pkScript = BinaryDataRef(assetSingle->getP2PKScript());
         auto p2pkScriptH160 = BinaryDataRef(assetSingle->getP2PKScriptH160());

         hash_to_preimage_.insert(make_pair(p2pkScriptH160, p2pkScript));
         
         //pub key to asset
         pubkey_to_asset_.insert(make_pair(pubkeyUncompressedRef, assetSingle));
         pubkey_to_asset_.insert(make_pair(pubkeyCompressedRef, assetSingle));
      }
   }

   //virtual
   BinaryData getByVal(const BinaryData& key)
   {
      auto keyRef = BinaryDataRef(key);
      auto iter = hash_to_preimage_.find(keyRef);
      if (iter == hash_to_preimage_.end())
         throw runtime_error("invalid value");

      return iter->second;
   }

   virtual const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey)
   {
      auto pubkeyref = BinaryDataRef(pubkey);
      auto iter = pubkey_to_asset_.find(pubkeyref);
      if (iter == pubkey_to_asset_.end())
         throw runtime_error("invalid value");

      const auto& privkeyAsset = iter->second->getPrivKey();
      return wltPtr_->getDecryptedValue(privkeyAsset);
   }
};

////////////////////////////////////////////////////////////////////////////////
class ResolvedFeed_AssetWalletMS : public ResolverFeed
{
private:
   shared_ptr<AssetWallet> wltPtr_;

   map<BinaryDataRef, BinaryDataRef> hash_to_preimage_;
   map<BinaryDataRef, shared_ptr<AssetEntry_Single>> pubkey_to_asset_;

public:
   //tors
   ResolvedFeed_AssetWalletMS(shared_ptr<AssetWallet_Multisig> wltPtr) :
      wltPtr_(wltPtr)
   {
      for (auto& entry : wltPtr->assets_)
      {
         auto assetMS =
            dynamic_pointer_cast<AssetEntry_Multisig>(entry.second);
         if (assetMS == nullptr)
            throw WalletException("unexpected asset entry type in ms wallet");

         auto script = assetMS->getScript().getRef();
         hash_to_preimage_.insert(make_pair(
            assetMS->getHash160().getRef(), script));
         hash_to_preimage_.insert(make_pair(
            assetMS->getHash256().getRef(), script));

         auto nested_p2wshScript = assetMS->getP2WSHScript().getRef();
         hash_to_preimage_.insert(make_pair(
            assetMS->getP2WSHScriptH160().getRef(), nested_p2wshScript));

         for (auto assetptr : assetMS->assetMap_)
         {
            auto assetSingle = 
               dynamic_pointer_cast<AssetEntry_Single>(assetptr.second);
            if (assetSingle == nullptr)
               throw WalletException("unexpected asset entry type in ms wallet");

            auto pubkeyCpr = 
               assetSingle->getPubKey()->getCompressedKey().getRef();

            pubkey_to_asset_.insert(make_pair(pubkeyCpr, assetSingle));
         }
      }
   }

   //virtual
   BinaryData getByVal(const BinaryData& key)
   {
      auto keyRef = BinaryDataRef(key);
      auto iter = hash_to_preimage_.find(keyRef);
      if (iter == hash_to_preimage_.end())
         throw runtime_error("invalid value");

      return iter->second;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey)
   {
      auto pubkeyref = BinaryDataRef(pubkey);
      auto iter = pubkey_to_asset_.find(pubkeyref);
      if (iter == pubkey_to_asset_.end())
         throw runtime_error("invalid value");

      const auto& privkeyAsset = iter->second->getPrivKey();
      return wltPtr_->getDecryptedValue(privkeyAsset);
   }
};

#endif

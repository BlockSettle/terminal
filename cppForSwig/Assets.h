////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017-2019, goatpig                                          //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_ASSETS
#define _H_ASSETS

#include <set>
#include <string>

#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "AssetEncryption.h"

class AssetException : public std::runtime_error
{
public:
   AssetException(const std::string& err) : std::runtime_error(err)
   {}
};

#define HMAC_KEY_ENCRYPTIONKEYS "EncyrptionKey"
#define HMAC_KEY_PRIVATEKEYS "PrivateKey"

#define ASSETENTRY_PREFIX        0x8A
#define PUBKEY_UNCOMPRESSED_BYTE 0x80
#define PUBKEY_COMPRESSED_BYTE   0x81
#define PRIVKEY_BYTE             0x82
#define ENCRYPTIONKEY_BYTE       0x83

#define METADATA_COMMENTS_PREFIX 0x90
#define METADATA_AUTHPEER_PREFIX 0x91
#define METADATA_PEERROOT_PREFIX 0x92
#define METADATA_ROOTSIG_PREFIX  0x93

#define ROOT_ASSETENTRY_ID       0xFFFFFFFF


////////////////////////////////////////////////////////////////////////////////
enum AssetType
{
   AssetType_EncryptedData,
   AssetType_PublicKey,
   AssetType_PrivateKey,
};

enum MetaType
{
   MetaType_Comment,
   MetaType_AuthorizedPeer,
   MetaType_PeerRootKey,
   MetaType_PeerRootSig
};

////
enum AssetEntryType
{
   AssetEntryType_Single = 0x01,
   AssetEntryType_Multisig
};

enum ScriptHashType
{
   ScriptHash_P2PKH_Uncompressed,
   ScriptHash_P2PKH_Compressed,
   ScriptHash_P2WPKH,
   ScriptHash_Nested_P2PK
};

////////////////////////////////////////////////////////////////////////////////
struct DecryptedEncryptionKey
{
   friend class DecryptedDataContainer;
   friend class Cypher_AES;
   friend class AssetWallet_Single;

private:
   const SecureBinaryData rawKey_;
   std::map<BinaryData, SecureBinaryData> derivedKeys_;

private:
   BinaryData computeId(const SecureBinaryData& key) const;
   const SecureBinaryData& getData(void) const { return rawKey_; }
   const SecureBinaryData& getDerivedKey(const BinaryData& id) const;

public:
   DecryptedEncryptionKey(SecureBinaryData& key) :
      rawKey_(std::move(key))
   {}

   void deriveKey(std::shared_ptr<KeyDerivationFunction> kdf);
   BinaryData getId(const BinaryData& kdfid) const;

   std::unique_ptr<DecryptedEncryptionKey> copy(void) const;
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
      id_(id), privateKey_(std::move(key))
   {}

   bool hasData(void) const { return privateKey_.getSize() != 0; }
   const unsigned& getId(void) const { return id_; }
   const SecureBinaryData& getDataRef(void) const { return privateKey_; }
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
         uncompressed_ = std::move(CryptoECDSA().UncompressPoint(pubkey));
         compressed_ = std::move(pubkey);
         break;
      }

      case 65:
      {
         compressed_ = std::move(CryptoECDSA().CompressPoint(pubkey));
         uncompressed_ = std::move(pubkey);
         break;
      }

      default:
         throw AssetException("cannot compress/decompress pubkey of that size");
      }
   }

   Asset_PublicKey(SecureBinaryData& uncompressedKey,
      SecureBinaryData& compressedKey) :
      Asset(AssetType_PublicKey),
      uncompressed_(std::move(uncompressedKey)),
      compressed_(std::move(compressedKey))
   {
      if (uncompressed_.getSize() != 65 ||
         compressed_.getSize() != 33)
         throw AssetException("invalid pubkey size");
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
   std::unique_ptr<Cypher> cypher_;

public:
   Asset_EncryptedData(SecureBinaryData& data, std::unique_ptr<Cypher> cypher)
      : Asset(AssetType_EncryptedData), data_(std::move(data))
   {
      if (data_.getSize() == 0)
         return;

      if (cypher == nullptr)
         throw AssetException("null cypher for privkey");

      cypher_ = std::move(cypher);
   }

   //virtual
   virtual ~Asset_EncryptedData(void) = 0;
   virtual bool isSame(Asset_EncryptedData* const) const = 0;

   //local
   bool hasData(void) const
   {
      return (data_.getSize() != 0);
   }

   std::unique_ptr<Cypher> copyCypher(void) const
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
   static std::shared_ptr<Asset_EncryptedData> deserialize(const BinaryDataRef&);
   static std::shared_ptr<Asset_EncryptedData> deserialize(
      size_t len, const BinaryDataRef&);
};

////////////////////////////////////////////////////////////////////////////////
struct Asset_EncryptionKey : public Asset_EncryptedData
{
public:
   const BinaryData id_;

private:
   std::unique_ptr<DecryptedEncryptionKey> decrypt(
      const SecureBinaryData& key) const;

public:
   Asset_EncryptionKey(BinaryData& id, SecureBinaryData& data,
      std::unique_ptr<Cypher> cypher) :
      Asset_EncryptedData(data, std::move(cypher)), id_(std::move(id))
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
   std::unique_ptr<DecryptedPrivateKey> decrypt(
      const SecureBinaryData& key) const;

public:
   Asset_PrivateKey(int id,
      SecureBinaryData& data, std::unique_ptr<Cypher> cypher) :
      Asset_EncryptedData(data, std::move(cypher)), id_(id)
   {}

   BinaryData serialize(void) const;
   unsigned getId(void) const { return id_; }

   bool isSame(Asset_EncryptedData* const) const;
};

////////////////////////////////////////////////////////////////////////////////
class AssetEntry
{
protected:
   AssetEntryType type_;
   const int index_;
   const BinaryData accountID_;
   BinaryData ID_; //accountID | index

   bool needsCommit_ = true;

public:
   //tors
   AssetEntry(AssetEntryType type, int id, const BinaryData& accountID) :
      type_(type), index_(id), accountID_(accountID)
   {
      ID_ = accountID_;
      ID_.append(WRITE_UINT32_BE(id));
   }

   virtual ~AssetEntry(void) = 0;

   //local
   int getIndex(void) const { return index_; }
   const BinaryData& getAccountID(void) const { return accountID_; }
   const BinaryData& getID(void) const { return ID_; }

   const AssetEntryType getType(void) const { return type_; }
   bool needsCommit(void) const { return needsCommit_; }
   void doNotCommit(void) { needsCommit_ = false; }
   BinaryData getDbKey(void) const;

   //virtual
   virtual BinaryData serialize(void) const = 0;
   virtual bool hasPrivateKey(void) const = 0;
   virtual const BinaryData& getPrivateEncryptionKeyId(void) const = 0;

   //static
   static std::shared_ptr<AssetEntry> deserialize(
      BinaryDataRef key, BinaryDataRef value);
   static std::shared_ptr<AssetEntry> deserDBValue(
      int index, const BinaryData& account_id,
      BinaryDataRef value);
};

////////////////////////////////////////////////////////////////////////////////
class AssetEntry_Single : public AssetEntry
{
private:
   std::shared_ptr<Asset_PublicKey> pubkey_;
   std::shared_ptr<Asset_PrivateKey> privkey_;

public:
   //tors
   AssetEntry_Single(int id, const BinaryData& accountID,
      SecureBinaryData& pubkey,
      std::shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id, accountID), 
      privkey_(privkey)
   {
      pubkey_ = std::make_shared<Asset_PublicKey>(pubkey);
   }

   AssetEntry_Single(int id, const BinaryData& accountID,
      SecureBinaryData& pubkeyUncompressed,
      SecureBinaryData& pubkeyCompressed,
      std::shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id, accountID), 
      privkey_(privkey)
   {
      pubkey_ = std::make_shared<Asset_PublicKey>(
         pubkeyUncompressed, pubkeyCompressed);
   }

   AssetEntry_Single(int id, const BinaryData& accountID,
      std::shared_ptr<Asset_PublicKey> pubkey,
      std::shared_ptr<Asset_PrivateKey> privkey) :
      AssetEntry(AssetEntryType_Single, id, accountID),
      pubkey_(pubkey), privkey_(privkey)
   {}

   //local
   std::shared_ptr<Asset_PublicKey> getPubKey(void) const { return pubkey_; }
   std::shared_ptr<Asset_PrivateKey> getPrivKey(void) const { return privkey_; }

   //virtual
   BinaryData serialize(void) const;
   bool hasPrivateKey(void) const;
   const BinaryData& getPrivateEncryptionKeyId(void) const;
};

////////////////////////////////////////////////////////////////////////////////
class AssetEntry_Multisig : public AssetEntry
{
   friend class AddressEntry_Multisig;

private:
   //map<AssetWalletID, AssetEntryPtr>
   //ordering by wallet ids guarantees the ms script hash can be 
   //reconstructed deterministically
   const std::map<BinaryData, std::shared_ptr<AssetEntry>> assetMap_;

   const unsigned m_;
   const unsigned n_;

private:
   const std::map<BinaryData, std::shared_ptr<AssetEntry>> getAssetMap(void) const
   {
      return assetMap_;
   }

public:
   //tors
   AssetEntry_Multisig(int id, const BinaryData& accountID,
      const std::map<BinaryData, std::shared_ptr<AssetEntry>>& assetMap,
      unsigned m, unsigned n) :
      AssetEntry(AssetEntryType_Multisig, id, accountID),
      assetMap_(assetMap), m_(m), n_(n)
   {
      if (assetMap.size() != n)
         throw AssetException("asset count mismatch in multisig entry");
   }

   //local
   unsigned getM(void) const { return m_; }
   unsigned getN(void) const { return n_; }

   //virtual
   BinaryData serialize(void) const
   {
      throw AssetException("no serialization for MS assets");
   }

   bool hasPrivateKey(void) const;
   const BinaryData& getPrivateEncryptionKeyId(void) const;
};

////////////////////////////////////////////////////////////////////////////////
struct MetaData
{
   friend class MetaDataAccount;

private:
   bool needsCommit_ = false;

protected:
   const MetaType type_;
   const BinaryData accountID_;
   const unsigned index_;

public:
   MetaData(MetaType type, const BinaryData& accountID, unsigned index) :
      type_(type), accountID_(accountID), index_(index)
   {}

   //virtuals
   virtual ~MetaData(void) = 0;
   virtual BinaryData serialize(void) const = 0;
   virtual BinaryData getDbKey(void) const = 0;
   virtual void deserializeDBValue(const BinaryDataRef&) = 0;
   virtual void clear(void) = 0;

   //locals
   bool needsCommit(void) { return needsCommit_; }
   void flagForCommit(void) { needsCommit_ = true; }
   MetaType type(void) const { return type_; }

   //static
   static std::shared_ptr<MetaData> deserialize(
      const BinaryDataRef& key, const BinaryDataRef& data);
};

////////////////////////////////////////////////////////////////////////////////
class PeerPublicData : public MetaData
{
private:
   std::set<std::string> names_; //IPs, domain names
   SecureBinaryData publicKey_;

public:
   PeerPublicData(const BinaryData& accountID, unsigned index) :
      MetaData(MetaType_AuthorizedPeer, accountID, index)
   {}

   //virtuals
   BinaryData serialize(void) const;
   BinaryData getDbKey(void) const;
   void deserializeDBValue(const BinaryDataRef&);
   void clear(void);

   //locals
   void addName(const std::string&);
   bool eraseName(const std::string&);
   void setPublicKey(const SecureBinaryData&);

   //
   const std::set<std::string> getNames(void) const { return names_; }
   const SecureBinaryData& getPublicKey(void) const { return publicKey_; }
};

////////////////////////////////////////////////////////////////////////////////
class PeerRootKey : public MetaData
{
   //carries the root key of authorized peers' parent public key
   //used to check signatures of child peer keys, typically a server with a
   //key pair cycling schedule

private:
   SecureBinaryData publicKey_;
   std::string description_;

public:
   PeerRootKey(const BinaryData& accountID, unsigned index) :
      MetaData(MetaType_PeerRootKey, accountID, index)
   {}

   //virtuals
   BinaryData serialize(void) const;
   BinaryData getDbKey(void) const;
   void deserializeDBValue(const BinaryDataRef&);
   void clear(void);

   //locals
   void set(const std::string&, const SecureBinaryData&);
   const SecureBinaryData& getKey(void) const { return publicKey_; }
   const std::string& getDescription(void) const { return description_; }

};

////////////////////////////////////////////////////////////////////////////////
class PeerRootSignature : public MetaData
{
   // carries the peer wallet's key pair signature from a 'parent' wallet
   // typically only one per peer wallet

private:
   SecureBinaryData publicKey_;
   SecureBinaryData signature_;

public:
   PeerRootSignature(const BinaryData& accountID, unsigned index) :
      MetaData(MetaType_PeerRootSig, accountID, index)
   {}

   //virtuals
   BinaryData serialize(void) const;
   BinaryData getDbKey(void) const;
   void deserializeDBValue(const BinaryDataRef&);
   void clear(void);

   //locals
   void set(const SecureBinaryData& key, const SecureBinaryData& sig);
   const SecureBinaryData& getKey(void) const { return publicKey_; }
   const SecureBinaryData& getSig(void) const { return signature_; }

};

#endif

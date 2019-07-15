////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017-2019, goatpig                                          //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_ACCOUNTS
#define _H_ACCOUNTS

#include <set>
#include <map>
#include <memory>

#include "ReentrantLock.h"
#include "BinaryData.h"
#include "lmdbpp.h"
#include "EncryptionUtils.h"
#include "Assets.h"
#include "Addresses.h"
#include "DerivationScheme.h"

#define ARMORY_LEGACY_ACCOUNTID        0xF6E10000
#define IMPORTS_ACCOUNTID              0x00000000
#define ARMORY_LEGACY_ASSET_ACCOUNTID  0x00000001

#define BIP32_LEGACY_OUTER_ACCOUNT_DERIVATIONID 0x00000000
#define BIP32_LEGACY_INNER_ACCOUNT_DERIVATIONID 0x00000001
#define BIP32_SEGWIT_OUTER_ACCOUNT_DERIVATIONID 0x10000000
#define BIP32_SEGWIT_INNER_ACCOUNT_DERIVATIONID 0x10000001

#define ECDH_ASSET_ACCOUTID 0x20000000

#define ADDRESS_ACCOUNT_PREFIX   0xD0
#define ASSET_ACCOUNT_PREFIX     0xE1
#define ASSET_COUNT_PREFIX       0xE2
#define ASSET_TOP_INDEX_PREFIX   0xE3

#define META_ACCOUNT_COMMENTS    0x000000C0
#define META_ACCOUNT_AUTHPEER    0x000000C1
#define META_ACCOUNT_PREFIX      0xF1

class AccountException : public std::runtime_error
{
public:
   AccountException(const std::string& err) : std::runtime_error(err)
   {}
};

enum AssetAccountTypeEnum
{
   AssetAccountTypeEnum_Plain = 0,
   AssetAccountTypeEnum_ECDH
};

enum AccountTypeEnum
{
   /*
   armory derivation scheme 
   outer and inner account are the same
   uncompressed P2PKH, compresed P2SH-P2PK, P2SH-P2WPKH
   */
   AccountTypeEnum_ArmoryLegacy = 0,

   /*
   BIP32 derivation scheme, provided by the user
   outer account: derPath/0
   inner account: derPath/1
   uncompressed and compressed P2PKH
   */
   AccountTypeEnum_BIP32_Legacy,

   /*
   BIP32 derivation scheme, provided by the user
   outer account: derPath/0x10000000
   inner account: derPath/0x10000001
   native P2WPKH, nested P2WPKH
   */
   AccountTypeEnum_BIP32_SegWit,

   /*
   BIP32 derivation scheme, derPath is used as is.
   inner account is outer account.
   no address type is assumed, this has to be provided at creation
   */
   AccountTypeEnum_BIP32_Custom,

   /*
   Derives from BIP32_Custom, ECDH all keys pairs with salt, 
   carried by derScheme object.
   */
   AccountTypeEnum_BIP32_Salted,

   /*
   Stealth address account. Has a single key pair, ECDH it with custom
   salts per asset.
   */
   AccountTypeEnum_ECDH,
   
   AccountTypeEnum_BIP44,
   AccountTypeEnum_Custom
};

enum MetaAccountType
{
   MetaAccount_Unset = 0,
   MetaAccount_Comments,
   MetaAccount_AuthPeers
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct AccountType
{
protected:
   std::set<AddressEntryType> addressTypes_;
   AddressEntryType defaultAddressEntryType_;
   bool isMain_ = false;

public:
   //tors
   AccountType()
   {}

   virtual ~AccountType() = 0;

   //locals
   void setMain(bool ismain) { isMain_ = ismain; }
   const bool isMain(void) const { return isMain_; }

   const std::set<AddressEntryType>& getAddressTypes(void) const 
   { return addressTypes_; }

   AddressEntryType getDefaultAddressEntryType(void) const 
   { return defaultAddressEntryType_; }

   void setAddressTypes(const std::set<AddressEntryType>&);
   void setDefaultAddressType(AddressEntryType);

   //virtuals
   virtual AccountTypeEnum type(void) const = 0;
   virtual BinaryData getAccountID(void) const = 0;
   virtual BinaryData getOuterAccountID(void) const = 0;
   virtual BinaryData getInnerAccountID(void) const = 0;
   virtual bool isWatchingOnly(void) const = 0;
};

////////////////////
struct AccountType_WithRoot : public AccountType
{
protected:
   SecureBinaryData privateRoot_;
   SecureBinaryData publicRoot_;
   mutable SecureBinaryData chainCode_;

protected:
   AccountType_WithRoot()
   {}

public:
   AccountType_WithRoot(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode) : 
      privateRoot_(std::move(privateRoot)),
      publicRoot_(std::move(publicRoot)),
      chainCode_(std::move(chainCode))
   {
      if (privateRoot_.getSize() == 0 && publicRoot_.getSize() == 0)
         throw AccountException("need at least one valid root");

      if (privateRoot_.getSize() > 0 && publicRoot_.getSize() > 0)
         throw AccountException("root types are mutualy exclusive");

      if (publicRoot_.getSize() > 0 && chainCode_.getSize() == 0)
         throw AccountException("need chaincode for public account");
   }

   //virtuals
   virtual ~AccountType_WithRoot(void) = 0;
   virtual const SecureBinaryData& getChaincode(void) const = 0;
   virtual const SecureBinaryData& getPrivateRoot(void) const = 0;
   virtual const SecureBinaryData& getPublicRoot(void) const = 0;
   
   //locals
   bool isWatchingOnly(void) const override
   {
      return privateRoot_.getSize() == 0 &&
         publicRoot_.getSize() > 0 &&
         chainCode_.getSize() > 0;
   }
};

////////////////////
struct AccountType_ArmoryLegacy : public AccountType_WithRoot
{
public:
   AccountType_ArmoryLegacy(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode) :
      AccountType_WithRoot(
      privateRoot, publicRoot, chainCode)
   {
      //uncompressed p2pkh
      addressTypes_.insert(AddressEntryType_P2PKH);

      //nested compressed p2pk
      addressTypes_.insert(AddressEntryType(
         AddressEntryType_P2PK | AddressEntryType_Compressed | AddressEntryType_P2SH));

      //nested p2wpkh
      addressTypes_.insert(AddressEntryType(
         AddressEntryType_P2WPKH | AddressEntryType_P2SH));

      //native p2wpkh
      addressTypes_.insert(AddressEntryType(
         AddressEntryType_P2WPKH));

      //default type
      defaultAddressEntryType_ = AddressEntryType_P2PKH;
   }

   AccountTypeEnum type(void) const
   { return AccountTypeEnum_ArmoryLegacy; }

   const SecureBinaryData& getChaincode(void) const;
   const SecureBinaryData& getPrivateRoot(void) const { return privateRoot_; }
   const SecureBinaryData& getPublicRoot(void) const { return publicRoot_; }
   BinaryData getAccountID(void) const { return WRITE_UINT32_BE(ARMORY_LEGACY_ACCOUNTID); }
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////
struct AccountType_BIP32 : public AccountType_WithRoot
{   
   friend struct AccountType_BIP32_Custom;
private:
   const std::vector<unsigned> derivationPath_;
   unsigned depth_ = 0;
   unsigned leafId_ = 0;
   unsigned fingerPrint_ = 0;

   SecureBinaryData derivedRoot_;
   SecureBinaryData derivedChaincode_;

private:
   void deriveFromRoot(void);

protected:
   AccountType_BIP32(
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId, unsigned fingerPrint) :
      derivationPath_(derivationPath),
      depth_(depth), leafId_(leafId), fingerPrint_(fingerPrint)
   {}


public:
   AccountType_BIP32(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId) :
      AccountType_WithRoot(privateRoot, publicRoot, chainCode),
      derivationPath_(derivationPath),
      depth_(depth), leafId_(leafId)
   {
      deriveFromRoot();
   }

   //bip32 virtuals
   virtual ~AccountType_BIP32(void) = 0;
   virtual std::set<unsigned> getNodes(void) const = 0;

   //AccountType virtuals
   const SecureBinaryData& getChaincode(void) const
   {
      return derivedChaincode_;
   }

   const SecureBinaryData& getPrivateRoot(void) const
   {
      return derivedRoot_;
   }

   const SecureBinaryData& getPublicRoot(void) const
   {
      return derivedRoot_;
   }

   BinaryData getAccountID(void) const;

   //bip32 locals
   unsigned getDepth(void) const { return depth_; }
   unsigned getLeafID(void) const { return leafId_; }
   unsigned getFingerPrint(void) const { return fingerPrint_; }
};


////////////////////
struct AccountType_BIP32_Legacy : public AccountType_BIP32
{
public:
   AccountType_BIP32_Legacy(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId) :
      AccountType_BIP32(
         privateRoot, publicRoot, chainCode, derivationPath,
         depth, leafId)
   {
      //uncompressed p2pkh
      addressTypes_.insert(AddressEntryType_P2PKH);

      //compressed p2pkh
      addressTypes_.insert(
         AddressEntryType(AddressEntryType_Compressed | AddressEntryType_P2PKH));

      defaultAddressEntryType_ =
         AddressEntryType(AddressEntryType_Compressed | AddressEntryType_P2PKH);
   }

   AccountTypeEnum type(void) const 
   { return AccountTypeEnum_BIP32_Legacy; }

   std::set<unsigned> getNodes(void) const;
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////
struct AccountType_BIP32_SegWit : public AccountType_BIP32
{
public:
   AccountType_BIP32_SegWit(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId) :
      AccountType_BIP32(
         privateRoot, publicRoot, chainCode, derivationPath,
         depth, leafId)
   {
      //p2wpkh
      addressTypes_.insert(AddressEntryType_P2WPKH);

      //nested p2wpkh
      addressTypes_.insert(
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH));

      //default
      defaultAddressEntryType_ =
         AddressEntryType(AddressEntryType_P2WPKH);
   }


   AccountTypeEnum type(void) const 
   { return AccountTypeEnum_BIP32_SegWit; }

   virtual std::set<unsigned> getNodes(void) const;
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////
struct AccountType_BIP32_Custom : public AccountType_BIP32
{
   friend class AssetWallet_Single;

private:
   BinaryData outerAccount_;
   BinaryData innerAccount_;

   std::set<unsigned> nodes_;
   unsigned addressLookup_ = UINT32_MAX;

private:
   void setPrivateKey(const SecureBinaryData&);
   void setPublicKey(const SecureBinaryData&);
   void setChaincode(const SecureBinaryData&);

public:
   AccountType_BIP32_Custom(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId) :
         AccountType_BIP32(
         privateRoot, publicRoot, chainCode, derivationPath,
         depth, leafId)
   {}

   AccountType_BIP32_Custom(void) :
      AccountType_BIP32(std::vector<unsigned>(), 0, 0, 0)
   {}

   /***
   Custom BIP32 accounts can come with or without nodes. Without nodes, 
   the derivation path is used as is to create a single underlying 
   AssetAccount. This account id will be UINT32_MAX, and both outer and 
   inner account will be set to this value (effectively, there will be 
   no inner account).

   If nodes are set, as with the other BIP32 account types, the AssetAccounts 
   will be derived from the derivation path + the individual node, and the 
   AssetAccount IDs will be set to the respective node value (in big endian).

   If you set custom nodes, you need to set custom inner and outer account, 
   or the main, inner and outer account values for the AddressAccount will be 
   set to UINT32_MAX at creation. 
   
   Any operation that fetches the main account under the hood will then fail, 
   since UINT32_MAX is a reserved node value that cannot be set by users.
   ***/

   virtual AccountTypeEnum type(void) const 
   { return AccountTypeEnum_BIP32_Custom; }

   std::set<unsigned> getNodes(void) const { return nodes_; }
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
   unsigned getAddressLookup(void) const;

   //set methods
   void setNodes(const std::set<unsigned>& nodes);
   void setOuterAccountID(const BinaryData&);
   void setInnerAccountID(const BinaryData&);
   void setAddressLookup(unsigned count) { addressLookup_ = count; }
};

////////////////////////////////////////////////////////////////////////////////
struct AccountType_BIP32_Salted : public AccountType_BIP32_Custom
{
private:
   const SecureBinaryData salt_;

public:
   AccountType_BIP32_Salted(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath,
      unsigned depth, unsigned leafId,
      const SecureBinaryData& salt) :
      AccountType_BIP32_Custom(
         privateRoot, publicRoot, chainCode, derivationPath,
         depth, leafId),
      salt_(salt)
   {}

   AccountType_BIP32_Salted(const SecureBinaryData& salt) :
      AccountType_BIP32_Custom(), salt_(salt)
   {}

   AccountTypeEnum type(void) const 
   { return AccountTypeEnum_BIP32_Salted; }

   const SecureBinaryData& getSalt(void) const 
   { return salt_; }
};

////////////////////////////////////////////////////////////////////////////////
class AccountType_ECDH : public AccountType
{
private:
   const SecureBinaryData privateKey_;
   const SecureBinaryData publicKey_;

   //ECDH accounts are always single
   const BinaryData accountID_;

public:
   //tor
   AccountType_ECDH(
      const SecureBinaryData& privKey,
      const SecureBinaryData& pubKey) :
      privateKey_(privKey), publicKey_(pubKey),
      accountID_(WRITE_UINT32_BE(ECDH_ASSET_ACCOUTID))
   {
      //run checks
      if (privateKey_.getSize() == 0 && publicKey_.getSize() == 0)
         throw AccountException("invalid key length");
   }

   //local
   const SecureBinaryData& getPrivKey(void) const { return privateKey_; }
   const SecureBinaryData& getPubKey(void) const { return publicKey_; }

   //virtual
   AccountTypeEnum type(void) const override { return AccountTypeEnum_ECDH; }
   BinaryData getAccountID(void) const override;
   BinaryData getOuterAccountID(void) const override { return accountID_; }
   BinaryData getInnerAccountID(void) const override { return accountID_; }
   virtual bool isWatchingOnly(void) const override;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AssetAccount : protected Lockable
{
   friend class AssetAccount_ECDH;
   friend class AddressAccount;

private:
   BinaryData id_;
   BinaryData parent_id_;

   std::shared_ptr<AssetEntry> root_;
   std::shared_ptr<DerivationScheme> derScheme_;
   std::map<unsigned, std::shared_ptr<AssetEntry>> assets_;

   std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

   unsigned lastUsedIndex_ = UINT32_MAX;

   //<assetID, <address type, prefixed address hash>>
   std::map<BinaryData, std::map<AddressEntryType, BinaryData>> addrHashMap_;
   unsigned lastHashedAsset_ = UINT32_MAX;

private:
   size_t writeAssetEntry(std::shared_ptr<AssetEntry>);
   void updateOnDiskAssets(void);

   void updateHighestUsedIndex(void);
   unsigned getAndBumpHighestUsedIndex(void);

   void commit(void);
   void updateAssetCount(void);

   void extendPublicChainToIndex(unsigned);
   void extendPublicChain(std::shared_ptr<AssetEntry>, unsigned);
   std::vector<std::shared_ptr<AssetEntry>> extendPublicChain(
      std::shared_ptr<AssetEntry>, unsigned, unsigned);

   void extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChainToIndex(
      std::shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>, unsigned);
   std::vector<std::shared_ptr<AssetEntry>> extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      std::shared_ptr<AssetEntry>,
      unsigned, unsigned);

   std::shared_ptr<AssetEntry> getNewAsset(void);
   std::shared_ptr<Asset_PrivateKey> fillPrivateKey(
      std::shared_ptr<DecryptedDataContainer> ddc,
      const BinaryData& id);

   virtual unsigned getLookup(void) const { return DERIVATION_LOOKUP; }
   virtual uint8_t type(void) const { return AssetAccountTypeEnum_Plain; }

public:
   AssetAccount(
      const BinaryData& ID,
      const BinaryData& parentID,
      std::shared_ptr<AssetEntry> root,
      std::shared_ptr<DerivationScheme> scheme,
      std::shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      id_(ID), parent_id_(parentID),
      root_(root), derScheme_(scheme),
      dbEnv_(dbEnv), db_(db)
   {}

   size_t getAssetCount(void) const { return assets_.size(); }
   int getLastComputedIndex(void) const;
   unsigned getHighestUsedIndex(void) const { return lastUsedIndex_; }
   std::shared_ptr<AssetEntry> getLastAssetWithPrivateKey(void) const;

   std::shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;
   std::shared_ptr<AssetEntry> getAssetForIndex(unsigned id) const;

   // temporary moved from private to public session to allow the build of old code
   void extendPublicChain(unsigned);

   void updateAddressHashMap(const std::set<AddressEntryType>&);
   const std::map<BinaryData, std::map<AddressEntryType, BinaryData>>&
      getAddressHashMap(const std::set<AddressEntryType>&);

   const BinaryData& getID(void) const { return id_; }
   BinaryData getFullID(void) const { return parent_id_ + id_; }
   const SecureBinaryData& getChaincode(void) const;

   //static
   static void putData(LMDB* db, const BinaryData& key, const BinaryData& data);
   static std::shared_ptr<AssetAccount> loadFromDisk(
      const BinaryData& key, std::shared_ptr<LMDBEnv>, LMDB*);

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}

   std::shared_ptr<AssetEntry> getRoot(void) const { return root_; }
};

////////////////////////////////////////////////////////////////////////////////
class AssetAccount_ECDH : public AssetAccount
{
private:
   unsigned getLookup(void) const override { return 1; }
   uint8_t type(void) const override { return AssetAccountTypeEnum_ECDH; }

public:
   AssetAccount_ECDH(
      const BinaryData& ID,
      const BinaryData& parentID,
      std::shared_ptr<AssetEntry> root,
      std::shared_ptr<DerivationScheme> scheme,
      std::shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      AssetAccount(ID, parentID, root, scheme, dbEnv, db)
   {}

   unsigned addSalt(const SecureBinaryData&);
   unsigned getSaltIndex(const SecureBinaryData&) const;
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AddressAccount : public Lockable
{
   friend class AssetWallet;
   friend class AssetWallet_Single;

private:
   std::map<BinaryData, std::shared_ptr<AssetAccount>> assetAccounts_;
   std::map<BinaryData, AddressEntryType> addresses_;

   BinaryData outerAccount_;
   BinaryData innerAccount_;

   AddressEntryType defaultAddressEntryType_ = AddressEntryType_P2PKH;
   std::set<AddressEntryType> addressTypes_;

   //<prefixed address hash, <assetID, address type>>
   std::map<BinaryData, std::pair<BinaryData, AddressEntryType>> addressHashes_;

   //account id, asset id
   std::map<BinaryData, BinaryData> topHashedAssetId_;

   BinaryData ID_;
   std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

private:
   void commit(void); //used for initial commit to disk
   void reset(void);

   void addAccount(std::shared_ptr<AssetAccount>);

   void updateInstantiatedAddressType(std::shared_ptr<AddressEntry>);
   void updateInstantiatedAddressType(
      const BinaryData&, AddressEntryType);
   void writeAddressType(const BinaryData&, AddressEntryType);
   void eraseInstantiatedAddressType(const BinaryData&);
   std::shared_ptr<Asset_PrivateKey> fillPrivateKey(
      std::shared_ptr<DecryptedDataContainer> ddc,
      const BinaryData& id);

public:
   AddressAccount(
      std::shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      dbEnv_(dbEnv), db_(db)
   {}

   const BinaryData& getID(void) const { return ID_; }

   void make_new(
      std::shared_ptr<AccountType>,
      std::shared_ptr<DecryptedDataContainer>,
      std::unique_ptr<Cipher>);

   void readFromDisk(const BinaryData& key);

   void extendPublicChain(unsigned);
   void extendPublicChainToIndex(const BinaryData&, unsigned);

   void extendPrivateChain(
      std::shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChainToIndex(
      std::shared_ptr<DecryptedDataContainer>,
      const BinaryData&, unsigned);

   std::shared_ptr<AddressEntry> getNewAddress(
      AddressEntryType aeType = AddressEntryType_Default);
   std::shared_ptr<AddressEntry> getNewChangeAddress(
      AddressEntryType aeType = AddressEntryType_Default);
   std::shared_ptr<AddressEntry> getNewAddress(
      const BinaryData& account, AddressEntryType aeType);
   std::shared_ptr<AssetEntry> getOutterAssetForIndex(unsigned) const;
   std::shared_ptr<AssetEntry> getOutterAssetRoot(void) const;


   AddressEntryType getAddressType(void) const
      { return defaultAddressEntryType_; }
   std::set<AddressEntryType> getAddressTypeSet(void) const
      { return addressTypes_; }
   bool hasAddressType(AddressEntryType);

   //get asset by binary string ID
   std::shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;

   //get asset by integer ID; bool arg defines whether it comes from the
   //outer account (true) or the inner account (false)
   std::shared_ptr<AssetEntry> getAssetForID(unsigned, bool) const;

   const std::pair<BinaryData, AddressEntryType>& 
      getAssetIDPairForAddr(const BinaryData&);

   void updateAddressHashMap(void);
   const std::map<BinaryData, std::pair<BinaryData, AddressEntryType>>& 
      getAddressHashMap(void);

   std::shared_ptr<AssetAccount> getOuterAccount(void) const;
   const std::map<BinaryData, std::shared_ptr<AssetAccount>>& 
      getAccountMap(void) const;

   const BinaryData& getOuterAccountID(void) const { return outerAccount_; }
   const BinaryData& getInnerAccountID(void) const { return innerAccount_; }

   std::shared_ptr<LMDBEnv> getDbEnv(void) const { return dbEnv_; }

   std::shared_ptr<AddressAccount> getWatchingOnlyCopy(
      std::shared_ptr<LMDBEnv>, LMDB*) const;

   std::shared_ptr<AddressEntry> getAddressEntryForID(
      const BinaryDataRef&) const;
   std::map<BinaryData, std::shared_ptr<AddressEntry>> 
      getUsedAddressMap(void) const;

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}

};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class MetaDataAccount : public Lockable
{
   friend struct AuthPeerAssetConversion;

private:
   MetaAccountType type_ = MetaAccount_Unset;
   BinaryData ID_;
   std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

   std::map<unsigned, std::shared_ptr<MetaData>> assets_;

private:
   bool writeAssetToDisk(std::shared_ptr<MetaData>);

public:
   MetaDataAccount(std::shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      dbEnv_(dbEnv), db_(db)
   {}

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}

   //storage methods
   void readFromDisk(const BinaryData& key);
   void commit(void);
   void updateOnDisk(void);
   std::shared_ptr<MetaDataAccount> copy(
      std::shared_ptr<LMDBEnv>, LMDB* db) const;

   //setup methods
   void reset(void);
   void make_new(MetaAccountType);

   //
   std::shared_ptr<MetaData> getMetaDataByIndex(unsigned) const;
   void eraseMetaDataByIndex(unsigned);
   MetaAccountType getType(void) const { return type_; }
};

struct AuthPeerAssetMap
{
   //<name, authorized pubkey>
   std::map<std::string, const SecureBinaryData*> nameKeyPair_;
   
   //<pubkey, sig>
   std::pair<SecureBinaryData, SecureBinaryData> rootSignature_;

   //<pubkey, description>
   std::map<SecureBinaryData, std::pair<std::string, unsigned>> peerRootKeys_;
};

////////////////////////////////////////////////////////////////////////////////
struct AuthPeerAssetConversion
{
   static AuthPeerAssetMap getAssetMap(
      const MetaDataAccount*);
   static std::map<SecureBinaryData, std::set<unsigned>> getKeyIndexMap(
      const MetaDataAccount*);

   static int addAsset(MetaDataAccount*, const SecureBinaryData&,
      const std::vector<std::string>&);

   static void addRootSignature(MetaDataAccount*, 
      const SecureBinaryData&, const SecureBinaryData&);
   static unsigned addRootPeer(MetaDataAccount*,
      const SecureBinaryData&, const std::string&);
};

#endif
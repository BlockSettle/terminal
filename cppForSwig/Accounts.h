////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
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
#define IMPORTS_ACCOUNTID              0
#define ARMORY_LEGACY_ASSET_ACCOUNTID  1

#define BIP32_LEGACY_OUTER_ACCOUNT_DERIVATIONID 0x00000000
#define BIP32_LEGACY_INNER_ACCOUNT_DERIVATIONID 0x00000001
#define BIP32_SEGWIT_OUTER_ACCOUNT_DERIVATIONID 0x10000000
#define BIP32_SEGWIT_INNER_ACCOUNT_DERIVATIONID 0x10000001


#define ADDRESS_ACCOUNT_PREFIX   0xD0
#define ASSET_ACCOUNT_PREFIX     0xE1
#define ASSET_COUNT_PREFIX       0xE2
#define ASSET_TOP_INDEX_PREFIX   0xE3

class AccountException : public std::runtime_error
{
public:
   AccountException(const std::string& err) : std::runtime_error(err)
   {}
};

enum AccountTypeEnum
{
   /*
   armory derivation scheme 
   outer and inner account are the same
   uncompressed P2PKH, compresed P2SH-P2PK, P2SH-P2WPKH
   */
   AccountTypeEnum_ArmoryLegacy,

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
   
   AccountTypeEnum_BIP44,
   AccountTypeEnum_Custom
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct AccountType
{
protected:
   const AccountTypeEnum type_;
   const SecureBinaryData privateRoot_;
   const SecureBinaryData publicRoot_;
   mutable SecureBinaryData chainCode_;

   bool isMain_ = false;

   std::set<AddressEntryType> addressTypes_;
   AddressEntryType defaultAddressEntryType_;

public:
   AccountType(AccountTypeEnum val, 
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode) :
      type_(val), 
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
   virtual ~AccountType(void) = 0;
   virtual const SecureBinaryData& getChaincode(void) const = 0;
   virtual const SecureBinaryData& getPrivateRoot(void) const = 0;
   virtual const SecureBinaryData& getPublicRoot(void) const = 0;
   
   virtual BinaryData getAccountID(void) const = 0;
   virtual BinaryData getOuterAccountID(void) const = 0;
   virtual BinaryData getInnerAccountID(void) const = 0;

   //locals
   void setMain(bool ismain) { isMain_ = ismain; }
   const bool isMain(void) const { return isMain_; }
   AccountTypeEnum type(void) const { return type_; }

   const std::set<AddressEntryType>& getAddressTypes(void) const { return addressTypes_; }
   AddressEntryType getDefaultAddressEntryType(void) const { return defaultAddressEntryType_; }

   bool isWatchingOnly(void) const
   {
      return privateRoot_.getSize() == 0 &&
         publicRoot_.getSize() > 0 &&
         chainCode_.getSize() > 0;
   }
};

////////////////////
struct AccountType_ArmoryLegacy : public AccountType
{
public:
   AccountType_ArmoryLegacy(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode) :
      AccountType(AccountTypeEnum_ArmoryLegacy,
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

   const SecureBinaryData& getChaincode(void) const;
   const SecureBinaryData& getPrivateRoot(void) const { return privateRoot_; }
   const SecureBinaryData& getPublicRoot(void) const { return publicRoot_; }
   BinaryData getAccountID(void) const { return WRITE_UINT32_BE(ARMORY_LEGACY_ACCOUNTID); }
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////
struct AccountType_BIP32 : public AccountType
{
private:
   const std::vector<unsigned> derivationPath_;
   SecureBinaryData derivedRoot_;
   SecureBinaryData derivedChaincode_;

private:
   void deriveFromRoot(void);

public:
   AccountType_BIP32(
      AccountTypeEnum type,
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath) :
      AccountType(type, privateRoot, publicRoot, chainCode),
      derivationPath_(derivationPath)
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
};


////////////////////
struct AccountType_BIP32_Legacy : public AccountType_BIP32
{
private:
   void deriveFromRoot(void);

public:
   AccountType_BIP32_Legacy(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath) :
      AccountType_BIP32(AccountTypeEnum_BIP32_Legacy,
         privateRoot, publicRoot, chainCode, derivationPath)
   {
      //uncompressed p2pkh
      addressTypes_.insert(AddressEntryType_P2PKH);

      //compressed p2pkh
      addressTypes_.insert(
         AddressEntryType(AddressEntryType_Compressed | AddressEntryType_P2PKH));

      defaultAddressEntryType_ =
         AddressEntryType(AddressEntryType_Compressed | AddressEntryType_P2PKH);
   }

   std::set<unsigned> getNodes(void) const;
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////
struct AccountType_BIP32_SegWit : public AccountType_BIP32
{
private:
   void deriveFromRoot(void);

public:
   AccountType_BIP32_SegWit(
      SecureBinaryData& privateRoot,
      SecureBinaryData& publicRoot,
      SecureBinaryData& chainCode,
      const std::vector<unsigned>& derivationPath) :
      AccountType_BIP32(AccountTypeEnum_BIP32_SegWit,
      privateRoot, publicRoot, chainCode, derivationPath)
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

   std::set<unsigned> getNodes(void) const;
   BinaryData getOuterAccountID(void) const;
   BinaryData getInnerAccountID(void) const;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AssetAccount : protected Lockable
{
   friend class AddressAccount;

private:
   BinaryData id_;
   BinaryData parent_id_;

   std::shared_ptr<AssetEntry> root_;
   std::shared_ptr<DerivationScheme> derScheme_;
   std::map<unsigned, std::shared_ptr<AssetEntry>> assets_;

   std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

   unsigned lastUsedIndex_ = 0;

   std::map<BinaryData, std::map<AddressEntryType, BinaryData>> addrHashMap_;
   bool updateHashMap_ = true;

private:
   size_t writeAssetEntry(std::shared_ptr<AssetEntry>);
   void updateOnDiskAssets(void);

   void updateHighestUsedIndex(void);
   unsigned getAndBumpHighestUsedIndex(void);

   void commit(void);
   void updateAssetCount(void);

   void extendPublicChain(unsigned);
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

   void unserialize(const BinaryDataRef);

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
   std::shared_ptr<AssetEntry> getLastAssetWithPrivateKey(void) const;

   std::shared_ptr<AssetEntry> getNewAsset(void);
   std::shared_ptr<AddressEntry> getNewAddress(AddressEntryType aeType);

   std::shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;
   std::shared_ptr<AssetEntry> getAssetForIndex(unsigned id) const;

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
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AddressAccount : public Lockable
{
   friend class AssetWallet_Single;

private:
   std::map<BinaryData, std::shared_ptr<AssetAccount>> assetAccounts_;
   
   BinaryData outerAccount_;
   BinaryData innerAccount_;

   AddressEntryType defaultAddressEntryType_ = AddressEntryType_P2PKH;
   std::set<AddressEntryType> addressTypes_;
   std::map<BinaryData, BinaryData> addressHashes_;

   BinaryData ID_;
   std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

private:
   void commit(void); //used for initial commit to disk
   void reset(void);

   void addAccount(std::shared_ptr<AssetAccount>);

public:
   //to search sets
   bool operator<(const AddressAccount& rhs)
   {
      if (rhs.ID_.getSize() < ID_.getSize())
         return ID_ < rhs.ID_;

      auto idRef = rhs.ID_.getSliceRef(0, ID_.getSize());
      return ID_ < idRef;
   }

   struct find_by_id
   {
      const BinaryData ID_;

      find_by_id(const BinaryData& ID) :
         ID_(ID)
      {}

      bool operator()(const AddressAccount& account)
      {
         if (account.ID_.getSize() < ID_.getSize())
            return false;

         auto idRef = account.ID_.getSliceRef(0, ID_.getSize());
         return ID_ == idRef;
      }

      bool operator()(const std::shared_ptr<AddressAccount>& account)
      {
         if (account->ID_.getSize() < ID_.getSize())
            return false;

         auto idRef = account->ID_.getSliceRef(0, ID_.getSize());
         return ID_ == idRef;
      }
   };

public:
   AddressAccount(
      std::shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      dbEnv_(dbEnv), db_(db)
   {}

   const BinaryData& getID(void) const { return ID_; }

   void make_new(
      std::shared_ptr<AccountType>,
      std::shared_ptr<DecryptedDataContainer>,
      std::unique_ptr<Cypher>);

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

   AddressEntryType getAddressType(void) const { return defaultAddressEntryType_; }
   bool hasAddressType(AddressEntryType);

   std::shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;
   const BinaryData& getAssetIDForAddr(const BinaryData&);

   void updateAddressHashMap(void);
   const std::map<BinaryData, BinaryData>& getAddressHashMap(void);

   std::shared_ptr<AssetAccount> getOuterAccount(void) const;
   const std::map<BinaryData, std::shared_ptr<AssetAccount>>& getAccountMap(void) const;

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}
};

#endif
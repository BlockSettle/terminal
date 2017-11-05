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
#include "lmdb/lmdbpp.h"
#include "EncryptionUtils.h"
#include "Assets.h"
#include "Addresses.h"
#include "DerivationScheme.h"

#define ARMORY_LEGACY_ACCOUNTID  0xF6E10000
#define OUTER_ASSET_ACCOUNTID    0x10000000  
#define INNER_ASSET_ACCOUNTID    0x20000000  

#define ADDRESS_ACCOUNT_PREFIX   0xD0

#define ASSET_ACCOUNT_PREFIX     0xE1
#define ASSET_COUNT_PREFIX       0xE2
#define ASSET_TOP_INDEX_PREFIX   0xE3

class AccountException : public runtime_error
{
public:
   AccountException(const string& err) : runtime_error(err)
   {}
};

enum AccountTypeEnum
{
   AccountTypeEnum_ArmoryLegacy,
   AccountTypeEnum_ArmoryLegacy_WatchingOnly,
   AccountTypeEnum_BIP32,
   AccountTypeEnum_BIP44,
   AccountTypeEnum_Custom
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AssetAccount : protected Lockable
{
   friend class AddressAccount;

private:
   BinaryData id_;
   BinaryData parent_id_;

   shared_ptr<AssetEntry> root_;
   shared_ptr<DerivationScheme> derScheme_;
   map<unsigned, shared_ptr<AssetEntry>> assets_;

   shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

   unsigned lastUsedIndex_ = 0;

   map<BinaryData, map<AddressEntryType, BinaryData>> addrHashMap_;
   bool updateHashMap_ = true;

private:
   size_t writeAssetEntry(shared_ptr<AssetEntry>);
   void updateOnDiskAssets(void);

   void updateHighestUsedIndex(void);
   unsigned getAndBumpHighestUsedIndex(void);

   void commit(void);
   void updateAssetCount(void);

   void extendPublicChain(unsigned);
   void extendPublicChain(shared_ptr<AssetEntry>, unsigned);
   void extendPublicChainToIndex(unsigned);

   void extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChainToIndex(
      shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      shared_ptr<AssetEntry>, unsigned);

   void unserialize(const BinaryDataRef);

public:
   AssetAccount(
      const BinaryData& ID,
      const BinaryData& parentID,
      shared_ptr<AssetEntry> root, 
      shared_ptr<DerivationScheme> scheme,
      shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      id_(ID), parent_id_(parentID),
      root_(root), derScheme_(scheme),
      dbEnv_(dbEnv), db_(db)
   {}
   
   size_t getAssetCount(void) const { return assets_.size(); }
   int getLastComputedIndex(void) const;
   shared_ptr<AssetEntry> getLastAssetWithPrivateKey(void) const;

   shared_ptr<AssetEntry> getNewAsset(void);
   shared_ptr<AddressEntry> getNewAddress(AddressEntryType aeType);
   
   shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;
   shared_ptr<AssetEntry> getAssetForIndex(unsigned id) const;

   void updateAddressHashMap(const set<AddressEntryType>&);
   const map<BinaryData, map<AddressEntryType, BinaryData>>& 
      getAddressHashMap(const set<AddressEntryType>&);

   const BinaryData& getID(void) const { return id_; }
   BinaryData getFullID(void) const { return parent_id_ + id_; }
   const SecureBinaryData& getChaincode(void) const;

   //static
   static void putData(LMDB* db, const BinaryData& key, const BinaryData& data);
   static shared_ptr<AssetAccount> loadFromDisk(
      const BinaryData& key, shared_ptr<LMDBEnv>, LMDB*);

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct AccountType
{
protected:
   const AccountTypeEnum type_;
   const SecureBinaryData root_;
   mutable SecureBinaryData chainCode_;

   bool isMain_ = false;

public:
   AccountType(AccountTypeEnum val, const SecureBinaryData& root) :
      type_(val), root_(root)
   {}

   virtual ~AccountType(void) = 0;
   virtual const SecureBinaryData& getChaincode(void) const = 0;
   
   void setMain(bool ismain) { isMain_ = ismain; }
   const bool isMain(void) const { return isMain_; }
   const SecureBinaryData& getRoot(void) const { return root_; }

   AccountTypeEnum type(void) const { return type_; }
};

////////////////////
struct AccountType_ArmoryLegacy : public AccountType
{
private:

public:
   AccountType_ArmoryLegacy(const SecureBinaryData& root) :
      AccountType(AccountTypeEnum_ArmoryLegacy, root)
   {}

   const SecureBinaryData& getChaincode(void) const;
};

////////////////////
struct AccountType_ArmoryLegacy_WatchingOnly : public AccountType
{
private:
   const SecureBinaryData chainCode_;

public:
   AccountType_ArmoryLegacy_WatchingOnly(
      const SecureBinaryData& root , const SecureBinaryData& chainCode) :
      AccountType(AccountTypeEnum_ArmoryLegacy_WatchingOnly, root), 
      chainCode_(chainCode)
   {}

   const SecureBinaryData& getChaincode(void) const { return chainCode_; }
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class AddressAccount : public Lockable
{
   friend class AssetWallet_Single;

private:
   map<BinaryData, shared_ptr<AssetAccount>> assetAccounts_;
   
   BinaryData outerAccount_;
   BinaryData innerAccount_;

   AddressEntryType defaultAddressEntryType_ = AddressEntryType_P2PKH;
   set<AddressEntryType> addressTypes_;
   map<BinaryData, BinaryData> addressHashes_;

   BinaryData ID_;
   shared_ptr<LMDBEnv> dbEnv_ = nullptr;
   LMDB* db_ = nullptr;

private:
   void commit(void); //used for initial commit to disk
   void reset(void);

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

      bool operator()(const shared_ptr<AddressAccount>& account)
      {
         if (account->ID_.getSize() < ID_.getSize())
            return false;

         auto idRef = account->ID_.getSliceRef(0, ID_.getSize());
         return ID_ == idRef;
      }
   };

public:
   AddressAccount(
      shared_ptr<LMDBEnv> dbEnv, LMDB* db) :
      dbEnv_(dbEnv), db_(db)
   {}

   const BinaryData& getID(void) const { return ID_; }

   void make_new(
      shared_ptr<AccountType>,
      shared_ptr<DecryptedDataContainer>,
      unique_ptr<Cypher>);

   void readFromDisk(const BinaryData& key);

   void extendPublicChain(unsigned);
   void extendPublicChainToIndex(const BinaryData&, unsigned);

   void extendPrivateChain(
      shared_ptr<DecryptedDataContainer>,
      unsigned);
   void extendPrivateChainToIndex(
      shared_ptr<DecryptedDataContainer>,
      const BinaryData&, unsigned);

   shared_ptr<AddressEntry> getNewAddress(
      AddressEntryType aeType = AddressEntryType_Default);
   shared_ptr<AddressEntry> getNewChangeAddress(
      AddressEntryType aeType = AddressEntryType_Default);
   shared_ptr<AddressEntry> getNewAddress(
      const BinaryData& account, AddressEntryType aeType);
   shared_ptr<AssetEntry> getOutterAssetForIndex(unsigned) const;

   AddressEntryType getAddressType(void) const { return defaultAddressEntryType_; }
   bool hasAddressType(AddressEntryType);

   shared_ptr<AssetEntry> getAssetForID(const BinaryData&) const;
   const BinaryData& getAssetIDForAddr(const BinaryData&);

   void updateAddressHashMap(void);
   const map<BinaryData, BinaryData>& getAddressHashMap(void);

   shared_ptr<AssetAccount> getOuterAccount(void) const;
   const map<BinaryData, shared_ptr<AssetAccount>>& getAccountMap(void) const;

   //Lockable virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}
};

#endif
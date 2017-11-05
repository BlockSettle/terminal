////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "DBUtils.h"
#include "Accounts.h"
#include "DecryptedDataContainer.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AssetType
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
AccountType::~AccountType()
{}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AccountType_ArmoryLegacy::getChaincode() const
{
   if (chainCode_.getSize() == 0)
      chainCode_ = move(BtcUtils::computeChainCode_Armory135(getRoot()));

   return chainCode_;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AssetAccount
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
size_t AssetAccount::writeAssetEntry(shared_ptr<AssetEntry> entryPtr)
{
   if (!entryPtr->needsCommit())
      return SIZE_MAX;

   auto&& serializedEntry = entryPtr->serialize();
   auto&& dbKey = entryPtr->getDbKey();

   CharacterArrayRef keyRef(dbKey.getSize(), dbKey.getPtr());
   CharacterArrayRef dataRef(serializedEntry.getSize(), serializedEntry.getPtr());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db_->insert(keyRef, dataRef);

   entryPtr->doNotCommit();
   return serializedEntry.getSize();
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::updateOnDiskAssets()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   for (auto& entryPtr : assets_)
      writeAssetEntry(entryPtr.second);

   updateAssetCount();
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::updateAssetCount()
{
   //asset count key
   BinaryWriter bwKey;
   bwKey.put_uint8_t(ASSET_COUNT_PREFIX);
   bwKey.put_BinaryData(getFullID());

   //asset count
   BinaryWriter bwData;
   bwData.put_var_int(assets_.size());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   putData(db_, bwKey.getData(), bwData.getData());
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::commit()
{
   //id as key
   BinaryWriter bwKey;
   bwKey.put_uint8_t(ASSET_ACCOUNT_PREFIX);
   bwKey.put_BinaryData(getFullID());

   //data
   BinaryWriter bwData;

   //parent key size
   bwData.put_var_int(parent_id_.getSize());

   //der scheme
   auto&& derSchemeSerData = derScheme_->serialize();
   bwData.put_var_int(derSchemeSerData.getSize());
   bwData.put_BinaryData(derSchemeSerData);
   
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   
   //commit root asset if there is one
   if (root_ != nullptr)
      writeAssetEntry(root_);

   //commit assets
   for (auto asset : assets_)
      writeAssetEntry(asset.second);

   //commit serialized account data
   putData(db_, bwKey.getData(), bwData.getData());

   updateAssetCount();
   updateHighestUsedIndex();
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::putData(
   LMDB* db, const BinaryData& key, const BinaryData& data)
{
   CharacterArrayRef carKey(key.getSize(), key.getCharPtr());
   CharacterArrayRef carData(data.getSize(), data.getCharPtr());

   db->insert(carKey, carData);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetAccount> AssetAccount::loadFromDisk(
   const BinaryData& key, shared_ptr<LMDBEnv> dbEnv, LMDB* db)
{
   //sanity checks
   if (dbEnv == nullptr || db == nullptr)
      throw AccountException("invalid db pointers");

   if (key.getSize() == 0)
      throw AccountException("invalid key size");

   if (key.getPtr()[0] != ASSET_ACCOUNT_PREFIX)
      throw AccountException("unexpected prefix for AssetAccount key");

   LMDBEnv::Transaction tx(dbEnv.get(), LMDB::ReadOnly);
   CharacterArrayRef carKey(key.getSize(), key.getCharPtr());

   auto carData = db->get_NoCopy(carKey);
   BinaryRefReader brr((const uint8_t*)carData.data, carData.len);

   //ids
   auto parent_id_len = brr.get_var_int();

   auto&& parent_id = key.getSliceCopy(1, parent_id_len);
   auto&& account_id = key.getSliceCopy(
      1 + parent_id_len, key.getSize() - 1 - parent_id_len);

   //der scheme
   auto len = brr.get_var_int();
   auto derSchemeBDR = DBUtils::getDataRefForPacket(brr.get_BinaryDataRef(len));
   auto derScheme = DerivationScheme::deserialize(derSchemeBDR);

   //asset count
   size_t assetCount = 0;
   {
      BinaryWriter bwKey_assetcount;
      bwKey_assetcount.put_uint8_t(ASSET_COUNT_PREFIX);
      bwKey_assetcount.put_BinaryDataRef(key.getSliceRef(
         1, key.getSize() - 1));
      CharacterArrayRef carKey_assetcount(
         bwKey_assetcount.getSize(),
         bwKey_assetcount.getData().getCharPtr());

      auto&& car_assetcount = db->get_NoCopy(carKey_assetcount);
      if (car_assetcount.len == 0)
         throw AccountException("missing asset count entry");

      BinaryDataRef bdr_assetcount(
         (uint8_t*)car_assetcount.data, car_assetcount.len);
      BinaryRefReader brr_assetcount(bdr_assetcount);
      assetCount = brr_assetcount.get_var_int();
   }

   //last used index
   size_t lastUsedIndex = 0;
   {
      BinaryWriter bwKey_lastusedindex;
      bwKey_lastusedindex.put_uint8_t(ASSET_TOP_INDEX_PREFIX);
      bwKey_lastusedindex.put_BinaryDataRef(key.getSliceRef(
         1, key.getSize() - 1));
      CharacterArrayRef carKey_lastusedindex(
         bwKey_lastusedindex.getSize(),
         bwKey_lastusedindex.getData().getCharPtr());

      auto&& car_lastusedindex = db->get_NoCopy(carKey_lastusedindex);
      if (car_lastusedindex.len == 0)
         throw AccountException("missing last used entry");

      BinaryDataRef bdr_lastusedindex(
         (uint8_t*)car_lastusedindex.data, car_lastusedindex.len);
      BinaryRefReader brr_lastusedindex(bdr_lastusedindex);
      lastUsedIndex = brr_lastusedindex.get_var_int();
   }

   //asset entry prefix key
   BinaryWriter bwAssetKey;
   bwAssetKey.put_uint8_t(ASSETENTRY_PREFIX);
   bwAssetKey.put_BinaryDataRef(key.getSliceRef(1, key.getSize() - 1));
   
   //asset key
   BinaryWriter bwRootkey;
   bwRootkey.put_BinaryData(bwAssetKey.getData());
   bwRootkey.put_uint32_t(ROOT_ASSETENTRY_ID); //root entry int id
   CharacterArrayRef carRootKey(
      bwRootkey.getSize(), bwRootkey.getData().getCharPtr());

   shared_ptr<AssetEntry> rootEntry = nullptr;
   map<unsigned, shared_ptr<AssetEntry>> assetMap;
   
   //get all assets
   {
      auto& assetDbKey = bwAssetKey.getData();
      CharacterArrayRef carAssetKey(
         assetDbKey.getSize(), assetDbKey.getCharPtr());

      auto dbIter = db->begin();
      dbIter.seek(carAssetKey, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid())
      {
         BinaryDataRef key_bdr(
            (const uint8_t*)dbIter.key().mv_data, dbIter.key().mv_size);
         BinaryDataRef value_bdr(
            (const uint8_t*)dbIter.value().mv_data, dbIter.value().mv_size);

         //check key isnt prefix
         if (key_bdr == assetDbKey)
            continue;

         //check key starts with prefix
         if (!key_bdr.startsWith(assetDbKey))
            break;

         //instantiate and insert asset
         auto assetPtr = AssetEntry::deserialize(
            key_bdr, 
            DBUtils::getDataRefForPacket(value_bdr));

         if (assetPtr->getIndex() != ROOT_ASSETENTRY_ID)
            assetMap.insert(make_pair(assetPtr->getIndex(), assetPtr));
         else
            rootEntry = assetPtr;

         ++dbIter;
      } 
   }

   //sanity check
   if (assetCount != assetMap.size())
      throw AccountException("unexpected account asset count");

   //instantiate object
   shared_ptr<AssetAccount> accountPtr = make_shared<AssetAccount>(
      account_id, parent_id,
      rootEntry, derScheme, 
      dbEnv, db);

   //fill members not covered by the ctor
   accountPtr->lastUsedIndex_ = lastUsedIndex;
   accountPtr->assets_ = move(assetMap);

   return accountPtr;
}

////////////////////////////////////////////////////////////////////////////////
int AssetAccount::getLastComputedIndex(void) const
{
   if (getAssetCount() == 0)
      return -1;

   auto iter = assets_.rbegin();
   return iter->first;
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPublicChain(unsigned count)
{
   ReentrantLock lock(this);

   //add *count* entries to address chain
   if (assets_.size() == 0)
      throw AccountException("empty asset map");

   if (count == 0)
      return;

   extendPublicChain(assets_.rbegin()->second, count);
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPublicChainToIndex(unsigned count)
{
   ReentrantLock lock(this);

   //make address chain at least *count* long
   auto lastComputedIndex = max(getLastComputedIndex(), 0);
   if (lastComputedIndex > count)
      return;

   auto toCompute = count - lastComputedIndex;
   extendPublicChain(toCompute);
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPublicChain(
   shared_ptr<AssetEntry> assetPtr, unsigned count)
{
   if (count == 0)
      return;

   ReentrantLock lock(this);

   auto&& assetVec = derScheme_->extendPublicChain(assetPtr, count);

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      for (auto& asset : assetVec)
      {
         auto id = asset->getIndex();
         auto iter = assets_.find(id);
         if (iter != assets_.end())
            continue;

         assets_.insert(make_pair(
            id, asset));
      }
   }

   updateOnDiskAssets();
   updateHashMap_ = true;
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   unsigned count)
{
   ReentrantLock lock(this);
   auto topAsset = getLastAssetWithPrivateKey();

   extendPrivateChain(ddc, topAsset, count);
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPrivateChainToIndex(
   shared_ptr<DecryptedDataContainer> ddc,
   unsigned id)
{
   ReentrantLock lock(this);

   auto topAsset = getLastAssetWithPrivateKey();
   auto topIndex = topAsset->getIndex();

   if (id > topIndex)
   {
      auto count = id - topAsset->getIndex();
      extendPrivateChain(ddc, topAsset, count);
   }
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   shared_ptr<AssetEntry> assetPtr, unsigned count)
{
   if (count == 0)
      return;

   ReentrantLock lock(this);
   auto privAsset = getLastAssetWithPrivateKey();
   auto lastIndex = getLastComputedIndex();

   auto&& assetVec = derScheme_->extendPrivateChain(
      ddc,
      assetPtr, count);

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {
      for (auto& asset : assetVec)
      {
         auto id = asset->getIndex();
         auto iter = assets_.find(id);
         if (iter != assets_.end())
         {
            //do not overwrite an existing asset that already has a privkey
            if (iter->second->hasPrivateKey())
            {
               continue;
            }
            else
            {
               iter->second = asset;
               continue;
            }
         }

         assets_.insert(make_pair(
            id, asset));
      }
   }

   updateOnDiskAssets();

   if (privAsset->getIndex() + count > lastIndex)
      updateHashMap_ = true;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetAccount::getLastAssetWithPrivateKey() const
{
   ReentrantLock lock(this);

   auto assetIter = assets_.rbegin();
   while (assetIter != assets_.rend())
   {
      if (assetIter->second->hasPrivateKey())
         return assetIter->second;

      ++assetIter;
   }

   throw runtime_error("no asset with private keys");
   return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::updateHighestUsedIndex()
{
   ReentrantLock lock(this);
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   BinaryWriter bwKey;
   bwKey.put_uint8_t(ASSET_TOP_INDEX_PREFIX);
   bwKey.put_BinaryData(getFullID());

   BinaryWriter bwData;
   bwData.put_var_int(lastUsedIndex_);

   putData(db_, bwKey.getData(), bwData.getData());
}

////////////////////////////////////////////////////////////////////////////////
unsigned AssetAccount::getAndBumpHighestUsedIndex()
{
   ReentrantLock lock(this);

   auto index = lastUsedIndex_++;
   updateHighestUsedIndex();
   return index;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetAccount::getNewAsset()
{
   ReentrantLock lock(this);

   auto index = getAndBumpHighestUsedIndex();
   auto entryIter = assets_.find(index);
   if (entryIter == assets_.end())
   {
      extendPublicChain(DERIVATION_LOOKUP);
      entryIter = assets_.find(index);
      if (entryIter == assets_.end())
         throw AccountException("requested index overflows max lookup");
   }

   return entryIter->second;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AssetAccount::getNewAddress(AddressEntryType aeType)
{
   auto asset = getNewAsset();
   return AddressEntry::instantiate(asset, aeType);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetAccount::getAssetForID(const BinaryData& ID) const
{
   if (ID.getSize() < 4)
      throw runtime_error("invalid asset ID");

   auto id_int = READ_UINT32_BE(ID.getPtr());
   return getAssetForIndex(id_int);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AssetAccount::getAssetForIndex(unsigned id) const
{
   auto iter = assets_.find(id);
   if (iter == assets_.end())
      throw AccountException("unknown asset index");

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
void AssetAccount::updateAddressHashMap(
   const set<AddressEntryType>& typeSet)
{
   if (updateHashMap_ = false)
      return;

   ReentrantLock lock(this);

   for (auto asset : assets_)
   {
      auto asset_iter = addrHashMap_.find(asset.second->getID());
      if (asset_iter == addrHashMap_.end())
      {
         asset_iter = 
            addrHashMap_.insert(make_pair(
               asset.second->getID(), 
               map<AddressEntryType, BinaryData>())).first;
      }

      for (auto ae_type : typeSet)
      {
         if (asset_iter->second.find(ae_type) != asset_iter->second.end())
            continue;

         auto addrPtr = AddressEntry::instantiate(asset.second, ae_type);
         auto& addrHash = addrPtr->getPrefixedHash();
         addrHashMap_[asset.second->getID()].insert(
            make_pair(ae_type, addrHash));
      }
   }

   updateHashMap_ = false;
}

////////////////////////////////////////////////////////////////////////////////
const map<BinaryData, map<AddressEntryType, BinaryData>>& 
   AssetAccount::getAddressHashMap(const set<AddressEntryType>& typeSet)
{
   updateAddressHashMap(typeSet);

   return addrHashMap_;
}

////////////////////////////////////////////////////////////////////////////////
const SecureBinaryData& AssetAccount::getChaincode() const
{
   if (derScheme_ == nullptr)
      throw AccountException("null derivation scheme");

   return derScheme_->getChaincode();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// AddressAccount
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void AddressAccount::make_new(
   shared_ptr<AccountType> accType,
   shared_ptr<DecryptedDataContainer> decrData,
   unique_ptr<Cypher> cypher)
{
   reset();

   switch (accType->type())
   {
   case AccountTypeEnum_ArmoryLegacy:
   {
      ID_ = WRITE_UINT32_BE(ARMORY_LEGACY_ACCOUNTID);
      auto asset_account_id = WRITE_UINT32_BE(OUTER_ASSET_ACCOUNTID);

      auto& root = accType->getRoot();
      auto chaincode = accType->getChaincode();

      auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chaincode);

      //first derived asset
      auto& full_account_id = ID_ + asset_account_id;
      shared_ptr<AssetEntry_Single> firstAsset;
      {
         ReentrantLock lock(decrData.get());
         firstAsset = derScheme->computeNextPrivateEntry(
            decrData,
            root, move(cypher),
            full_account_id, 0);
      }

      //instantiate account and set first entry
      auto asset_account = make_shared<AssetAccount>(
         asset_account_id, ID_,
         nullptr, derScheme, //no root asset for legacy derivation scheme, using first entry instead
         dbEnv_, db_);
      asset_account->assets_.insert(make_pair(0, firstAsset));

      //add the asset account
      assetAccounts_.insert(make_pair(asset_account_id, asset_account));

      //set the address types
      {
         //uncompressed p2pkh
         addressTypes_.insert(AddressEntryType_P2PKH);

         //nested compressed p2pk
         addressTypes_.insert(AddressEntryType(
            AddressEntryType_P2PK | AddressEntryType_Compressed | AddressEntryType_P2SH));

         //nested p2wsh
         addressTypes_.insert(AddressEntryType(
            AddressEntryType_P2WPKH | AddressEntryType_P2SH));
      }

      //set default address type
      defaultAddressEntryType_ = AddressEntryType_P2PKH;

      //set inner and outer accounts
      outerAccount_ = asset_account_id;
      innerAccount_ = asset_account_id;
      
      break;
   }

   case AccountTypeEnum_ArmoryLegacy_WatchingOnly:
   {
      ID_ = WRITE_UINT32_BE(ARMORY_LEGACY_ACCOUNTID);
      auto asset_account_id = WRITE_UINT32_BE(OUTER_ASSET_ACCOUNTID);

      auto& root = accType->getRoot();
      auto chaincode = accType->getChaincode();

      auto derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chaincode);

      //first derived asset
      auto& full_account_id = ID_ + asset_account_id;
      shared_ptr<AssetEntry_Single> firstAsset;
      {
         firstAsset = derScheme->computeNextPublicEntry(
            root, 
            full_account_id, 0);
      }

      //instantiate account and set first entry
      auto asset_account = make_shared<AssetAccount>(
         asset_account_id, ID_,
         nullptr, derScheme, //no root asset for legacy derivation scheme, using first entry instead
         dbEnv_, db_);
      asset_account->assets_.insert(make_pair(0, firstAsset));

      //add the asset account
      assetAccounts_.insert(make_pair(asset_account_id, asset_account));

      //set the address types
      {
         //uncompressed p2pkh
         addressTypes_.insert(AddressEntryType_P2PKH);

         //nested compressed p2pk
         addressTypes_.insert(AddressEntryType(
            AddressEntryType_P2PK | AddressEntryType_Compressed | AddressEntryType_P2SH));

         //nested p2wsh
         addressTypes_.insert(AddressEntryType(
            AddressEntryType_P2WPKH | AddressEntryType_P2SH));
      }

      //set default address type
      defaultAddressEntryType_ = AddressEntryType_P2PKH;

      //set inner and outer accounts
      outerAccount_ = asset_account_id;
      innerAccount_ = asset_account_id;

      break;
   }

   default:
      throw AccountException("unknown account type");
   }
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::reset()
{
   outerAccount_.clear();
   innerAccount_.clear();

   assetAccounts_.clear();
   addressTypes_.clear();
   addressHashes_.clear();
   ID_.clear();
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::commit()
{
   //id as key
   BinaryWriter bwKey;
   bwKey.put_uint8_t(ADDRESS_ACCOUNT_PREFIX);
   bwKey.put_BinaryData(ID_);

   //data
   BinaryWriter bwData;

   //outer and inner account
   bwData.put_var_int(outerAccount_.getSize());
   bwData.put_BinaryData(outerAccount_);

   bwData.put_var_int(innerAccount_.getSize());
   bwData.put_BinaryData(innerAccount_);

   //address type set
   bwData.put_var_int(addressTypes_.size());

   for (auto& addrType : addressTypes_)
      bwData.put_uint32_t(addrType);

   //default address type
   bwData.put_uint32_t(defaultAddressEntryType_);

   //asset accounts count
   bwData.put_var_int(assetAccounts_.size());

   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   //asset accounts
   for (auto& account : assetAccounts_)
   {
      auto&& assetAccountID = account.second->getFullID();
      bwData.put_var_int(assetAccountID.getSize());
      bwData.put_BinaryData(assetAccountID);

      account.second->commit();
   }

   //commit address account data to disk
   CharacterArrayRef carKey(bwKey.getSize(), bwKey.getData().getCharPtr());
   CharacterArrayRef carData(bwData.getSize(), bwData.getData().getCharPtr());

   db_->insert(carKey, carData);
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::readFromDisk(const BinaryData& key)
{
   //sanity checks
   if (key.getSize() == 0)
      throw AccountException("empty AddressAccount key");

   if (key.getPtr()[0] != ADDRESS_ACCOUNT_PREFIX)
      throw AccountException("unexpected key prefix for AddressAccount");

   if (dbEnv_ == nullptr || db_ == nullptr)
      throw AccountException("unintialized AddressAccount object");

   //wipe object prior to loading from disk
   reset();

   //get data from disk
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);
   
   CharacterArrayRef carKey(key.getSize(), key.getCharPtr());
   auto&& carData = db_->get_NoCopy(carKey);

   BinaryDataRef bdr((const uint8_t*)carData.data, carData.len);
   BinaryRefReader brr(bdr);

   //outer and inner accounts
   size_t len, count;
   
   len = brr.get_var_int();
   outerAccount_ = brr.get_BinaryData(len);

   len = brr.get_var_int();
   innerAccount_ = brr.get_BinaryData(len);

   //address type set
   count = brr.get_var_int();
   for (unsigned i = 0; i < count; i++)
      addressTypes_.insert(AddressEntryType(brr.get_uint32_t()));

   //default address type
   defaultAddressEntryType_ = AddressEntryType(brr.get_uint32_t());

   //asset accounts
   count = brr.get_var_int();

   for (unsigned i = 0; i < count; i++)
   {
      len = brr.get_var_int();
      BinaryWriter bw_asset_key(1 + len);
      bw_asset_key.put_uint8_t(ASSET_ACCOUNT_PREFIX);
      bw_asset_key.put_BinaryData(brr.get_BinaryData(len));

      auto accountPtr = AssetAccount::loadFromDisk(
         bw_asset_key.getData(), dbEnv_, db_);
      assetAccounts_.insert(make_pair(accountPtr->id_, accountPtr));
   }

   ID_ = key.getSliceCopy(1, key.getSize() - 1);
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::extendPublicChain(unsigned count)
{
   for (auto& account : assetAccounts_)
      account.second->extendPublicChain(count);
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   unsigned count)
{
   for (auto& account : assetAccounts_)
      account.second->extendPrivateChain(ddc, count);
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::extendPublicChainToIndex(
   const BinaryData& accountID, unsigned count)
{
   auto iter = assetAccounts_.find(accountID);
   if (iter == assetAccounts_.end())
      throw AccountException("unknown account");

   iter->second->extendPublicChainToIndex(count);
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::extendPrivateChainToIndex(
   shared_ptr<DecryptedDataContainer> ddc,
   const BinaryData& accountID, unsigned count)
{
   auto iter = assetAccounts_.find(accountID);
   if (iter == assetAccounts_.end())
      throw AccountException("unknown account");

   iter->second->extendPrivateChainToIndex(ddc, count);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AddressAccount::getNewAddress(
   AddressEntryType aeType)
{
   if (outerAccount_.getSize() == 0)
      throw AccountException("no currently active asset account");

   return getNewAddress(outerAccount_, aeType);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AddressAccount::getNewChangeAddress(
   AddressEntryType aeType)
{
   if (innerAccount_.getSize() == 0)
      throw AccountException("no currently active asset account");

   return getNewAddress(innerAccount_, aeType);
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AddressEntry> AddressAccount::getNewAddress(
   const BinaryData& account, AddressEntryType aeType)
{
   auto iter = assetAccounts_.find(account);
   if (iter == assetAccounts_.end())
      throw AccountException("invalid asset account");

   if (aeType == AddressEntryType_Default)
      aeType = defaultAddressEntryType_;

   auto aeIter = addressTypes_.find(aeType);
   if (aeIter == addressTypes_.end())
      throw AccountException("invalid address type for this account");

   return iter->second->getNewAddress(aeType);
}

////////////////////////////////////////////////////////////////////////////////
bool AddressAccount::hasAddressType(AddressEntryType aeType)
{
   auto iter = addressTypes_.find(aeType);
   return iter != addressTypes_.end();
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AddressAccount::getAssetForID(const BinaryData& ID) const
{
   if (ID.getSize() < 4)
      throw AccountException("invalid asset ID");

   auto accID = ID.getSliceRef(0, 4);
   auto iter = assetAccounts_.find(accID);

   if (iter == assetAccounts_.end())
      throw AccountException("unknown account ID");

   auto assetID = ID.getSliceRef(4, ID.getSize() - 4);
   return iter->second->getAssetForID(assetID);
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& AddressAccount::getAssetIDForAddr(const BinaryData& scrAddr)
{
   updateAddressHashMap();

   auto iter = addressHashes_.find(scrAddr);
   if (iter == addressHashes_.end())
      throw AccountException("unknown scrAddr");

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
void AddressAccount::updateAddressHashMap()
{
   ReentrantLock lock(this);

   for (auto account : assetAccounts_)
   {
      auto& hashMap = account.second->getAddressHashMap(addressTypes_);

      for (auto& assetHash : hashMap)
      {
         for (auto& hash : assetHash.second)
         {
            addressHashes_.insert(make_pair(hash.second, assetHash.first));
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
const map<BinaryData, BinaryData>& AddressAccount::getAddressHashMap()
{
   updateAddressHashMap();
   return addressHashes_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetAccount> AddressAccount::getOuterAccount() const
{
   auto iter = assetAccounts_.find(outerAccount_);
   if (iter == assetAccounts_.end())
      throw AccountException("invalid outer account ID");

   return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
const map<BinaryData, shared_ptr<AssetAccount>>& 
   AddressAccount::getAccountMap() const
{
   return assetAccounts_;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry> AddressAccount::getOutterAssetForIndex(unsigned id) const
{
   auto account = getOuterAccount();
   return account->getAssetForIndex(id);
}
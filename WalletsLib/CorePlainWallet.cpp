#include "CorePlainWallet.h"
#include <spdlog/spdlog.h>
#include "SystemFileUtils.h"
#include "Wallets.h"

using namespace bs::core;

#define WALLET_PREFIX_BYTE    0x01     // can use as format version


PlainWallet::PlainWallet(NetworkType netType, const std::string &name, const std::string &desc
                         , const std::shared_ptr<spdlog::logger> &logger)
   : Wallet(logger), desc_(desc), netType_(netType)
{
   walletName_ = name;
   walletId_ = wallet::computeID(CryptoPRNG::generateRandom(32)).toBinStr();
}

PlainWallet::PlainWallet(NetworkType netType, const std::string &filename
                         , const std::shared_ptr<spdlog::logger> &logger)
   : Wallet(logger), netType_(netType)
{
   loadFromFile(filename);
}

PlainWallet::PlainWallet(NetworkType netType, const std::shared_ptr<spdlog::logger> &logger)
   : Wallet(logger), netType_(netType)
{
   walletId_ = wallet::computeID(CryptoPRNG::generateRandom(32)).toBinStr();
}

PlainWallet::~PlainWallet()
{
   if (db_) {
      db_->close();
      dbEnv_->close();
   }
}

void PlainWallet::openDBEnv(const std::string &filename)
{
   dbEnv_ = std::make_shared<LMDBEnv>(2);
   dbEnv_->open(filename);
   dbFilename_ = filename;
}

BinaryDataRef PlainWallet::getDataRefForKey(const std::shared_ptr<LMDB> &db, const BinaryData& key) const
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   auto ref = db->get_NoCopy(keyRef);

   if (ref.data == nullptr) {
      throw NoEntryInWalletException();
   }

   BinaryRefReader brr((const uint8_t*)ref.data, ref.len);
   auto len = brr.get_var_int();
   if (len != brr.getSizeRemaining()) {
      throw WalletException("on disk data length mismatch: "
         + std::to_string(len) + ", " + std::to_string(brr.getSizeRemaining()));
   }
   return brr.get_BinaryDataRef((uint32_t)brr.getSizeRemaining());
}

BinaryDataRef PlainWallet::getDataRefForKey(uint32_t key) const
{
   BinaryWriter bwKey;
   bwKey.put_uint32_t(key);
   return getDataRefForKey(db_, bwKey.getData());
}

void PlainWallet::putDataToDB(const std::shared_ptr<LMDB> &db, const BinaryData &key, const BinaryData &data)
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   CharacterArrayRef dataRef(data.getSize(), data.getPtr());
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db->insert(keyRef, dataRef);
}

void PlainWallet::writeDB()
{
   if (!dbEnv_) {
      return;     // diskless operation
   }
   if (!db_) {
      BinaryData masterID(walletId());
      if (masterID.isNull()) {
         throw std::invalid_argument("master ID is empty");
      }

      auto dbMeta = std::make_shared<LMDB>();
      dbMeta->open(dbEnv_.get(), WALLETMETA_DBNAME);
      {
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MASTERID_KEY);
         BinaryWriter bwData;
         bwData.put_var_int(masterID.getSize());

         BinaryDataRef idRef;
         idRef.setRef(masterID);
         bwData.put_BinaryDataRef(idRef);

         putDataToDB(dbMeta, bwKey.getData(), bwData.getData());
      }

      {
         BinaryWriter bwKey;
         bwKey.put_uint8_t(WALLETMETA_PREFIX);
         bwKey.put_BinaryData(masterID);

         BinaryWriter bw;
         bw.put_var_int(sizeof(uint8_t));
         bw.put_uint8_t(WALLET_PREFIX_BYTE);
         putDataToDB(dbMeta, bwKey.getData(), bw.getData());
      }
      dbMeta->close();

      db_ = std::make_shared<LMDB>(dbEnv_.get(), BS_WALLET_DBNAME);

      {
         BinaryWriter bwKey;
         bwKey.put_uint32_t(WALLETNAME_KEY);

         BinaryData walletNameData = walletName_;
         BinaryWriter bwName;
         bwName.put_var_int(walletNameData.getSize());
         bwName.put_BinaryData(walletNameData);
         putDataToDB(db_, bwKey.getData(), bwName.getData());
      }
      {
         BinaryWriter bwKey;
         bwKey.put_uint32_t(WALLETDESCRIPTION_KEY);

         BinaryData walletDescriptionData = desc_;
         BinaryWriter bwDesc;
         bwDesc.put_var_int(walletDescriptionData.getSize());
         bwDesc.put_BinaryData(walletDescriptionData);
         putDataToDB(db_, bwKey.getData(), bwDesc.getData());
      }
   }

   {  // asset count
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(4);
      bwData.put_int32_t(lastAssetIndex_);

      putDataToDB(db_, bwKey.getData(), bwData.getData());
   }

   for (const auto &asset : assets_) {
      if (!asset.second->needsCommit()) {
         continue;
      }
      BinaryWriter bwKey;
      bwKey.put_uint8_t(ASSETENTRY_PREFIX);
      bwKey.put_int32_t(asset.second->id());

      BinaryWriter bwData;
      const auto &assetSer = asset.second->serialize();
      bwData.put_var_int(assetSer.getSize());
      bwData.put_BinaryData(assetSer);

      putDataToDB(db_, bwKey.getData(), bwData.getData());
      asset.second->doNotCommit();
   }
}

void PlainWallet::openDB()
{
   db_ = std::make_shared<LMDB>(dbEnv_.get(), BS_WALLET_DBNAME);
}

void PlainWallet::readFromDB()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);

   try {
      walletName_ = getDataRefForKey(WALLETNAME_KEY).toBinStr();
   }
   catch (const NoEntryInWalletException &) {
      throw WalletException("wallet name not set");
   }
   try {
      desc_ = getDataRefForKey(WALLETDESCRIPTION_KEY).toBinStr();
   }
   catch (const NoEntryInWalletException&) {}

   {  // asset index
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);
      BinaryRefReader brr(getDataRefForKey(db_, bwKey.getData()));
      lastAssetIndex_ = brr.get_int32_t();
   }

   {  // assets
      auto dbIter = db_->begin();
      BinaryWriter bwKey;
      bwKey.put_uint8_t(ASSETENTRY_PREFIX);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());
      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid()) {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);
         if (keyBDR.getSize() != (sizeof(uint8_t) + sizeof(int32_t))) {
            dbIter.advance();
            continue;
         }
         BinaryRefReader brrKey(keyBDR);
         BinaryRefReader brrVal(valueBDR);

         const auto prefix = brrKey.get_uint8_t();
         if (prefix != ASSETENTRY_PREFIX) {
            dbIter.advance();
            continue;
         }
         const auto id = brrKey.get_int32_t();

         try {
            const auto &len = static_cast<uint32_t>(brrVal.get_var_int());
            if (len == 0) {
               throw WalletException("empty asset");
               break;
            }
            if (len != brrVal.getSizeRemaining()) {
               throw WalletException("invalid asset length");
               break;
            }
            const auto assetRef = brrVal.get_BinaryDataRef(len);
            const auto &assetPair = deserializeAsset(assetRef);
            if (assets_.find(assetPair.second->id()) != assets_.end()) {
               throw WalletException("index " + std::to_string(assetPair.second->id()) + " already exists");
            }
            if (assetPair.second->id() != id) {
               throw WalletException("id check failed: " + std::to_string(assetPair.second->id()) + " vs " + std::to_string(id));
            }
            addAddress(assetPair.first, assetPair.second);
         }
         catch (const std::exception &e) {
            throw WalletException(std::string("failed to deser asset: ") + e.what());
         }
         dbIter.advance();
      }
   }
   MetaData::readFromDB(dbEnv_, db_.get());
}

void PlainWallet::loadFromFile(const std::string &filename)
{
   if (filename.empty()) {
      throw std::invalid_argument("no file name provided");
   }
   if (!SystemFileUtils::isValidFilePath(filename)) {
      throw std::invalid_argument("Invalid file path: " + filename);
   }

   if (!SystemFileUtils::fileExist(filename)) {
      throw std::runtime_error("Wallet file does not exist");
   }

   openDBEnv(filename);
   openDB();
   readFromDB();
}

void PlainWallet::saveToFile(const std::string &filename)
{
   openDBEnv(filename);
   writeDB();
}

std::string PlainWallet::getFileName(const std::string &dir) const
{
   return (dir + "/" + fileNamePrefix(isWatchingOnly()) + walletId() + "_wallet.lmdb");
}

void PlainWallet::saveToDir(const std::string &targetDir)
{
   if (!SystemFileUtils::pathExist(targetDir)) {
      SystemFileUtils::mkPath(targetDir);
   }
   const auto masterID = BinaryData(walletId());
   saveToFile(getFileName(targetDir));
}

int PlainWallet::addAddress(const bs::Address &addr, const std::shared_ptr<GenericAsset> &inAsset)
{
   auto asset = inAsset;
   int id = 0;
   if (asset) {
      if (asset->id() < 0) {
         id = lastAssetIndex_++;
      }
      else {
         id = asset->id();
      }
      asset->setId(id);
   }
   else {
      id = lastAssetIndex_++;
      asset = std::make_shared<PlainAsset>(id, addr);
   }
   assets_[id] = asset;
   assetByAddr_[addr] = asset;
   usedAddresses_.push_back(addr);
   addressHashes_.insert(addr.unprefixed());
   return id;
}

std::shared_ptr<AddressEntry> PlainWallet::getAddressEntryForAddr(const BinaryData &addr)
{
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return nullptr;
   }
   const auto plainAsset = std::dynamic_pointer_cast<PlainAsset>(itAsset->second);
   if (!plainAsset) {
      return nullptr;
   }
   SecureBinaryData privKeyBin = plainAsset->privKey().copy();
   const auto privKey = std::make_shared<Asset_PrivateKey>(BinaryData{}, privKeyBin
      , std::make_unique<Cipher_AES>(BinaryData{}, BinaryData{}));
   SecureBinaryData pubKey = plainAsset->publicKey();
   const auto assetEntry = std::make_shared<AssetEntry_Single>(plainAsset->id(), BinaryData{}, pubKey, privKey);

   std::shared_ptr<AddressEntry> result;
   switch (plainAsset->address().getType()) {
   case AddressEntryType_P2PKH:
      result = std::make_shared<AddressEntry_P2PKH>(assetEntry, true);
      break;
   case AddressEntryType_P2SH: {
         const auto nested = std::make_shared<AddressEntry_P2PKH>(assetEntry, true);
         result = std::make_shared<AddressEntry_P2SH>(nested);
      }
      break;
   case AddressEntryType_P2WPKH:
      result = std::make_shared<AddressEntry_P2WPKH>(assetEntry);
      break;
   case AddressEntryType_P2WSH: {
         const auto nested = std::make_shared<AddressEntry_P2WPKH>(assetEntry);
         result = std::make_shared<AddressEntry_P2WSH>(nested);
      }
      break;
   default:
      return nullptr;
   }

   return result;
}

int PlainWallet::addressIndex(const bs::Address &addr) const
{
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return -1;
   }
   return itAsset->second->id();
}

std::string PlainWallet::getAddressIndex(const bs::Address &addr)
{
   const auto index = addressIndex(addr);
   if (index < 0) {
      return {};
   }
   return std::to_string(index);
}

bool PlainWallet::addressIndexExists(const std::string &index) const
{
   if (index.empty()) {
      return false;
   }
   const auto id = std::stoi(index);
   return (assets_.find(id) != assets_.end());
}

bool PlainWallet::containsAddress(const bs::Address &addr)
{
   const auto itAsset = assetByAddr_.find(addr);
   return (itAsset != assetByAddr_.end());
}

SecureBinaryData PlainWallet::getPublicKeyFor(const bs::Address &addr)
{
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return {};
   }
   const auto plainAsset = std::dynamic_pointer_cast<PlainAsset>(itAsset->second);
   if (!plainAsset) {
      return {};
   }
   return plainAsset->publicKey();
}

KeyPair PlainWallet::getKeyPairFor(const bs::Address &addr, const SecureBinaryData &)
{
   const auto &itAsset = assetByAddr_.find(addr);
   if (itAsset == assetByAddr_.end()) {
      return {};
   }
   const auto plainAsset = std::dynamic_pointer_cast<PlainAsset>(itAsset->second);
   if (!plainAsset) {
      return {};
   }
   return { plainAsset->privKey(), plainAsset->publicKey() };
}

void PlainWallet::shutdown()
{
   if (db_ != nullptr)
   {
      db_->close();
      db_ = nullptr;
   }

   if (dbEnv_ != nullptr)
   {
      dbEnv_->close();
      dbEnv_ = nullptr;
   }
}

////////////////////////////////////////////////////////////////////////////////

class PlainResolver : public ResolverFeed
{
public:
   PlainResolver(const std::map<bs::Address, std::shared_ptr<GenericAsset>> &map) {
      for (const auto &addrPair : map) {
         const auto plainAsset = std::dynamic_pointer_cast<PlainAsset>(addrPair.second);
         if (!plainAsset) {
            continue;
         }
         hashToPub_[addrPair.first.unprefixed()] = plainAsset->publicKey();
      }
   }

   BinaryData getByVal(const BinaryData& key) override {
      const auto itKey = hashToPub_.find(key);
      if (itKey == hashToPub_.end()) {
         throw std::runtime_error("hash not found");
      }
      return itKey->second;
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData&) override {
      throw std::runtime_error("no privkey");
      return {};
   }

private:
   std::map<BinaryData, SecureBinaryData> hashToPub_;
};

class PlainSigningResolver : public PlainResolver
{
public:
   PlainSigningResolver(const std::map<bs::Address, std::shared_ptr<GenericAsset>> &map)
      : PlainResolver(map) {
      for (const auto &addrPair : map) {
         const auto plainAsset = std::dynamic_pointer_cast<PlainAsset>(addrPair.second);
         if (!plainAsset || plainAsset->privKey().isNull()) {
            continue;
         }
         pubToPriv_[plainAsset->publicKey()] = plainAsset->privKey();
      }
   }

   const SecureBinaryData& getPrivKeyForPubkey(const BinaryData& pubkey) override {
      const auto &itKey = pubToPriv_.find(pubkey);
      if (itKey == pubToPriv_.end()) {
         throw std::runtime_error("no pubkey found");
      }
      return itKey->second;
   }

private:
   std::map<SecureBinaryData, SecureBinaryData>   pubToPriv_;
};

std::shared_ptr<ResolverFeed> PlainWallet::getResolver() const
{
   if (isWatchingOnly()) {
      return nullptr;
   }
   return std::make_shared<PlainSigningResolver>(assetByAddr_);
}

////////////////////////////////////////////////////////////////////////////////

std::pair<bs::Address, std::shared_ptr<PlainAsset>> PlainAsset::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   const auto assetType = static_cast<AssetEntryType>(brrVal.get_uint8_t());
   if (assetType != AssetEntryType_Single) {
      throw std::runtime_error("Unable to handle other asset types except "
         + std::to_string(AssetEntryType_Single) + " (current " + std::to_string(assetType) + ")");
   }
   const auto id = brrVal.get_int32_t();

   std::unordered_map<uint8_t, BinaryRefReader> values;
   while (brrVal.getSizeRemaining() > 0) {
      const auto len = brrVal.get_var_int();
      const auto valRef = brrVal.get_BinaryDataRef(len);
      BinaryRefReader brrData(valRef);
      const auto key = brrData.get_uint8_t();
      values[key] = brrData;
   }

   bs::Address addr;
   if (values.find(ENCRYPTIONKEY_BYTE) != values.end()) {
      auto brrData = values[ENCRYPTIONKEY_BYTE];
      addr = bs::Address(BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining())));
   }
   if (addr.isNull() || !addr.isValid()) {
      throw std::runtime_error("Failed to read address");
   }

   SecureBinaryData privKey;
   if (values.find(PRIVKEY_BYTE) != values.end()) {
      auto brrData = values[PRIVKEY_BYTE];
      privKey = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining())).toBinStr();
   }

   auto asset = std::make_shared<PlainAsset>(id, addr, privKey);
   asset->doNotCommit();
   return { addr, asset };
}

BinaryData PlainAsset::serialize(void) const
{
   BinaryWriter bw;
   bw.put_uint8_t(static_cast<uint8_t>(getType()));
   bw.put_int32_t(id_);

   BinaryData addrKey(address_.id());
   bw.put_var_int(addrKey.getSize() + 1);
   bw.put_uint8_t(ENCRYPTIONKEY_BYTE);
   bw.put_BinaryData(addrKey);

   bw.put_var_int(privKey_.getSize() + 1);
   bw.put_uint8_t(PRIVKEY_BYTE);
   bw.put_BinaryData(privKey_);

   return bw.getData();
}

SecureBinaryData PlainAsset::publicKey() const
{
   if (pubKey_.isNull()) {
      pubKey_ = CryptoECDSA().ComputePublicKey(privKey_);
   }
   return pubKey_;
}

std::pair<bs::Address, std::shared_ptr<PlainAsset>> PlainAsset::generateRandom(AddressEntryType addrType)
{
   const auto &privKey = CryptoPRNG::generateRandom(32);
   const auto &pubKey = CryptoECDSA().ComputePublicKey(privKey);
   BinaryData hash;
   switch (addrType) {
   case AddressEntryType_P2PKH:
   case AddressEntryType_P2WPKH:
      hash = BtcUtils::getHash160(pubKey);
      break;
   case AddressEntryType_P2SH:
   case AddressEntryType_P2WSH:
      hash = BtcUtils::getHash256(pubKey);
      break;
   default:
      throw std::invalid_argument("Unknown address type");
   }
   const bs::Address addr(hash, addrType);
   return { addr, std::make_shared<PlainAsset>(0, addr, privKey) };
}

#include "CoreHDWallet.h"
#include <bech32/ref/c++/segwit_addr.h>
#include <spdlog/spdlog.h>
#include "SystemFileUtils.h"
#include "Wallets.h"


#define LOG(logger, method, ...) \
if ((logger)) { \
   logger->method(__VA_ARGS__); \
}


using namespace bs::core;

hd::Wallet::Wallet(const std::string &name, const std::string &desc
                   , const wallet::Seed &seed
                   , const SecureBinaryData& passphrase
                   , const std::string& folder
                   , const std::shared_ptr<spdlog::logger> &logger)
   : name_(name), desc_(desc)
   , netType_(seed.networkType())
   , logger_(logger)
{
   initNew(seed, passphrase, folder);
}

hd::Wallet::Wallet(const std::string &filename, NetworkType netType,
   const std::string& folder, const std::shared_ptr<spdlog::logger> &logger)
   : netType_(netType), logger_(logger)
{
   loadFromFile(filename, folder);
}

hd::Wallet::Wallet(const std::string &name, const std::string &desc
   , NetworkType netType, const SecureBinaryData& passphrase
   , const std::string& folder
   , const std::shared_ptr<spdlog::logger> &logger)
   : name_(name), desc_(desc)
   , netType_(netType)
   , logger_(logger)
{
   wallet::Seed seed(CryptoPRNG::generateRandom(32), netType);
   initNew(seed, passphrase, folder);
}

hd::Wallet::~Wallet()
{
   shutdown();
}

void hd::Wallet::initNew(const wallet::Seed &seed, 
   const SecureBinaryData& passphrase, const std::string& folder)
{
   try
   {
      walletPtr_ = AssetWallet_Single::createFromSeed_BIP32_Blank(
         folder, seed.seed(), passphrase);
   }
   catch(WalletException&)
   {
      //empty account structure, will be set at group creation
      std::set<std::shared_ptr<AccountType>> accountTypes;
      
      auto& node = seed.getNode();
      if (node.getPrivateKey().getSize() != 32 &&
         node.getPublicKey().getSize() != 33)
         throw WalletException("invalid seed node");

      walletPtr_ = AssetWallet_Single::createFromBIP32Node(
         seed.getNode(),
         accountTypes,
         passphrase,
         folder,
         0); //no lookup, as there are no accounts
   }

   dbEnv_ = walletPtr_->getDbEnv();
   db_ = new LMDB(dbEnv_.get(), BS_WALLET_DBNAME);
   initializeDB();
}

void hd::Wallet::loadFromFile(
   const std::string &filename, 
   const std::string& folder)
{
   auto fullname = folder;
   DBUtils::appendPath(fullname, filename);
   if (!SystemFileUtils::isValidFilePath(fullname)) {
      throw std::invalid_argument(std::string("Invalid file path: ") + fullname);
   }
   if (!SystemFileUtils::fileExist(fullname)) {
      throw std::runtime_error("Wallet file " + fullname + " does not exist");
   }

   //load armory wallet
   auto walletPtr = AssetWallet::loadMainWalletFromFile(fullname);
   walletPtr_ = std::dynamic_pointer_cast<AssetWallet_Single>(walletPtr);
   if (walletPtr_ == nullptr)
      throw WalletException("failed to load wallet");

   //setup bs wallet db object. the bs wallet custom data exists in its own
   //db name, isolated from the armory content.
   dbEnv_ = walletPtr_->getDbEnv();
   db_ = new LMDB(dbEnv_.get(), BS_WALLET_DBNAME);

   readFromDB();
}

std::vector<std::shared_ptr<hd::Group>> hd::Wallet::getGroups() const
{
   std::vector<std::shared_ptr<hd::Group>> result;
   result.reserve(groups_.size());
   for (const auto &group : groups_) {
      result.emplace_back(group.second);
   }
   return result;
}

size_t hd::Wallet::getNumLeaves() const
{
   size_t result = 0;
   for (const auto &group : groups_) {
      result += group.second->getNumLeaves();
   }
   return result;
}

std::vector<std::shared_ptr<hd::Leaf>> hd::Wallet::getLeaves() const
{
   std::vector<std::shared_ptr<hd::Leaf>> leaves;
   for (const auto &group : groups_) {
      const auto &groupLeaves = group.second->getAllLeaves();
      for (const auto &leaf : groupLeaves) 
         leaves.push_back(leaf);
   }

   return leaves;
}

std::shared_ptr<hd::Leaf> hd::Wallet::getLeaf(const std::string &id) const
{
   for (const auto &group : groups_) {
      auto leafPtr = group.second->getLeafById(id);
      if (leafPtr != nullptr)
         return leafPtr;
   }

   return nullptr;
}

std::shared_ptr<hd::Group> hd::Wallet::createGroup(bs::hd::CoinType ct)
{
   std::shared_ptr<Group> result;
   result = getGroup(ct);
   if (result) {
      return result;
   }

   const bs::hd::Path path({ bs::hd::purpose, ct });
   switch (ct) {
   case bs::hd::CoinType::BlockSettle_Auth:
      result = std::make_shared<AuthGroup>(
         walletPtr_, path, netType_, logger_);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<CCGroup>(
         walletPtr_, path, netType_, logger_);
      break;

   case bs::hd::CoinType::BlockSettle_Settlement:
      result = std::make_shared<SettlementGroup>(
         walletPtr_, path, netType_, logger_);
      break;

   default:
      result = std::make_shared<Group>(
         walletPtr_, path, netType_, extOnlyFlag_, logger_);
      break;
   }
   addGroup(result);
   writeGroupsToDB();
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   groups_[group->index()] = group;
}

std::shared_ptr<hd::Group> hd::Wallet::getGroup(bs::hd::CoinType ct) const
{
   const auto &itGroup = groups_.find(static_cast<bs::hd::Path::Elem>(ct));
   if (itGroup == groups_.end()) {
      return nullptr;
   }
   return itGroup->second;
}

void hd::Wallet::createStructure(unsigned lookup)
{
   const auto groupXBT = createGroup(getXBTGroupType());
   groupXBT->createLeaf(0u, lookup);
   writeGroupsToDB();
}

void hd::Wallet::shutdown()
{
   for (auto& group : groups_)
      group.second->shutdown();
   groups_.clear();

   if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
   }
   dbEnv_.reset();

   if (walletPtr_ != nullptr)
   {
      walletPtr_->shutdown();
      walletPtr_.reset();
   }
}

bool hd::Wallet::eraseFile()
{
   auto fname = getFileName();
   shutdown();

   if (fname.size() == 0)
      return true;

   bool rc = true;
   if (std::remove(fname.c_str()) != 0)
      rc = false;

   fname.append("-lock");
   if (std::remove(fname.c_str()) != 0)
      rc = false;

   return rc;
}

const std::string& hd::Wallet::getFileName() const
{
   if (walletPtr_ == nullptr)
      throw WalletException("wallet is not initialized, cannot return filename");
   return walletPtr_->getDbFilename();
}

void hd::Wallet::initializeDB()
{
   //commit bs header data
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {  //network type
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETTYPE_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(1);
      bwData.put_uint8_t(static_cast<uint8_t>(netType_));

      putDataToDB(bwKey.getData(), bwData.getData());
   }

   {  //name
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETNAME_KEY);

      BinaryData walletNameData = name_;
      BinaryWriter bwName;
      bwName.put_var_int(walletNameData.getSize());
      bwName.put_BinaryData(walletNameData);
      putDataToDB(bwKey.getData(), bwName.getData());
   }
   {  //description
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETDESCRIPTION_KEY);

      BinaryData walletDescriptionData = desc_;
      BinaryWriter bwDesc;
      bwDesc.put_var_int(walletDescriptionData.getSize());
      bwDesc.put_BinaryData(walletDescriptionData);
      putDataToDB(bwKey.getData(), bwDesc.getData());
   }

   {  //ext only flag
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLET_EXTONLY_KEY);

      BinaryWriter bwDesc;
      bwDesc.put_uint8_t(1); //flag size
      bwDesc.put_uint8_t(extOnlyFlag_);
      putDataToDB(bwKey.getData(), bwDesc.getData());
   }
}

void hd::Wallet::readFromDB()
{
   //this needs to be a readwrite because initializing a leaf results 
   //in opening a db name
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   {  //header data
      auto typeBdr = getDataRefForKey(WALLETTYPE_KEY);
      if (typeBdr.getSize() != 1) 
         throw WalletException("invalid netType length");
      netType_ = static_cast<NetworkType>(typeBdr.getPtr()[0]);

      name_ = getDataRefForKey(WALLETNAME_KEY).toBinStr();
      desc_ = getDataRefForKey(WALLETDESCRIPTION_KEY).toBinStr();
      extOnlyFlag_ = (bool)*getDataRefForKey(WALLET_EXTONLY_KEY).getPtr();
   }

   {  // groups
      auto dbIter = db_->begin();

      BinaryWriter bwKey;
      bwKey.put_uint8_t(BS_GROUP_PREFIX);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);
      while (dbIter.isValid()) {
         
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

         //sanity check on the key
         if (keyBDR.getSize() == 0 || keyBDR.getPtr()[0] != BS_GROUP_PREFIX)
            break;

         BinaryRefReader brrVal(valueBDR);
         auto valsize = brrVal.get_var_int();
         if (valsize != brrVal.getSizeRemaining())
            throw WalletException("entry val size mismatch");
         
         try {
            const auto group = hd::Group::deserialize(walletPtr_,
               keyBDR, brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining())
                 , name_, desc_, netType_, logger_);
            if (group != nullptr)
               addGroup(group);
         }
         catch (const std::exception&) 
         {}

         dbIter.advance();
      }
   }
   for (const auto &leaf : getLeaves()) {
      leaf->readMetaData();
   }
}

void hd::Wallet::writeGroupsToDB(bool force)
{
   for (const auto &group : groups_)
      group.second->commit(force);
}

BinaryDataRef hd::Wallet::getDataRefForKey(LMDB* db, const BinaryData& key) const
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

BinaryDataRef hd::Wallet::getDataRefForKey(uint32_t key) const
{
   BinaryWriter bwKey;
   bwKey.put_uint32_t(key);
   return getDataRefForKey(db_, bwKey.getData());
}

void hd::Wallet::putDataToDB(const BinaryData& key, const BinaryData& data)
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   CharacterArrayRef dataRef(data.getSize(), data.getPtr());
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db_->insert(keyRef, dataRef);
}

std::string hd::Wallet::fileNamePrefix(bool watchingOnly)
{
   return watchingOnly ? "bip44wo_" : "bip44_";
}

std::shared_ptr<hd::Wallet> hd::Wallet::createWatchingOnly() const
{
   //fork WO copy of armory wallet
   auto woFilename = AssetWallet::forkWathcingOnly(walletPtr_->getDbFilename());

   //instantiate empty core::hd::Wallet
   std::shared_ptr<hd::Wallet> woCopy(new hd::Wallet());

   //populate with this wallet's meta data
   woCopy->name_ = name_;
   woCopy->desc_ = desc_;
   woCopy->netType_ = netType_;
   woCopy->logger_ = logger_;

   //setup the armory wallet ptr and dbs
   woCopy->walletPtr_ = std::dynamic_pointer_cast<AssetWallet_Single>(
      AssetWallet::loadMainWalletFromFile(woFilename));
   woCopy->dbEnv_ = woCopy->walletPtr_->getDbEnv();
   woCopy->db_ = new LMDB(woCopy->dbEnv_.get(), BS_WALLET_DBNAME);

   //init wo blocksettle meta data db
   woCopy->initializeDB();

   //copy group and leaf structure
   for (auto& groupPair : groups_)
   {
      auto newGroup = groupPair.second->getCopy(woCopy->walletPtr_);
      woCopy->addGroup(newGroup);
   }

   //commit to disk
   woCopy->writeGroupsToDB();

   return woCopy;
}

bool hd::Wallet::isWatchingOnly() const
{
   return walletPtr_->isWatchingOnly();
}

static bool nextCombi(std::vector<int> &a , const int n, const int m)
{
   for (int i = m - 1; i >= 0; --i) {
      if (a[i] < n - m + i) {
         ++a[i];
         for (int j = i + 1; j < m; ++j) {
            a[j] = a[j - 1] + 1;
         }
         return true;
      }
   }
   return false;
}

bool hd::Wallet::changePassword(const SecureBinaryData& newPass)
{
   if (newPass.getSize() == 0)
      return false;

   //we assume the wallet passphrase prompt lambda has been set
   auto lock = walletPtr_->lockDecryptedContainer();
   try
   {
      walletPtr_->changeMasterPassphrase(newPass);
      return true;
   }
   catch (std::exception&)
   {
      return false;
   }
}

bool hd::Wallet::isPrimary() const
{
   if ((getGroup(bs::hd::CoinType::BlockSettle_Auth) != nullptr)
      && (getGroup(getXBTGroupType()) != nullptr)) {
      return true;
   }
   return false;
}

void hd::Wallet::copyToFile(const std::string& filename)
{
   std::ifstream source(dbEnv_->getFilename(), std::ios::binary);
   std::ofstream dest(filename, std::ios::binary);

   std::istreambuf_iterator<char> begin_source(source);
   std::istreambuf_iterator<char> end_source;
   std::ostreambuf_iterator<char> begin_dest(dest);
   std::copy(begin_source, end_source, begin_dest);

   source.close();
   dest.close();
}

WalletEncryptionLock hd::Wallet::lockForEncryption(const SecureBinaryData& passphrase)
{
   return WalletEncryptionLock(walletPtr_, passphrase);
}

void hd::Wallet::setExtOnly()
{
   //no point going further if the flag is already set
   if (extOnlyFlag_)
      return;

   //cannot flag for ext only if the wallet already has a structure
   if (getNumLeaves() > 0)
      throw WalletException("cannot flag initialized wallet for ext only");
   
   extOnlyFlag_ = true;
   
   //update flag on disk
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);

   BinaryWriter bwKey;
   bwKey.put_uint32_t(WALLET_EXTONLY_KEY);

   BinaryWriter bwDesc;
   bwDesc.put_uint8_t(1); //flag size
   bwDesc.put_uint8_t(extOnlyFlag_);
   putDataToDB(bwKey.getData(), bwDesc.getData());
}

bs::core::wallet::Seed hd::Wallet::getDecryptedSeed(void) const
{
   /***
   Expects wallet to be locked and passphrase lambda set
   ***/

   if (walletPtr_ == nullptr)
      throw WalletException("uninitialized armory wallet");

   auto seedPtr = walletPtr_->getEncryptedSeed();
   if(seedPtr == nullptr)
      throw WalletException("wallet has no seed");

   auto clearSeed = walletPtr_->getDecryptedValue(seedPtr);
   bs::core::wallet::Seed rootObj(clearSeed, netType_);
   return rootObj;
}

SecureBinaryData hd::Wallet::getDecryptedRootXpriv(void) const
{
   /***
   Expects wallet to be locked and passphrase lambda set
   ***/

   if (walletPtr_ == nullptr)
      throw WalletException("uninitialized armory wallet");

   if(walletPtr_->isWatchingOnly())
      throw WalletException("wallet is watching only");

   auto root = walletPtr_->getRoot();
   if(!root->hasPrivateKey())
      throw WalletException("wallet is missing root private key, this shouldnt happen");

   auto rootBip32 = std::dynamic_pointer_cast<AssetEntry_BIP32Root>(root);
   if (rootBip32 == nullptr)
      throw WalletException("unexpected wallet root type");

   auto decryptedRootPrivKey = walletPtr_->getDecryptedPrivateKeyForAsset(root);
   
   BIP32_Node node;
   node.initFromPrivateKey(
      rootBip32->getDepth(), rootBip32->getLeafID(), rootBip32->getFingerPrint(),
      decryptedRootPrivKey, rootBip32->getChaincode());
   return node.getBase58();
}

std::shared_ptr<hd::Leaf> hd::Wallet::createSettlementLeaf(
   const bs::Address& addr)
{
   /*
   This method expects the wallet locked and passprhase lambda set 
   for a full wallet.
   */

   //does this wallet have a settlement group?
   auto group = getGroup(bs::hd::BlockSettle_Settlement);
   if (group == nullptr)
      group = createGroup(bs::hd::BlockSettle_Settlement);

   auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
   if (settlGroup == nullptr)
      throw AccountException("unexpected settlement group type");

   //get hd path for addr
   auto getPathForAddr = [this, &addr](void)->bs::hd::Path
   {
      for (auto& groupPair : groups_)
      {
         for (auto& leafPair : groupPair.second->leaves_)
         {
            auto path = leafPair.second->getPathForAddress(addr);
            if (path.length() != 0)
               return path;
         }
      }

      return {};
   };

   auto addrPath = getPathForAddr();
   if (addrPath.length() == 0)
      throw AssetException("failed to resolve path for settlement address");

   auto leaf = settlGroup->getLeafByPath(addrPath.get(-1));
   if (leaf) {
      return leaf;
   }
   return settlGroup->createLeaf(addr, addrPath);
}

std::shared_ptr<AssetEntry> hd::Wallet::getAssetForAddress(
   const bs::Address& addr)
{
   auto& idPair = walletPtr_->getAssetIDForAddr(addr.prefixed());
   return walletPtr_->getAssetForID(idPair.first);
}

bs::Address hd::Wallet::getSettlementPayinAddress(
   const bs::core::wallet::SettlementData &sd) const
{
   auto addrPtr = getAddressPtrForSettlement(
      sd.settlementId, sd.cpPublicKey, sd.ownKeyFirst);
   return bs::Address(addrPtr->getPrefixedHash());
}

std::shared_ptr<AddressEntry_P2WSH> hd::Wallet::getAddressPtrForSettlement(
   const SecureBinaryData& settlementID,
   const SecureBinaryData& counterPartyPubKey,
   bool isMyKeyFirst) const
{
   //get settlement leaf for id
   auto leafPtr = getLeafForSettlementID(settlementID);
   if (!leafPtr) {
      throw AssetException("failed to find leaf for settlement id " + settlementID.toHexStr());
   }

   //grab settlement asset from leaf
   auto index = leafPtr->getIndexForSettlementID(settlementID);
   auto myAssetPtr = leafPtr->accountPtr_->getAssetForID(index, true);
   auto myAssetSingle = std::dynamic_pointer_cast<AssetEntry_Single>(myAssetPtr);
   if (myAssetSingle == nullptr) {
      throw AssetException("unexpected asset type");
   }
   //salt counterparty pubkey
   auto&& counterPartySaltedKey = CryptoECDSA::PubKeyScalarMultiply(
      counterPartyPubKey, settlementID);

   //create counterparty asset
   auto counterPartyAsset = std::make_shared<AssetEntry_Single>(
      0, BinaryData(), counterPartySaltedKey, nullptr);

   //create ms asset
   std::map<BinaryData, std::shared_ptr<AssetEntry>> assetMap;

   if (isMyKeyFirst) {
      assetMap.insert(std::make_pair(READHEX("00"), myAssetSingle));
      assetMap.insert(std::make_pair(READHEX("01"), counterPartyAsset));
   }
   else {
      assetMap.insert(std::make_pair(READHEX("00"), counterPartyAsset));
      assetMap.insert(std::make_pair(READHEX("01"), myAssetSingle));
   }

   auto assetMs = std::make_shared<AssetEntry_Multisig>(
      0, BinaryData(), assetMap, 1, 2);

   //create ms address
   auto addrMs = std::make_shared<AddressEntry_Multisig>(assetMs, true);

   //nest it
   auto addrP2wsh = std::make_shared<AddressEntry_P2WSH>(addrMs);

   //done
   return addrP2wsh;
}

std::shared_ptr<hd::SettlementLeaf> hd::Wallet::getLeafForSettlementID(
   const SecureBinaryData& id) const
{
   auto group = getGroup(bs::hd::CoinType::BlockSettle_Settlement);
   auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
   if (settlGroup == nullptr)
      throw AccountException("missing settlement group");

   return settlGroup->getLeafForSettlementID(id);
}

BinaryData hd::Wallet::signSettlementTXRequest(const wallet::TXSignRequest &txReq
   , const wallet::SettlementData &sd)
{
   //get p2wsh address entry
   auto addrP2wsh = getAddressPtrForSettlement(
      sd.settlementId, sd.cpPublicKey, sd.ownKeyFirst);

   //grab wallet resolver, seed it with p2wsh script
   auto resolver = 
      std::make_shared<ResolverFeed_AssetWalletSingle>(walletPtr_);
   resolver->seedFromAddressEntry(addrP2wsh);

   //get leaf
   auto leafPtr = getLeafForSettlementID(sd.settlementId);

   //sign & return
   auto signer = leafPtr->getSigner(txReq, false);
   signer.resetFeeds();
   signer.setFeed(resolver);

   signer.sign();
   return signer.serialize();
}

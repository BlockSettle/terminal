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
                   , const std::shared_ptr<spdlog::logger> &logger
                   , bool extOnlyAddresses)
   : name_(name), desc_(desc)
   , netType_(seed.networkType())
   , extOnlyAddresses_(extOnlyAddresses)
   , logger_(logger)
{
   initNew(seed);
}

hd::Wallet::Wallet(const std::string &filename
                   , const std::shared_ptr<spdlog::logger> &logger
                   , bool extOnlyAddresses)
   : extOnlyAddresses_(extOnlyAddresses)
   , logger_(logger)
{
   loadFromFile(filename);
}

hd::Wallet::Wallet(const std::string &walletId, NetworkType netType
                   , bool extOnlyAddresses, const std::string &name
                   , const std::shared_ptr<spdlog::logger> &logger
                   , const std::string &desc)
   : walletId_(walletId), name_(name), desc_(desc)
   , netType_(netType)
   , extOnlyAddresses_(extOnlyAddresses)
   , logger_(logger)
{}

void hd::Wallet::initNew(const wallet::Seed &seed)
{
   const auto &rootNode = std::make_shared<hd::Node>(seed);
   walletId_ = rootNode->getId();
   rootNodes_ = hd::Nodes({ rootNode }, {0, 0}, walletId_);
}

void hd::Wallet::loadFromFile(const std::string &filename)
{
   if (!SystemFileUtils::isValidFilePath(filename)) {
      throw std::invalid_argument(std::string("Invalid file path: ") + filename);
   }
   if (!SystemFileUtils::fileExist(filename)) {
      throw std::runtime_error("Wallet file does not exist");
   }

   openDBEnv(filename);
   openDB();
   setDBforDependants();
   readFromDB();
}

hd::Wallet::~Wallet()
{
   if (db_) {
      db_->close();
      dbEnv_->close();
      delete db_;
   }
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

std::vector<std::shared_ptr<bs::core::Wallet>> hd::Wallet::getLeaves() const
{
   const auto nbLeaves = getNumLeaves();
   if (leaves_.size() != nbLeaves) {
      leaves_.clear();
      for (const auto &group : groups_) {
         const auto &groupLeaves = group.second->getAllLeaves();
         for (const auto &leaf : groupLeaves) {
            leaves_[leaf->walletId()] = leaf;
         }
      }
   }

   std::vector<std::shared_ptr<bs::core::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<bs::core::Wallet> hd::Wallet::getLeaf(const std::string &id) const
{
   const auto &itLeaf = leaves_.find(id);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
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
      result = std::make_shared<AuthGroup>(rootNodes_, path, name_, desc_
                                           , logger_, extOnlyAddresses_);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<CCGroup>(rootNodes_, path, name_, desc_, logger_
                                         , extOnlyAddresses_);
      break;

   default:
      result = std::make_shared<Group>(rootNodes_, path, name_, desc_, logger_
         , extOnlyAddresses_);
      break;
   }
   addGroup(result);
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   group->setDB(dbEnv_, db_);
   if (!chainCode_.isNull()) {
      group->setChainCode(chainCode_);
   }
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

void hd::Wallet::createStructure()
{
   const auto groupXBT = createGroup(getXBTGroupType());
   groupXBT->createLeaf(0u);
}

void hd::Wallet::setChainCode(const BinaryData &chainCode)
{
   chainCode_ = chainCode;

   for (const auto &group : groups_) {
      group.second->setChainCode(chainCode);
   }
}

std::string hd::Wallet::getFileName(const std::string &dir) const
{
   return (dir + "/" + fileNamePrefix(isWatchingOnly()) + walletId() + "_wallet.lmdb");
}

void hd::Wallet::saveToDir(const std::string &targetDir)
{
   const auto masterID = BinaryData(walletId());
   saveToFile(getFileName(targetDir));
}

bool hd::Wallet::eraseFile()
{
   if (dbFilename_.empty()) {
      return true;
   }
   if (dbEnv_) {
      db_->close();
      dbEnv_->close();
      delete db_;
      db_ = nullptr;
   }
   bool rc = true;
   if (SystemFileUtils::fileExist(dbFilename_)) {
      rc &= SystemFileUtils::rmFile(dbFilename_);
      rc &= SystemFileUtils::rmFile(dbFilename_ + "-lock");
   }
   return rc;
}

void hd::Wallet::saveToFile(const std::string &filename, bool force)
{
   openDBEnv(filename);
   initDB();
   setDBforDependants();
   writeToDB(force);
}

void hd::Wallet::copyToFile(const std::string &filename)
{
   const auto prevDbFilename = dbFilename_;
   const auto prevDbEnv = dbEnv_;
   const auto prevDb = db_;
   saveToFile(filename, true);
   dbEnv_ = prevDbEnv;
   db_ = prevDb;
   dbFilename_ = prevDbFilename;
   setDBforDependants();
}

void hd::Wallet::openDBEnv(const std::string &filename)
{
   dbEnv_ = std::make_shared<LMDBEnv>(2);
   dbEnv_->open(filename);
   dbFilename_ = filename;
}

void hd::Wallet::initDB()
{
   BinaryData masterID(walletId());
   if (masterID.isNull()) {
      throw std::invalid_argument("master ID is empty");
   }

   LMDB dbMeta;
   dbMeta.open(dbEnv_.get(), WALLETMETA_DBNAME);
   {
      BinaryWriter bwKey;
      bwKey.put_uint32_t(MASTERID_KEY);
      BinaryWriter bwData;
      bwData.put_var_int(masterID.getSize());

      BinaryDataRef idRef;
      idRef.setRef(masterID);
      bwData.put_BinaryDataRef(idRef);

      putDataToDB(&dbMeta, bwKey.getData(), bwData.getData());
   }

   const auto groupXBT = getGroup(getXBTGroupType());
   if (groupXBT != nullptr) {
      const auto leafMain = groupXBT->getLeaf(0);
      if (leafMain != nullptr) {
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MAINWALLET_KEY);

         const auto mainWalletId = leafMain->walletId();
         BinaryWriter bwData;
         bwData.put_var_int(mainWalletId.size());
         bwData.put_BinaryData(mainWalletId);
         putDataToDB(&dbMeta, bwKey.getData(), bwData.getData());
      }
   }

   {
      BinaryWriter bwKey;
      bwKey.put_uint8_t(WALLETMETA_PREFIX);
      bwKey.put_BinaryData(masterID);

      BinaryWriter bw;
      bw.put_var_int(sizeof(uint32_t));
      bw.put_uint32_t(bs::hd::purpose);
      putDataToDB(&dbMeta, bwKey.getData(), bw.getData());
   }
   dbMeta.close();

   db_ = new LMDB(dbEnv_.get(), masterID.toBinStr());

   {  //wallet type
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETTYPE_KEY);

      BinaryWriter bwData;
      bwData.put_var_int(1);
      bwData.put_uint8_t(static_cast<uint8_t>(netType_));

      putDataToDB(db_, bwKey.getData(), bwData.getData());
   }

   if (!rootNodes_.empty()) {
      BinaryWriter bwKey, bwData;
      bwKey.put_uint32_t(MAIN_ACCOUNT_KEY);
      bwData.put_var_int(sizeof(uint32_t) * 2);
      bwData.put_uint32_t(rootNodes_.rank().first);
      bwData.put_uint32_t(rootNodes_.rank().second);
      putDataToDB(db_, bwKey.getData(), bwData.getData());
   }

   uint16_t nodeCounter = 0;
   const auto &cbNode = [this, &nodeCounter](const std::shared_ptr<hd::Node> &node) {
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);
      bwKey.put_uint16_t(nodeCounter++);

      BinaryWriter bwData;
      const auto &nodeSer = node->serialize();
      bwData.put_var_int(nodeSer.getSize());
      bwData.put_BinaryData(nodeSer);

      putDataToDB(db_, bwKey.getData(), bwData.getData());
   };
   rootNodes_.forEach(cbNode);

   {
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETNAME_KEY);

      BinaryData walletNameData = name_;
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

void hd::Wallet::openDB()
{
   BinaryData masterID, mainWalletID, walletID;
   unsigned int dbCount = 0;

   LMDB dbMeta;
   dbMeta.open(dbEnv_.get(), WALLETMETA_DBNAME);
   {
      LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);
      {  //masterID
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MASTERID_KEY);

         try {
            masterID = getDataRefForKey(&dbMeta, bwKey.getData());
            walletId_ = masterID.toBinStr();
         }
         catch (NoEntryInWalletException&) {
            throw std::runtime_error("missing masterID entry");
         }
      }
      {  //mainWalletID
         BinaryWriter bwKey;
         bwKey.put_uint32_t(MAINWALLET_KEY);

         try {
            mainWalletID = getDataRefForKey(&dbMeta, bwKey.getData());
         }
         catch (NoEntryInWalletException&) {}
      }

      auto dbIter = dbMeta.begin();

      BinaryWriter bwKey;
      bwKey.put_uint8_t(WALLETMETA_PREFIX);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());

      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);

      while (dbIter.isValid()) {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);

         //check value's advertized size is packet size and strip it
         BinaryRefReader brrVal(valueBDR);
         auto valsize = brrVal.get_var_int();
         if (valsize != brrVal.getSizeRemaining()) {
            throw WalletException("entry val size mismatch");
         }
         try {
            if (keyBDR.getSize() < 2) {
               throw WalletException("invalid meta key");
            }
            auto val = brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining());
            BinaryRefReader brrKey(keyBDR);
            auto prefix = brrKey.get_uint8_t();
            if (prefix != WALLETMETA_PREFIX) {
               throw WalletException("invalid wallet meta prefix");
            }
            std::string dbname((char*)brrKey.getCurrPtr(), brrKey.getSizeRemaining());
            {
               BinaryRefReader brrVal(val);
               auto wltType = (WalletMetaType)brrVal.get_uint32_t();
               if (wltType != bs::hd::purpose) {
                  throw WalletException("invalid BIP44 wallet meta type");
               }
            }
            walletID = brrKey.get_BinaryData((uint32_t)brrKey.getSizeRemaining());

            dbCount++;
         }
         catch (const std::exception&) {
            throw WalletException("metadata reading error");
         }

         dbIter.advance();
      }
   }
   dbMeta.close();

   db_ = new LMDB(dbEnv_.get(), masterID.toBinStr());
}

void hd::Wallet::readFromDB()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);

   {  // netType
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETTYPE_KEY);
      auto netTypeRef = getDataRefForKey(db_, bwKey.getData());

      if (netTypeRef.getSize() != 1) {
         throw WalletException("invalid netType length");
      }
      BinaryRefReader brr(netTypeRef);
      netType_ = static_cast<NetworkType>(brr.get_uint8_t());
   }

   try {
      name_ = getDataRefForKey(WALLETNAME_KEY).toBinStr();
   }
   catch (const NoEntryInWalletException &) {
      throw WalletException("wallet name not set");
   }
   try {
      desc_ = getDataRefForKey(WALLETDESCRIPTION_KEY).toBinStr();
   }
   catch (const NoEntryInWalletException&) {}

   bs::wallet::KeyRank keyRank = { 0, 0 };
   {
      BinaryWriter bwKey;
      bwKey.put_uint32_t(MAIN_ACCOUNT_KEY);
      try {
         auto assetRef = getDataRefForKey(db_, bwKey.getData());
         if (!assetRef.isNull()) {
            BinaryRefReader brr(assetRef);
            keyRank.first = brr.get_uint32_t();
            keyRank.second = brr.get_uint32_t();
         }
      }
      catch (const NoEntryInWalletException &) {}
   }

   {  // root nodes
      auto dbIter = db_->begin();
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);
      CharacterArrayRef keyRef(bwKey.getSize(), bwKey.getData().getPtr());
      dbIter.seek(keyRef, LMDB::Iterator::Seek_GE);
      std::vector<std::shared_ptr<hd::Node>> rootNodes;

      while (dbIter.isValid()) {
         auto iterkey = dbIter.key();
         auto itervalue = dbIter.value();

         BinaryDataRef keyBDR((uint8_t*)iterkey.mv_data, iterkey.mv_size);
         BinaryDataRef valueBDR((uint8_t*)itervalue.mv_data, itervalue.mv_size);
         BinaryRefReader brrVal(valueBDR);

         try {
            const auto &len = static_cast<uint32_t>(brrVal.get_var_int());
            if (len == 0) {
               break;
            }
            if (len != brrVal.getSizeRemaining()) {
               break;
            }
            if (len < 65) {
               dbIter.advance();
               continue;
            }
            const auto assetRef = brrVal.get_BinaryDataRef(len);
            if (*assetRef.getPtr() != bs::hd::purpose) {
               dbIter.advance();
               continue;
            }
            const auto &node = hd::Node::deserialize(assetRef);
            rootNodes.emplace_back(node);
         }
         catch (const std::exception &e) {
            throw WalletException(std::string("failed to deser root node: ") + e.what());
         }
         dbIter.advance();
      }
      if ((keyRank == bs::wallet::KeyRank{ 0, 0 }) && (rootNodes.size() == 1) && !rootNodes[0]->encTypes().empty()) {
         keyRank = { 1, 1 };
      }
      rootNodes_ = hd::Nodes(rootNodes, keyRank, walletId_);
   }

   {  // groups
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

         BinaryRefReader brrVal(valueBDR);
         auto valsize = brrVal.get_var_int();
         if (valsize != brrVal.getSizeRemaining()) {
            throw WalletException("entry val size mismatch");
         }
         try {
            const auto group = hd::Group::deserialize(keyBDR
                 , brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining())
                                                      , rootNodes_, name_
                                                      , desc_, logger_
                                                      , extOnlyAddresses_);
            if (group != nullptr) {
               addGroup(group);
            }
         }
         catch (const std::exception &) { }

         dbIter.advance();
      }
   }
}

void hd::Wallet::setDBforDependants()
{
   for (auto group : groups_) {
      group.second->setDB(dbEnv_, db_);
   }
}

void hd::Wallet::writeToDB(bool force)
{
   for (const auto &group : groups_) {
      if (!force && !group.second->needsCommit()) {
         continue;
      }
      BinaryWriter bwKey;
      bwKey.put_uint8_t(ASSETENTRY_PREFIX);
      bwKey.put_uint32_t(group.second->index());
      putDataToDB(db_, bwKey.getData(), group.second->serialize());
      group.second->committed();
   }
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

void hd::Wallet::putDataToDB(LMDB* db, const BinaryData& key, const BinaryData& data)
{
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   CharacterArrayRef dataRef(data.getSize(), data.getPtr());
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadWrite);
   db->insert(keyRef, dataRef);
}

std::string hd::Wallet::fileNamePrefix(bool watchingOnly)
{
   return watchingOnly ? "bip44wo_" : "bip44_";
}

std::shared_ptr<hd::Wallet> hd::Wallet::createWatchingOnly(const SecureBinaryData &password) const
{
   if (rootNodes_.empty()) {
      LOG(logger_, info, "[Wallet::CreateWatchingOnly] {} already watching-only", walletId());
      return nullptr;
   }
   auto woWallet = std::make_shared<hd::Wallet>(walletId(), netType_
                                                , extOnlyAddresses_, name_
                                                , logger_, desc_);

   const auto &extNode = rootNodes_.decrypt(password);
   if (!extNode) {
      LOG(logger_, warn, "[Wallet::CreateWatchingOnly] failed to decrypt root node[s]");
      return nullptr;
   }
   for (const auto &group : groups_) {
      woWallet->addGroup(group.second->createWatchingOnly(extNode));
   }
   return woWallet;
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

bool hd::Wallet::changePassword(const std::vector<bs::wallet::PasswordData> &newPass
   , bs::wallet::KeyRank keyRank, const SecureBinaryData &oldPass
   , bool addNew, bool removeOld, bool dryRun)
{
   unsigned int newPassSize = (unsigned int)newPass.size();
   if (addNew) {
      newPassSize += rootNodes_.rank().second;

      if (keyRank.first != 1) {
         LOG(logger_, error, "Wallet::changePassword: adding new keys is supported only for 1-of-N scheme");
         return false;
      }
   }

   if (removeOld) {
      if (keyRank.first != 1) {
         LOG(logger_, error, "Wallet::changePassword: removing old keys is supported only for 1-of-N scheme");
         return false;
      }
   }

   if (keyRank.second != newPassSize) {
      LOG(logger_, error, "Wallet::changePassword: keyRank.second != newPassSize ({} != {}), rootNodes_: {} items, newPass: {} items"
         , keyRank.second, newPassSize, rootNodes_.rank().second, newPass.size());
      return false;
   }

   if ((keyRank.first < 1) || (keyRank.first > keyRank.second)) {
      LOG(logger_, error, "Wallet::changePassword: keyRank.first > keyRank.second ({} > {})"
         , keyRank.first, keyRank.second);
      return false;
   }

   const auto &decrypted = rootNodes_.decrypt(oldPass);
   if (!decrypted) {
      LOG(logger_, error, "Wallet::changePassword: decrypt failed");
      return false;
   }

   if (dryRun) {
      return true;
   }

   std::vector<std::shared_ptr<hd::Node>> rootNodes;

   if (addNew) {
      rootNodes_.forEach([&rootNodes](const std::shared_ptr<Node> node) {
         // Copy old encrypted nodes
         rootNodes.push_back(node);
      });

      for (const auto &passData : newPass) {
         rootNodes.push_back(decrypted->encrypt(passData.password, { passData.encType }
            , passData.encKey.isNull() ? std::vector<SecureBinaryData>{} : std::vector<SecureBinaryData>{ passData.encKey }));
      }

      if (keyRank.second != rootNodes.size()) {
         LOG(logger_, error, "[Wallet::changePassword] keyRank.second ({}) != rootNodes.size ({}) after adding keys"
            , keyRank.second, rootNodes.size());
         return false;
      }
   } else if (removeOld) {
      rootNodes_.forEach([&rootNodes, &newPass](const std::shared_ptr<Node> node) {
         // Copy old encrypted nodes
         for (const auto &oldKey : node->encKeys()) {
            for (const auto &newKey : newPass) {
               if (oldKey == newKey.encKey) {
                  rootNodes.push_back(node);
                  return;
               }
            }
         }
      });
   } else {
      const auto &addNode = [this, &rootNodes, decrypted, newPass, keyRank](const std::vector<int> &combi) {
         if (keyRank.first == 1) {
            const auto &passData = newPass[combi[0]];
            rootNodes.emplace_back(decrypted->encrypt(passData.password, { passData.encType }
               , passData.encKey.isNull() ? std::vector<SecureBinaryData>{} : std::vector<SecureBinaryData>{ passData.encKey }));
         }
         else {
            SecureBinaryData xorPass;
            std::set<bs::wallet::EncryptionType> encTypes;
            std::set<SecureBinaryData> encKeys;
            for (int i = 0; i < int(keyRank.first); ++i) {
               const auto &idx = combi[i];
               const auto &passData = newPass[idx];
               xorPass = mergeKeys(xorPass, passData.password);
               encTypes.insert(passData.encType);
               if (!passData.encKey.isNull()) {
                  encKeys.insert(passData.encKey);
               }
            }
            std::vector<bs::wallet::EncryptionType> mergedEncTypes;
            for (const auto &encType : encTypes) {
               mergedEncTypes.emplace_back(encType);
            }
            std::vector<SecureBinaryData> mergedEncKeys;
            for (const auto &encKey : encKeys) {
               mergedEncKeys.emplace_back(encKey);
            }
            const auto &encrypted = decrypted->encrypt(xorPass, mergedEncTypes, mergedEncKeys);
            if (!encrypted) {
               LOG(logger_, error, "Wallet::changePassword: failed to encrypt node");
               return false;
            }
            rootNodes.emplace_back(encrypted);
         }
         return true;
      };

      std::vector<int> combiIndices;
      combiIndices.reserve(keyRank.second);
      for (unsigned int i = 0; i < keyRank.second; ++i) {
         combiIndices.push_back(i);
      }
      if (!addNode(combiIndices)) {
         return false;
      }
      while (nextCombi(combiIndices, keyRank.second, keyRank.first)) {
         if (!addNode(combiIndices)) {
            return false;
         }
      }
   }

   rootNodes_ = hd::Nodes(rootNodes, keyRank, walletId_);

   for (const auto &group : groups_) {
      group.second->updateRootNodes(rootNodes_, decrypted);
   }

   updatePersistence();
   LOG(logger_, info, "Wallet::changePassword: success");
   return true;
}

void hd::Wallet::updatePersistence()
{
   if (db_) {
      initDB();
      // Force update because otherwise removed nodes would be still in the file
      writeToDB(true);
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

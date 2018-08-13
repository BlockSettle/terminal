#include "HDWallet.h"
#include <QFile>
#include <QtConcurrent/QtConcurrentRun>
#include <bech32/ref/c++/segwit_addr.h>
#include "SystemFileUtils.h"
#include "MetaData.h"
#include "Wallets.h"


using namespace bs;

hd::Wallet::Wallet(const std::string &name, const std::string &desc, const bs::wallet::Seed &seed, bool extOnlyAddresses)
   : QObject(nullptr), name_(name), desc_(desc), netType_(seed.networkType()), extOnlyAddresses_(extOnlyAddresses)
{
   initNew(seed);
}

hd::Wallet::Wallet(const std::string &filename, bool extOnlyAddresses)
   : extOnlyAddresses_(extOnlyAddresses)
{
   loadFromFile(filename);
}

hd::Wallet::Wallet(const std::string &walletId, NetworkType netType, bool extOnlyAddresses, const std::string &name
   , const std::string &desc)
   : QObject(nullptr), walletId_(walletId), name_(name), desc_(desc), netType_(netType)
   , extOnlyAddresses_(extOnlyAddresses)
{ }


void hd::Wallet::initNew(const bs::wallet::Seed &seed)
{
   const auto &rootNode = std::make_shared<hd::Node>(seed);
   walletId_ = rootNode->getId();
   rootNodes_ = hd::Nodes({ rootNode }, {0, 0}, walletId_);
}

void hd::Wallet::loadFromFile(const std::string &filename)
{
   if (!SystemFileUtils::IsValidFilePath(filename)) {
      throw std::invalid_argument(std::string("Invalid file path: ") + filename);
   }

   if (!SystemFileUtils::FileExist(filename)) {
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

std::string hd::Wallet::getWalletId() const
{
   return walletId_;
}

std::vector<std::shared_ptr<hd::Group>> hd::Wallet::getGroups() const
{
   std::vector<std::shared_ptr<hd::Group>> result;
   result.reserve(groups_.size());
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         result.emplace_back(group.second);
      }
   }
   return result;
}

size_t hd::Wallet::getNumLeaves() const
{
   size_t result = 0;
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         result += group.second->getNumLeaves();
      }
   }
   return result;
}

std::vector<std::shared_ptr<bs::Wallet>> hd::Wallet::getLeaves() const
{
   const auto nbLeaves = getNumLeaves();
   if (leaves_.size() != nbLeaves) {
      leaves_.clear();
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         const auto &groupLeaves = group.second->getAllLeaves();
         for (const auto &leaf : groupLeaves) {
            leaves_[leaf->GetWalletId()] = leaf;
         }
      }
   }

   std::vector<std::shared_ptr<bs::Wallet>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<bs::Wallet> hd::Wallet::getLeaf(const std::string &id) const
{
   const auto &itLeaf = leaves_.find(id);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
}

std::shared_ptr<hd::Group> hd::Wallet::createGroup(CoinType ct)
{
   std::shared_ptr<Group> result;
   result = getGroup(ct);
   if (result) {
      return result;
   }

   const Path path({ purpose, ct });
   switch (ct) {
   case CoinType::BlockSettle_Auth:
      result = std::make_shared<AuthGroup>(rootNodes_, path, name_, desc_, extOnlyAddresses_);
      break;

   case CoinType::BlockSettle_CC:
      result = std::make_shared<CCGroup>(rootNodes_, path, name_, desc_, extOnlyAddresses_);
      break;

   default:
      result = std::make_shared<Group>(rootNodes_, path, name_, hd::Group::nameForType(ct), desc_, extOnlyAddresses_);
      break;
   }
   addGroup(result);
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   connect(group.get(), &hd::Group::changed, this, &hd::Wallet::onGroupChanged);
   connect(group.get(), &hd::Group::leafAdded, this, &hd::Wallet::onLeafAdded);
   connect(group.get(), &hd::Group::leafDeleted, this, &hd::Wallet::onLeafDeleted);
   group->setDB(dbEnv_, db_);
   if (!userId_.isNull()) {
      group->setUserID(userId_);
   }

   QMutexLocker lock(&mtxGroups_);
   groups_[group->getIndex()] = group;
}

std::shared_ptr<hd::Group> hd::Wallet::getGroup(hd::CoinType ct) const
{
   QMutexLocker lock(&mtxGroups_);
   const auto &itGroup = groups_.find(static_cast<Path::Elem>(ct));
   if (itGroup == groups_.end()) {
      return nullptr;
   }
   return itGroup->second;
}

void hd::Wallet::onGroupChanged()
{
   updatePersistence();
}

void hd::Wallet::onLeafAdded(QString id)
{
   getLeaves();
   emit leafAdded(id);
}

void hd::Wallet::onLeafDeleted(QString id)
{
   getLeaves();
   emit leafDeleted(id);
}

void hd::Wallet::createStructure()
{
   const auto groupXBT = createGroup(getXBTGroupType());
   groupXBT->createLeaf(0u);
}

void hd::Wallet::setUserId(const BinaryData &userId)
{
   userId_ = userId;
   std::vector<std::shared_ptr<hd::Group>> groups;
   groups.reserve(groups_.size());
   {
      QMutexLocker lock(&mtxGroups_);
      for (const auto &group : groups_) {
         groups.push_back(group.second);
      }
   }
   for (const auto &group : groups) {
      group->setUserID(userId);
   }
}

void hd::Wallet::SetBDM(const std::shared_ptr<PyBlockDataManager> &bdm)
{
   for (const auto &leaf : getLeaves()) {
      leaf->SetBDM(bdm);
   }
}

void hd::Wallet::RegisterWallet(const std::shared_ptr<PyBlockDataManager>& bdm, bool asNew)
{
   for (const auto &leaf : getLeaves()) {
      leaf->RegisterWallet(bdm, asNew);
   }
}

bool hd::Wallet::startRescan(const hd::Wallet::cb_scan_notify &cb, const cb_scan_read_last &cbr
   , const cb_scan_write_last &cbw)
{
   {
      QMutexLocker lock(&mtxGroups_);
      if (!scannedLeaves_.empty()) {
         return false;
      }
   }
   QtConcurrent::run(this, &hd::Wallet::rescanBlockchain, cb, cbr, cbw);
   return true;
}

std::string hd::Wallet::getFileName(const std::string &dir) const
{
   return (dir + "/" + fileNamePrefix(isWatchingOnly()) + getWalletId() + "_wallet.lmdb");
}

void hd::Wallet::saveToDir(const std::string &targetDir)
{
   const auto masterID = BinaryData(getWalletId());
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
   QFile walletFile(QString::fromStdString(dbFilename_));
   if (walletFile.exists()) {
      rc = walletFile.remove();

      QFile lockFile(QString::fromStdString(dbFilename_ + "-lock"));
      rc &= lockFile.remove();
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
   BinaryData masterID(getWalletId());
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

         const auto mainWalletId = leafMain->GetWalletId();
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
      bw.put_uint32_t(hd::purpose);
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

   const auto &cbNode = [this](const std::shared_ptr<hd::Node> &node) {
      BinaryWriter bwKey;
      bwKey.put_uint32_t(ROOTASSET_KEY);

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
            throw runtime_error("missing masterID entry");
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
               if (wltType != hd::purpose) {
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

   wallet::KeyRank keyRank = { 0, 0 };
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
      if ((keyRank == wallet::KeyRank{ 0, 0 }) && (rootNodes.size() == 1) && !rootNodes[0]->encTypes().empty()) {
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
            const auto group = hd::Group::deserialize(keyBDR, brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining())
               , rootNodes_, name_, desc_, extOnlyAddresses_);
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
   QMutexLocker lock(&mtxGroups_);
   for (auto group : groups_) {
      group.second->setDB(dbEnv_, db_);
   }
}

void hd::Wallet::writeToDB(bool force)
{
   QMutexLocker lock(&mtxGroups_);
   for (const auto &group : groups_) {
      if (!force && !group.second->needsCommit()) {
         continue;
      }
      BinaryWriter bwKey;
      bwKey.put_uint8_t(ASSETENTRY_PREFIX);
      bwKey.put_uint32_t(group.second->getIndex());
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
      throw WalletException("on disk data length mismatch: " + to_string(len) + ", " + to_string(brr.getSizeRemaining()));
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

void hd::Wallet::rescanBlockchain(const hd::Wallet::cb_scan_notify &cb, const cb_scan_read_last &cbr
   , const cb_scan_write_last &cbw)
{
   QMutexLocker lock(&mtxGroups_);
   for (const auto &group : groups_) {
      group.second->rescanBlockchain(cb, cbr, cbw);
      for (const auto &leaf : group.second->getLeaves()) {
         scannedLeaves_.insert(leaf->GetWalletId());
         connect(leaf.get(), &hd::Leaf::scanComplete, this, &hd::Wallet::onScanComplete);
      }
   }
}

void hd::Wallet::onScanComplete(const std::string &leafId)
{
   QMutexLocker lock(&mtxGroups_);
   scannedLeaves_.erase(leafId);
   if (scannedLeaves_.empty()) {
      emit scanComplete(getWalletId());
   }
}

std::string hd::Wallet::fileNamePrefix(bool watchingOnly)
{
   return watchingOnly ? "bip44wo_" : "bip44_";
}

std::shared_ptr<hd::Wallet> hd::Wallet::CreateWatchingOnly(const SecureBinaryData &password) const
{
   if (rootNodes_.empty()) {    // already watching-only
      return nullptr;
   }
   auto woWallet = std::make_shared<hd::Wallet>(getWalletId(), netType_, extOnlyAddresses_, name_, desc_);

   const auto &extNode = rootNodes_.decrypt(password);
   for (const auto &group : groups_) {
      woWallet->addGroup(group.second->CreateWatchingOnly(extNode));
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

bool hd::Wallet::changePassword(const std::vector<wallet::PasswordData> &newPass, wallet::KeyRank keyRank, const SecureBinaryData &oldPass)
{
   if ((keyRank.second != newPass.size()) || (keyRank.first < 1) || (keyRank.first > keyRank.second)) {
      return false;
   }
   const auto &decrypted = rootNodes_.decrypt(oldPass);
   if (!decrypted) {
      return false;
   }

   std::vector<std::shared_ptr<hd::Node>> rootNodes;
   const auto &addNode = [&rootNodes, decrypted, newPass, keyRank](const std::vector<int> &combi) {
      if (keyRank.first == 1) {
         const auto &passData = newPass[combi[0]];
         rootNodes.emplace_back(decrypted->encrypt(passData.password, { passData.encType }
            , passData.encKey.isNull() ? std::vector<SecureBinaryData>{} : std::vector<SecureBinaryData>{ passData.encKey }));
      }
      else {
         SecureBinaryData xorPass;
         std::set<wallet::EncryptionType> encTypes;
         std::set<SecureBinaryData> encKeys;
         for (int i = 0; i < keyRank.first; ++i) {
            const auto &idx = combi[i];
            const auto &passData = newPass[idx];
            xorPass = mergeKeys(xorPass, passData.password);
            encTypes.insert(passData.encType);
            if (!passData.encKey.isNull()) {
               encKeys.insert(passData.encKey);
            }
         }
         std::vector<wallet::EncryptionType> mergedEncTypes;
         for (const auto &encType : encTypes) {
            mergedEncTypes.emplace_back(encType);
         }
         std::vector<SecureBinaryData> mergedEncKeys;
         for (const auto &encKey : encKeys) {
            mergedEncKeys.emplace_back(encKey);
         }
         rootNodes.emplace_back(decrypted->encrypt(xorPass, mergedEncTypes, mergedEncKeys));
      }
   };

   std::vector<int> combiIndices;
   combiIndices.reserve(keyRank.second);
   for (int i = 0; i < keyRank.second; ++i) {
      combiIndices.push_back(i);
   }
   addNode(combiIndices);
   while (nextCombi(combiIndices, keyRank.second, keyRank.first)) {
      addNode(combiIndices);
   }
   rootNodes_ = hd::Nodes(rootNodes, keyRank, walletId_);

   for (const auto &group : groups_) {
      group.second->updateRootNodes(rootNodes_, decrypted);
   }

   updatePersistence();
   return true;
}

void hd::Wallet::updatePersistence()
{
   if (db_) {
      initDB();
      writeToDB();
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

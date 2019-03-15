#include "CoreHDWallet.h"
#include <QFile>
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
                   , const wallet::Seed &seed, const std::string &walletsPath
                   , const SecureBinaryData& passphrase
                   , const std::shared_ptr<spdlog::logger> &logger)
   : name_(name), desc_(desc)
   , netType_(seed.networkType())
   , logger_(logger)
{
   initNew(seed, walletsPath, passphrase);
}

hd::Wallet::Wallet(const std::string &filename
                   , const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{
   loadFromFile(filename);
}

hd::Wallet::Wallet(const std::string &walletId, NetworkType netType
                   , const std::string &name
                   , const std::shared_ptr<spdlog::logger> &logger
                   , const std::string &desc)
   : walletId_(walletId), name_(name), desc_(desc)
   , netType_(netType)
   , logger_(logger)
{}

hd::Wallet::~Wallet()
{
   if (db_ != nullptr)
      delete db_;
}

void hd::Wallet::initNew(const wallet::Seed &seed, 
   const std::string &walletsPath, const SecureBinaryData& passphrase)
{
   walletPtr_ = AssetWallet_Single::createFromSeed_BIP32_Blank(
      walletsPath, seed.seed(), passphrase);
   dbEnv_ = walletPtr_->getDbEnv();
   db_ = new LMDB(dbEnv_.get(), BS_WALLET_DBNAME);
}

void hd::Wallet::loadFromFile(const std::string &filename)
{
   if (!SystemFileUtils::IsValidFilePath(filename)) {
      throw std::invalid_argument(std::string("Invalid file path: ") + filename);
   }

   if (!SystemFileUtils::FileExist(filename)) {
      throw std::runtime_error("Wallet file does not exist");
   }

   //load armory wallet
   auto walletPtr = AssetWallet::loadMainWalletFromFile(filename);
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
      result = std::make_shared<AuthGroup>(
         walletPtr_, path, netType_, logger_);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<CCGroup>(
         walletPtr_, path, netType_, logger_);
      break;

   default:
      result = std::make_shared<Group>(
         walletPtr_, path, netType_, logger_);
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

void hd::Wallet::createStructure()
{
   initializeDB();
   const auto groupXBT = createGroup(getXBTGroupType());
   groupXBT->createLeaf(0u);
   writeGroupsToDB();
}

std::string hd::Wallet::getFileName(const std::string &dir) const
{
   return (dir + "/" + fileNamePrefix(isWatchingOnly()) + walletId() + "_wallet.lmdb");
}

bool hd::Wallet::eraseFile()
{
   auto& fname = walletPtr_->getDbFilename();
   if (fname.size() == 0)
      return true;

   bool rc = true;
   QFile walletFile(QString::fromStdString(fname));
   if (walletFile.exists()) {
      rc = walletFile.remove();

      QFile lockFile(QString::fromStdString(fname + "-lock"));
      rc &= lockFile.remove();
   }
   return rc;
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
}

void hd::Wallet::readFromDB()
{
   LMDBEnv::Transaction tx(dbEnv_.get(), LMDB::ReadOnly);

   {  //header data
      auto typeBdr = getDataRefForKey(WALLETTYPE_KEY);
      if (typeBdr.getSize() != 1) 
         throw WalletException("invalid netType length");
      netType_ = static_cast<NetworkType>(typeBdr.getPtr()[0]);

      name_ = getDataRefForKey(WALLETNAME_KEY).toBinStr();
      desc_ = getDataRefForKey(WALLETDESCRIPTION_KEY).toBinStr();
   }

   {  // groups
      auto dbIter = db_->begin();

      //TODO:: use dedicated key for groups, do not mix the custom bs wallet data
      //with the main armory wallet content
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
         if (valsize != brrVal.getSizeRemaining()) {
            throw WalletException("entry val size mismatch");
         }
         try {
            const auto group = hd::Group::deserialize(walletPtr_,
               keyBDR, brrVal.get_BinaryDataRef((uint32_t)brrVal.getSizeRemaining())
                 , name_, desc_, netType_, logger_);
            if (group != nullptr) {
               addGroup(group);
            }
         }
         catch (const std::exception &) { }

         dbIter.advance();
      }
   }
}

void hd::Wallet::writeGroupsToDB(bool force)
{
   for (const auto &group : groups_) {
      if (!force && !group.second->needsCommit()) {
         continue;
      }
      BinaryWriter bwKey;
      bwKey.put_uint8_t(BS_GROUP_PREFIX);
      bwKey.put_uint32_t(group.second->index());
      putDataToDB(bwKey.getData(), group.second->serialize());
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

std::shared_ptr<hd::Wallet> hd::Wallet::createWatchingOnly(const SecureBinaryData &password) const
{
   //TODO: rework this
   if (walletPtr_->isWatchingOnly()) {
      LOG(logger_, info, "[Wallet::CreateWatchingOnly] {} already watching-only", walletId());
      return nullptr;
   }
   auto woWallet = std::make_shared<hd::Wallet>(walletId(), netType_
                                                , name_
                                                , logger_, desc_);
   for (const auto &group : groups_) {
      woWallet->addGroup(group.second);
   }
   return woWallet;
}

bool hd::Wallet::isWatchingOnly() const
{
   auto mainAccID = walletPtr_->getMainAccountID();
   auto accPtr = walletPtr_->getAccountRoot(mainAccID);
   return accPtr->hasPrivateKey();
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

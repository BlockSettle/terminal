/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
                   , const bs::wallet::PasswordData &pd
                   , const std::string& folder
                   , const std::shared_ptr<spdlog::logger> &logger)
   : name_(name), desc_(desc)
   , netType_(seed.networkType())
   , logger_(logger)
{
   initNew(seed, pd, folder);
}

hd::Wallet::Wallet(const std::string &filename, NetworkType netType,
   const std::string& folder, const SecureBinaryData &controlPassphrase
   , const std::shared_ptr<spdlog::logger> &logger)
   : netType_(netType), logger_(logger)
{
   loadFromFile(filename, folder, controlPassphrase);
}

hd::Wallet::Wallet(const std::string &name, const std::string &desc
   , NetworkType netType, const bs::wallet::PasswordData & pd
   , const std::string& folder
   , const std::shared_ptr<spdlog::logger> &logger)
   : name_(name), desc_(desc)
   , netType_(netType)
   , logger_(logger)
{
   wallet::Seed seed(CryptoPRNG::generateRandom(32), netType);
   initNew(seed, pd, folder);
}

hd::Wallet::~Wallet()
{
   shutdown();
}

void hd::Wallet::initNew(const wallet::Seed &seed
   , const bs::wallet::PasswordData &pd, const std::string &folder)
{
   try {
      walletPtr_ = AssetWallet_Single::createFromSeed_BIP32_Blank(
         folder, seed.seed(), pd.password, pd.controlPassword);
   }
   catch (const WalletException &) {
      //empty account structure, will be set at group creation
      std::set<std::shared_ptr<AccountType>> accountTypes;
      
      auto& node = seed.getNode();
      if (node.getPrivateKey().getSize() != 32 &&
         node.getPublicKey().getSize() != 33)
         throw WalletException("invalid seed node");

      walletPtr_ = AssetWallet_Single::createFromBIP32Node(
         seed.getNode(), accountTypes, pd.password, pd.controlPassword
         , folder, 0); //no lookup, as there are no accounts
   }
   filePathName_ = folder;
   DBUtils::appendPath(filePathName_, walletPtr_->getDbFilename());

   lbdControlPassphrase_ = [controlPassphrase = pd.controlPassword]
      (const std::set<BinaryData>&) -> SecureBinaryData
   {
      return controlPassphrase;
   };

   pwdMeta_.push_back(pd.metaData);

   initializeDB();
   writeToDB();
}

void hd::Wallet::loadFromFile(const std::string &filename,
   const std::string &folder, const SecureBinaryData &controlPassphrase)
{
   filePathName_ = folder;
   DBUtils::appendPath(filePathName_, filename);
   if (!SystemFileUtils::isValidFilePath(filePathName_)) {
      throw std::invalid_argument(std::string("Invalid file path: ") + filePathName_);
   }
   if (!SystemFileUtils::fileExist(filePathName_)) {
      throw std::runtime_error("Wallet file " + filePathName_ + " does not exist");
   }

   lbdControlPassphrase_ = [controlPassphrase]
      (const std::set<BinaryData>&)->SecureBinaryData
   {
      return controlPassphrase;
   };

   //load armory wallet
   auto walletPtr = AssetWallet::loadMainWalletFromFile(filePathName_, lbdControlPassphrase_);
   walletPtr_ = std::dynamic_pointer_cast<AssetWallet_Single>(walletPtr);
   if (walletPtr_ == nullptr) {
      throw WalletException("failed to load wallet");
   }

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

std::vector<bs::wallet::EncryptionType> hd::Wallet::encryptionTypes() const
{
   std::set<bs::wallet::EncryptionType> setOfTypes;
   std::vector<bs::wallet::EncryptionType> result;
   for (const auto &meta : pwdMeta_) {
      setOfTypes.insert(meta.encType);
   }
   result.insert(result.end(), setOfTypes.cbegin(), setOfTypes.cend());
   return result;
}

std::vector<BinaryData> hd::Wallet::encryptionKeys() const
{
   std::vector<BinaryData> result;
   for (const auto &meta : pwdMeta_) {
      result.push_back(meta.encKey);
   }
   return result;
}

std::shared_ptr<hd::Group> hd::Wallet::createGroup(bs::hd::CoinType ct)
{
   std::shared_ptr<Group> result;
   ct = static_cast<bs::hd::CoinType>(ct | bs::hd::hardFlag);
   result = getGroup(ct);
   if (result) {
      return result;
   }

   switch (ct) {
   case bs::hd::CoinType::BlockSettle_Auth:
      result = std::make_shared<AuthGroup>(
         walletPtr_, netType_, logger_);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      result = std::make_shared<CCGroup>(
         walletPtr_, netType_, logger_);
      break;

   case bs::hd::CoinType::BlockSettle_Settlement:
      result = std::make_shared<SettlementGroup>(
         walletPtr_, netType_, logger_);
      break;

   default:
      result = std::make_shared<Group>(
         walletPtr_, ct, netType_, extOnlyFlag_, logger_);
      break;
   }
   addGroup(result);
   writeToDB();
   return result;
}

void hd::Wallet::addGroup(const std::shared_ptr<hd::Group> &group)
{
   groups_[group->index() | bs::hd::hardFlag] = group;
}

std::shared_ptr<hd::Group> hd::Wallet::getGroup(bs::hd::CoinType ct) const
{
   ct = static_cast<bs::hd::CoinType>(ct | bs::hd::hardFlag);
   const auto &itGroup = groups_.find(static_cast<bs::hd::Path::Elem>(ct));
   if (itGroup == groups_.end()) {
      return nullptr;
   }
   return itGroup->second;
}

void hd::Wallet::createStructure(unsigned lookup)
{
   const auto groupXBT = createGroup(getXBTGroupType());
   assert(groupXBT);
   for (const auto &aet : groupXBT->getAddressTypeSet()) {
      groupXBT->createLeaf(aet, 0u, lookup);
   }
   writeToDB();
}

void hd::Wallet::createChatPrivKey()
{
   bs::hd::Path path;
   path.append("BS-alt'");
   path.append("Chat'");
   path.append(0 | bs::hd::hardFlag);

   chatNode_ = getDecryptedSeed().getNode();
   for (int i = 0; i < path.length(); ++i) {
      chatNode_.derivePrivate(path.get(i));
   }
   if (logger_) {
      logger_->debug("[{}] created chat key {}", __func__
         , chatNode_.getPublicKey().toHexStr());
   }

   try {
      walletPtr_->addSubDB(BS_CHAT_DBNAME, lbdControlPassphrase_);
   } catch (const std::exception &e) {
      if (logger_) {
         logger_->warn("[{}] wallet {} DB {} already inited: {}", __func__
            , walletId(), BS_CHAT_DBNAME, e.what());
      }
   }

   BinaryWriter bwKey;
   bwKey.put_uint32_t(CHAT_NODE_KEY);

   const auto tx = walletPtr_->beginSubDBTransaction(BS_CHAT_DBNAME, true);
   tx->insert(bwKey.getData(), chatNode_.getBase58());
}

BIP32_Node hd::Wallet::getChatNode() const
{
   if (!chatNode_.getPrivateKey().isNull()) {
      return chatNode_;
   }
   try {
      const auto tx = walletPtr_->beginSubDBTransaction(BS_CHAT_DBNAME, false);
      BinaryWriter bwKey;
      bwKey.put_uint32_t(CHAT_NODE_KEY);

      chatNode_.initFromBase58(tx->getDataRef(bwKey.getData()));
   } catch (...) {}    // np if chat DB doesn't exist
   return chatNode_;
}

void hd::Wallet::shutdown()
{
   for (const auto &group : groups_) {
      group.second->shutdown();
   }
   groups_.clear();

   if (walletPtr_ != nullptr) {
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
   if (walletPtr_ == nullptr) {
      throw WalletException("wallet is not initialized, cannot return filename");
   }
   return walletPtr_->getDbFilename();
}

void hd::Wallet::initializeDB()
{
   try {
      walletPtr_->addSubDB(BS_WALLET_DBNAME, lbdControlPassphrase_);
   }
   catch (const std::exception &e) {
      if (logger_) {
         logger_->error("[{}] Wallet {} DB already inited: {}", __func__, walletId(), e.what());
      }
   }
   //commit bs header data
   const auto tx = walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, true);

   {  //network type
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETTYPE_KEY);

      BinaryWriter bwData;
      bwData.put_uint8_t(static_cast<uint8_t>(netType_));

      tx->insert(bwKey.getData(), bwData.getData());
   }

   {  //name
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETNAME_KEY);

      BinaryData walletNameData = name_;
      BinaryWriter bwName;
//      bwName.put_var_int(walletNameData.getSize());
      bwName.put_BinaryData(walletNameData);
      tx->insert(bwKey.getData(), bwName.getData());
   }
   {  //description
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLETDESCRIPTION_KEY);

      BinaryData walletDescriptionData = desc_;
      BinaryWriter bwDesc;
//      bwDesc.put_var_int(walletDescriptionData.getSize());
      bwDesc.put_BinaryData(walletDescriptionData);
      tx->insert(bwKey.getData(), bwDesc.getData());
   }

   {  //ext only flag
      BinaryWriter bwKey;
      bwKey.put_uint32_t(WALLET_EXTONLY_KEY);

      BinaryWriter bwFlag;
      bwFlag.put_uint8_t(extOnlyFlag_);
      tx->insert(bwKey.getData(), bwFlag.getData());
   }
}

static BinaryDataRef getDataRefForKey(const std::shared_ptr<DBIfaceTransaction> &tx
   , uint32_t key)
{
   BinaryWriter bwKey;
   bwKey.put_uint32_t(key);
   return tx->getDataRef(bwKey.getData());
}

void hd::Wallet::readFromDB()
{
   const auto tx = walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, false);

   {  //header data
      auto typeBdr = getDataRefForKey(tx, WALLETTYPE_KEY);
      if (typeBdr.getSize() != 1) {
         throw WalletException("invalid netType length " + std::to_string(typeBdr.getSize()));
      }
      netType_ = static_cast<NetworkType>(typeBdr.getPtr()[0]);

      name_ = getDataRefForKey(tx, WALLETNAME_KEY).toBinStr();
      desc_ = getDataRefForKey(tx, WALLETDESCRIPTION_KEY).toBinStr();
      extOnlyFlag_ = (bool)*getDataRefForKey(tx, WALLET_EXTONLY_KEY).getPtr();
   }

   { // password metadata
      BinaryRefReader brrPwdMeta(getDataRefForKey(tx, WALLET_PWD_META_KEY));
      auto pwdMetaSize = brrPwdMeta.get_var_int();
      if (pwdMetaSize > 32) { // unlikely there can be more than 32 passwords in a wallet
         throw WalletException("invalid password meta of size " + std::to_string(pwdMetaSize));
      }
      while (pwdMetaSize--> 0) {
         const auto encType = static_cast<bs::wallet::EncryptionType>(brrPwdMeta.get_uint8_t());
         const auto encKeyLen = brrPwdMeta.get_var_int();
         pwdMeta_.push_back({ encType, brrPwdMeta.get_BinaryData(encKeyLen) });
      }
   }

   {  // groups
      const auto dbIter = tx->getIterator();

      BinaryWriter bwKey;
      bwKey.put_uint8_t(BS_GROUP_PREFIX);

      dbIter->seek(bwKey.getData());
      while (dbIter->isValid()) {
         auto keyBDR = dbIter->key();
         auto valueBDR = dbIter->value();

         //sanity check on the key
         if (keyBDR.getSize() == 0 || keyBDR.getPtr()[0] != BS_GROUP_PREFIX) {
            break;
         }
         if (valueBDR.getSize() < 2) {
            throw WalletException("invalid serialized group size " + std::to_string(valueBDR.getSize()));
         }
         try {
            const auto group = hd::Group::deserialize(walletPtr_, keyBDR, valueBDR
                 , name_, desc_, netType_, logger_);
            if (group != nullptr) {
               addGroup(group);
               if (logger_) {
                  logger_->debug("[{}] group {} added", __func__, (uint32_t)group->index());
               }
            }
         }
         catch (const std::exception &e) {
            if (logger_) {
               logger_->error("[{}] error reading group: {}", __func__, e.what());
            }
         }

         dbIter->advance();
      }
   }
   for (const auto &leaf : getLeaves()) {
      leaf->readMetaData();
   }
}

void hd::Wallet::writeToDB(bool force)
{
   BinaryWriter bwKey;
   bwKey.put_uint32_t(WALLET_PWD_META_KEY);

   BinaryWriter bwPwdMeta;
   bwPwdMeta.put_var_int(pwdMeta_.size());
   for (const auto &meta : pwdMeta_) {
      bwPwdMeta.put_uint8_t(uint8_t(meta.encType));
      bwPwdMeta.put_var_int(meta.encKey.getSize());
      bwPwdMeta.put_BinaryData(meta.encKey);
   }

   const auto tx = walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, true);
   tx->insert(bwKey.getData(), bwPwdMeta.getData());

   for (const auto &group : groups_) {
      group.second->commit(tx, force);
   }
}

std::string hd::Wallet::fileNamePrefix(bool watchingOnly)
{
   return watchingOnly ? "bip44wo_" : "bip44_";
}

std::shared_ptr<hd::Wallet> hd::Wallet::createWatchingOnly() const
{
   //fork WO copy of armory wallet
   auto woFilename = AssetWallet::forkWatchingOnly(walletPtr_->getDbFilename()
      , lbdControlPassphrase_);

   //instantiate empty core::hd::Wallet
   std::shared_ptr<hd::Wallet> woCopy(new hd::Wallet());

   //populate with this wallet's meta data
   woCopy->name_ = name_;
   woCopy->desc_ = desc_;
   woCopy->netType_ = netType_;
   woCopy->logger_ = logger_;

   //setup the armory wallet ptr and dbs
   woCopy->walletPtr_ = std::dynamic_pointer_cast<AssetWallet_Single>(
      AssetWallet::loadMainWalletFromFile(woFilename, lbdControlPassphrase_));
   woCopy->lbdControlPassphrase_ = lbdControlPassphrase_;

   //init wo blocksettle meta data db
   woCopy->initializeDB();

   //copy group and leaf structure
   for (auto& groupPair : groups_)
   {
      auto newGroup = groupPair.second->getCopy(woCopy->walletPtr_);
      woCopy->addGroup(newGroup);
   }

   //commit to disk
   woCopy->writeToDB();

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

bool hd::Wallet::changePassword(const bs::wallet::PasswordMetaData &oldPD
   , const bs::wallet::PasswordData &pd)
{
   if ((pd.password.getSize() < 6) ||
      (pd.metaData.encType == bs::wallet::EncryptionType::Unencrypted)) {
      logger_->error("[{}] invalid new password", __func__);
      return false;
   }

   auto itPwdMeta = std::find_if(pwdMeta_.begin(), pwdMeta_.end()
      , [oldPD](const bs::wallet::PasswordMetaData &pmd)->bool {
      if ((oldPD.encType == pmd.encType) && (oldPD.encKey == pmd.encKey)) {
         return true;
      }
      return false;
   });
   if (itPwdMeta == pwdMeta_.end()) {
      logger_->error("[{}] failed to find previous password meta {}"
         , __func__, oldPD.encKey.toBinStr());
      return false;
   }

   try {
      walletPtr_->changeMasterPassphrase(pd.password);
      *itPwdMeta = pd.metaData;
      writeToDB();
      return true;
   }
   catch (const std::exception &e) {
      logger_->error("[{}] got error: {}", __func__, e.what());
      return false;
   }
   catch (const AlreadyLocked &) {
      logger_->error("[{}] secure container already locked", __func__);
      return false;
   }
}

bool hd::Wallet::addPassword(const bs::wallet::PasswordData &pd)
{
   if ((pd.password.getSize() < 6) ||
      (pd.metaData.encType == bs::wallet::EncryptionType::Unencrypted)) {
      logger_->error("[{}] invalid new password", __func__);
      return false;
   }
   try {
      walletPtr_->addPassphrase(pd.password);
      pwdMeta_.push_back(pd.metaData);
      writeToDB();
      return true;
   }
   catch (const std::exception &e) {
      logger_->error("[{}] got error: {}", __func__, e.what());
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

void hd::Wallet::copyToFile(const std::string &filename)
{
   std::ifstream source(filePathName_, std::ios::binary);
   std::ofstream dest(filename, std::ios::binary);

   std::istreambuf_iterator<char> begin_source(source);
   std::istreambuf_iterator<char> end_source;
   std::ostreambuf_iterator<char> begin_dest(dest);
   std::copy(begin_source, end_source, begin_dest);

   source.close();
   dest.close();
}

void hd::Wallet::pushPasswordPrompt(const std::function<SecureBinaryData()> &lbd)
{
   if (!walletPtr_) {
      return;
   }
   const auto lbdWrap = [lbd, this](const std::set<BinaryData> &)->SecureBinaryData {
      return lbd();
   };
   walletPtr_->setPassphrasePromptLambda(lbdWrap);
   lbdPwdPrompts_.push_back(lbdWrap);
}

void hd::Wallet::popPasswordPrompt()
{
   lbdPwdPrompts_.pop_back();
   if (!walletPtr_) {
      return;
   }
   if (lbdPwdPrompts_.empty()) {
      walletPtr_->resetPassphrasePromptLambda();
   }
   else {
      walletPtr_->setPassphrasePromptLambda(lbdPwdPrompts_.back());
   }
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
   const auto tx = walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, true);

   BinaryWriter bwKey;
   bwKey.put_uint32_t(WALLET_EXTONLY_KEY);

   BinaryWriter bwDesc;
   bwDesc.put_uint8_t(extOnlyFlag_);
   tx->insert(bwKey.getData(), bwDesc.getData());
}

bs::core::wallet::Seed hd::Wallet::getDecryptedSeed(void) const
{  /***
   Expects wallet to be locked and passphrase lambda set
   ***/

   if (walletPtr_ == nullptr) {
      throw WalletException("uninitialized armory wallet");
   }
   auto seedPtr = walletPtr_->getEncryptedSeed();
   if (seedPtr == nullptr) {
      throw WalletException("wallet has no seed");
   }
   auto lock = walletPtr_->lockDecryptedContainer();
   auto clearSeed = walletPtr_->getDecryptedValue(seedPtr);
   bs::core::wallet::Seed rootObj(clearSeed, netType_);
   return rootObj;
}

SecureBinaryData hd::Wallet::getDecryptedRootXpriv(void) const
{  /***
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

   auto lock = walletPtr_->lockDecryptedContainer();
   auto decryptedRootPrivKey = walletPtr_->getDecryptedPrivateKeyForAsset(root);
   
   BIP32_Node node;
   node.initFromPrivateKey(
      rootBip32->getDepth(), rootBip32->getLeafID(), rootBip32->getFingerPrint(),
      decryptedRootPrivKey, rootBip32->getChaincode());
   return node.getBase58();
}

bs::hd::Path hd::Wallet::getPathForAddress(const bs::Address &addr)
{
   for (auto& groupPair : groups_) {
      for (auto& leafPair : groupPair.second->leaves_) {
         auto path = leafPair.second->getPathForAddress(addr);
         if (path.length() != 0) {
            return path;
         }
      }
   }
   return {};
};

std::shared_ptr<hd::Leaf> hd::Wallet::createSettlementLeaf(
   const bs::Address& addr)
{  /*
   This method expects the wallet locked and passprhase lambda set 
   for a full wallet.
   */

   //does this wallet have a settlement group?
   auto group = getGroup(bs::hd::BlockSettle_Settlement);
   if (group == nullptr) {
      group = createGroup(bs::hd::BlockSettle_Settlement);
   }
   auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
   if (settlGroup == nullptr) {
      throw AccountException("unexpected settlement group type");
   }

   auto addrPath = getPathForAddress(addr);
   if (addrPath.length() == 0) {
      throw AssetException("failed to resolve path for settlement address");
   }

   const bs::hd::Path settlLeafPath({ bs::hd::Purpose::Native
      , bs::hd::BlockSettle_Settlement, addrPath.get(-1) });
   auto leaf = settlGroup->getLeafByPath(settlLeafPath);
   if (leaf) {
      return leaf;
   }
   return settlGroup->createLeaf(addr, settlLeafPath);
}

std::shared_ptr<hd::Leaf> hd::Wallet::getSettlementLeaf(const bs::Address &addr)
{  /*
   This method expects the wallet locked and passprhase lambda set
   for a full wallet.
   */

   //does this wallet have a settlement group?
   auto group = getGroup(bs::hd::BlockSettle_Settlement);
   if (group) {
      auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
      if (!settlGroup) {
         return nullptr;
      }
      auto addrPath = getPathForAddress(addr);
      if (addrPath.length() == 0) {
         return nullptr;
      }

      const bs::hd::Path settlLeafPath({ bs::hd::Purpose::Native
         , bs::hd::BlockSettle_Settlement, addrPath.get(-1) });
      const auto leaf = settlGroup->getLeafByPath(settlLeafPath);
      if (leaf) {
         return leaf;
      }
   }
   return nullptr;
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
   return bs::Address::fromHash(addrPtr->getPrefixedHash());
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
   if (index == UINT32_MAX) {
      throw AssetException("settlement id " + settlementID.toHexStr()
         + " not found in " + leafPtr->walletId());
   }
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
   const SecureBinaryData &id) const
{
   auto group = getGroup(bs::hd::CoinType::BlockSettle_Settlement);
   auto settlGroup = std::dynamic_pointer_cast<hd::SettlementGroup>(group);
   if (settlGroup == nullptr) {
      throw AccountException("missing settlement group");
   }
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
   const auto leafPtr = getLeafForSettlementID(sd.settlementId);
   const auto lock = leafPtr->lockDecryptedContainer();

   //sign & return
   auto signer = leafPtr->getSigner(txReq, false);
   signer.resetFeeds();
   signer.setFeed(resolver);

   signer.sign();
   return signer.serialize();
}


WalletPasswordScoped::WalletPasswordScoped(const std::shared_ptr<hd::Wallet> &wallet
   , const SecureBinaryData &passphrase) : wallet_(wallet)
{
   const auto lbd = [this, passphrase]()->SecureBinaryData
   {
      if (++nbTries_ > maxTries_) {
         return {};
      }
      return passphrase;
   };
   wallet_->pushPasswordPrompt(lbd);
}

#include "CoreHDGroup.h"
#include "HDPath.h"
#include "Wallets.h"
#include <spdlog/spdlog.h>

using namespace bs::core;


hd::Group::Group(std::shared_ptr<AssetWallet_Single> walletPtr
   , bs::hd::Path::Elem index, NetworkType netType, bool isExtOnly,
   const std::shared_ptr<spdlog::logger> &logger)
   : walletPtr_(walletPtr), index_(index & ~bs::hd::hardFlag)
   , netType_(netType), isExtOnly_(isExtOnly)
   , logger_(logger)
{
   if (walletPtr_ == nullptr) {
      throw AccountException("null armory wallet pointer");
   }
   db_ = new LMDB(walletPtr_->getDbEnv().get(), BS_WALLET_DBNAME);
}

hd::Group::~Group()
{
   shutdown();
}

std::shared_ptr<hd::Leaf> hd::Group::getLeafByPath(const bs::hd::Path &path) const
{
   if (path.length() < 3) {
      throw AccountException("wrong path length " + std::to_string(path.length()));
   }
   auto leafPath = path;
   for (size_t i = 0; i < 3; ++i) {
      leafPath.setHardened(i);
   }
   const auto itLeaf = leaves_.find(leafPath);
   if (itLeaf == leaves_.end()) {
      return nullptr;
   }
   return itLeaf->second;
}

std::shared_ptr<hd::Leaf> hd::Group::getLeafById(const std::string &id) const
{
   for (auto& leaf : leaves_)
   {
      if (leaf.second->walletId() == id)
         return leaf.second;
   }

   return nullptr;
}

std::vector<std::shared_ptr<hd::Leaf>> hd::Group::getLeaves() const
{
   std::vector<std::shared_ptr<hd::Leaf>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::vector<std::shared_ptr<hd::Leaf>> hd::Group::getAllLeaves() const
{
   std::vector<std::shared_ptr<hd::Leaf>> result;
   result.reserve(leaves_.size());
   for (const auto &leaf : leaves_) {
      result.emplace_back(leaf.second);
   }
   return result;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(const bs::hd::Path &path
   , unsigned lookup)
{
   if (path.length() != 3) {
      throw std::runtime_error("invalid path length " + std::to_string(path.length()));
   }
   const auto aet = bs::hd::addressType(path.get(0));
   const auto leafIndex = path.get(2);
   return createLeaf(aet, leafIndex, lookup);
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(AddressEntryType aet
   , bs::hd::Path::Elem elem, unsigned lookup)
{
   const auto purpose = static_cast<bs::hd::Path::Elem>(bs::hd::purpose(aet));
   bs::hd::Path pathLeaf({ purpose | bs::hd::hardFlag, index_ | bs::hd::hardFlag });

   //leaves are always hardened
   elem |= bs::hd::hardFlag;
   pathLeaf.append(elem);
   if (getLeafByPath(pathLeaf) != nullptr) {
      throw std::runtime_error("leaf already exists");
   }
   try {
      auto result = newLeaf(aet);
      initLeaf(result, pathLeaf, lookup);
      addLeaf(result);
      commit();
      return result;
   }
   catch (std::exception &e) {
      throw e;
   }
   return nullptr;
}

std::shared_ptr<hd::Leaf> hd::Group::createLeaf(AddressEntryType aet
   , const std::string &key, unsigned lookup)
{
   return createLeaf(aet, bs::hd::Path::keyToElem(key), lookup);
}

bool hd::Group::addLeaf(const std::shared_ptr<hd::Leaf> &leaf)
{
   leaves_[leaf->path()] = leaf;
   needsCommit_ = true;
   return true;
}

bool hd::Group::deleteLeaf(const bs::hd::Path &path)
{
   const auto &leaf = getLeafByPath(path);
   if (leaf == nullptr) {
      return false;
   }
   leaves_.erase(leaf->path());
   needsCommit_ = true;
   return true;
}

bool hd::Group::deleteLeaf(const std::shared_ptr<bs::core::hd::Leaf> &wallet)
{
   bs::hd::Path path;
   bool found = false;
   for (const auto &leaf : leaves_) {
      if (leaf.second->walletId() == wallet->walletId()) {
         path = leaf.first;
         found = true;
         break;
      }
   }
   if (!found) {
      return false;
   }
   return deleteLeaf(path);
}

BinaryData hd::Group::serialize() const
{
   BinaryWriter bw;
   
   bw.put_uint32_t(index_ & ~bs::hd::hardFlag);
   bw.put_uint8_t(isExtOnly_);

   serializeLeaves(bw);
   return bw.getData();
}

void hd::Group::serializeLeaves(BinaryWriter &bw) const
{
   for (const auto &leaf : leaves_) {
      bw.put_uint32_t(LEAF_KEY);
      const BinaryData serLeaf = leaf.second->serialize();
      bw.put_var_int(serLeaf.getSize());
      bw.put_BinaryData(serLeaf);
   }
}

std::shared_ptr<hd::Group> hd::Group::deserialize(
   std::shared_ptr<AssetWallet_Single> walletPtr
   , BinaryDataRef key, BinaryDataRef value
   , const std::string &name, const std::string &desc
   , NetworkType netType, const std::shared_ptr<spdlog::logger> &logger)
{
   BinaryRefReader brrKey(key);
   auto prefix = brrKey.get_uint8_t();
   if (prefix != BS_GROUP_PREFIX) {
      return nullptr;
   }
   std::shared_ptr<hd::Group> group = nullptr;
   const auto grpType = static_cast<bs::hd::CoinType>(brrKey.get_uint32_t() | bs::hd::hardFlag);

   switch (grpType) {
   case bs::hd::CoinType::BlockSettle_Auth:
      group = std::make_shared<hd::AuthGroup>(
         walletPtr, netType, logger);
      break;

   case bs::hd::CoinType::Bitcoin_main:
   case bs::hd::CoinType::Bitcoin_test:
      //use a place holder for isExtOnly (false), set it 
      //while deserializing db value
      group = std::make_shared<hd::Group>(
         walletPtr, UINT32_MAX, netType, false, logger);
      break;

   case bs::hd::CoinType::BlockSettle_CC:
      group = std::make_shared<hd::CCGroup>(
         walletPtr, netType, logger);
      break;

   case bs::hd::CoinType::BlockSettle_Settlement:
      group = std::make_shared<hd::SettlementGroup>(
         walletPtr, netType, logger);
      break;

   default:
      throw WalletException("unknown group type " + std::to_string((uint32_t)grpType));
      break;
   }
   group->deserialize(value);
   return group;
}

std::shared_ptr<hd::Leaf> hd::Group::newLeaf(AddressEntryType aet) const
{
   switch (aet) {
   case AddressEntryType_P2WPKH:
   case AddressEntryType_Default:
      return std::make_shared<hd::LeafNative>(netType_, logger_, type());

   case AddressEntryType_P2SH:
   case static_cast<AddressEntryType>(AddressEntryType_P2SH | AddressEntryType_P2WPKH):
      return std::make_shared<hd::LeafNested>(netType_, logger_);

   case AddressEntryType_P2PKH:
      return std::make_shared<hd::LeafNonSW>(netType_, logger_);

   default:    break;
   }

   assert(false);
   return nullptr;
}

void hd::Group::initLeaf(
   std::shared_ptr<hd::Leaf> &leaf, const bs::hd::Path &path,
   unsigned lookup) const
{
   std::vector<unsigned> pathInt;
   for (unsigned i = 0; i < path.length(); i++) {
      pathInt.push_back(path.get(i));
   }
   //setup address account
   auto accTypePtr = std::make_shared<AccountType_BIP32_Custom>();
   
   //account IDs and nodes
   if (!isExtOnly_)
   {
      accTypePtr->setNodes({ hd::Leaf::addrTypeExternal_, hd::Leaf::addrTypeInternal_ });
      accTypePtr->setOuterAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
      accTypePtr->setInnerAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeInternal_));
   }
   else
   {
      //ext only address account uses the same asset account for both outer and 
      //inner chains
      accTypePtr->setNodes({ hd::Leaf::addrTypeExternal_ });
      accTypePtr->setOuterAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
      accTypePtr->setInnerAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
   }

   //address types
   accTypePtr->setAddressTypes(/*getAddressTypeSet()*/ { leaf->addressType() });
   accTypePtr->setDefaultAddressType(leaf->addressType());

   //address lookup
   if (lookup == UINT32_MAX) {
      lookup = DERIVATION_LOOKUP;
   }
   accTypePtr->setAddressLookup(lookup);

   //Lock the underlying armory wallet to allow accounts to derive their root from
   //the wallet's. We assume the passphrase prompt lambda is already set.
   auto lock = walletPtr_->lockDecryptedContainer();

   auto accID = walletPtr_->createBIP32Account(nullptr, pathInt, accTypePtr);
   leaf->setPath(path);
   leaf->init(walletPtr_, accID);
}

void hd::Group::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   index_ = brrVal.get_uint32_t() & ~bs::hd::hardFlag;
   isExtOnly_ = (bool)brrVal.get_uint8_t();

   while (brrVal.getSizeRemaining() > 0) {
      auto key = brrVal.get_uint32_t();
      if (key != LEAF_KEY) {
         throw AccountException("unexpected leaf type " + std::to_string(key));
      }
      const auto len = brrVal.get_var_int();
      const auto serLeaf = brrVal.get_BinaryData(len);
      auto leafPair = hd::Leaf::deserialize(serLeaf, netType_, logger_);

      auto leaf = leafPair.first;
      leaf->init(walletPtr_, leafPair.second);
      addLeaf(leaf);
   }
}

void hd::Group::shutdown()
{
   for (auto& leaf : leaves_)
      leaf.second->shutdown();
   leaves_.clear();

   if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
   }

   walletPtr_ = nullptr;
}

std::set<AddressEntryType> hd::Group::getAddressTypeSet(void) const
{              // disabled for now
   return { /*AddressEntryType_P2PKH,*/ AddressEntryType_P2WPKH,
      AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH)
      };
}

void hd::Group::commit(bool force)
{
   if (!force && !needsCommit()) {
      return;
   }
   BinaryWriter bwKey;
   bwKey.put_uint8_t(BS_GROUP_PREFIX);
   bwKey.put_uint32_t(index());

   BinaryWriter bwData;
   const auto serData = serialize();
   bwData.put_var_int(serData.getSize());
   bwData.put_BinaryData(serData);
   putDataToDB(bwKey.getData(), bwData.getData());
   committed();
}

void hd::Group::putDataToDB(const BinaryData& key, const BinaryData& data)
{
   if (walletPtr_ == nullptr) {
      throw WalletException("null wallet ptr");
   }
   CharacterArrayRef keyRef(key.getSize(), key.getPtr());
   CharacterArrayRef dataRef(data.getSize(), data.getPtr());

   auto envPtr = walletPtr_->getDbEnv();
   LMDBEnv::Transaction tx(envPtr.get(), LMDB::ReadWrite);
   db_->insert(keyRef, dataRef);
}

std::shared_ptr<hd::Group> hd::Group::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   if (wltPtr == nullptr) {
      throw AccountException("empty wlt ptr");
   }
   auto grpCopy = std::make_shared<hd::Group>(wltPtr
      , index_, netType_, isExtOnly_, logger_);

   for (auto& leafPair : leaves_) {
      auto leafCopy = leafPair.second->getCopy(wltPtr);
      grpCopy->addLeaf(leafCopy);
   }

   return grpCopy;
}


////////////////////////////////////////////////////////////////////////////////

hd::AuthGroup::AuthGroup(std::shared_ptr<AssetWallet_Single> walletPtr
   , NetworkType netType, const std::shared_ptr<spdlog::logger>& logger) :
   Group(walletPtr, bs::hd::CoinType::BlockSettle_Auth, netType, true, logger)
{}    //auth wallets are always ext only

void hd::AuthGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf
   , const bs::hd::Path &path, unsigned lookup) const
{
   auto authLeafPtr = std::dynamic_pointer_cast<hd::AuthLeaf>(leaf);
   if (authLeafPtr == nullptr) {
      throw AccountException("expected auth leaf ptr");
   }
   std::vector<unsigned> pathInt;
   for (unsigned i = 0; i < path.length(); i++) {
      pathInt.push_back(path.get(i));
   }
   //setup address account
   if (salt_.getSize() != 32) {
      throw AccountException("empty auth group salt");
   }
   auto accTypePtr = std::make_shared<AccountType_BIP32_Salted>(salt_);

   //account IDs and nodes
   if (!isExtOnly_) {
      accTypePtr->setNodes({ hd::Leaf::addrTypeExternal_, hd::Leaf::addrTypeInternal_ });
      accTypePtr->setOuterAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
      accTypePtr->setInnerAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeInternal_));
   }
   else {
      //ext only address account uses the same asset account for both outer and 
      //inner chains
      accTypePtr->setNodes({ hd::Leaf::addrTypeExternal_ });
      accTypePtr->setOuterAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
      accTypePtr->setInnerAccountID(WRITE_UINT32_BE(hd::Leaf::addrTypeExternal_));
   }

   //address types
   accTypePtr->setAddressTypes({ leaf->addressType() });
   accTypePtr->setDefaultAddressType(leaf->addressType());

   //address lookup
   if (lookup == UINT32_MAX) {
      lookup = DERIVATION_LOOKUP;
   }
   accTypePtr->setAddressLookup(lookup);

   //Lock the underlying armory wallet to allow accounts to derive their root from
   //the wallet's. We assume the passphrase prompt lambda is already set.
   auto lock = walletPtr_->lockDecryptedContainer();
   auto accID = walletPtr_->createBIP32Account(nullptr, pathInt, accTypePtr);
   
   authLeafPtr->setPath(path);
   authLeafPtr->init(walletPtr_, accID);
   authLeafPtr->setSalt(salt_);
}

std::shared_ptr<hd::Leaf> hd::AuthGroup::newLeaf(AddressEntryType) const
{
   return std::make_shared<hd::AuthLeaf>(netType_, nullptr);
}

bool hd::AuthGroup::addLeaf(const std::shared_ptr<Leaf> &leaf)
{
   return hd::Group::addLeaf(leaf);
}

void hd::AuthGroup::serializeLeaves(BinaryWriter &bw) const
{
   for (const auto &leaf : leaves_) {
      bw.put_uint32_t(AUTH_LEAF_KEY);
      const BinaryData serLeaf = leaf.second->serialize();
      bw.put_var_int(serLeaf.getSize());
      bw.put_BinaryData(serLeaf);
   }
}

void hd::AuthGroup::shutdown()
{
   hd::Group::shutdown();
}

std::set<AddressEntryType> hd::AuthGroup::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2WPKH };
}

void hd::AuthGroup::setSalt(const SecureBinaryData& salt)
{
   if (salt_.getSize() != 0)
      throw AccountException("salt already set");

   salt_ = salt;
}

BinaryData hd::AuthGroup::serialize() const
{
   BinaryWriter bw;

   bw.put_uint8_t(isExtOnly_);

   bw.put_var_int(salt_.getSize());
   bw.put_BinaryData(salt_);

   serializeLeaves(bw);

   return bw.getData();
}

void hd::AuthGroup::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   index_ = bs::hd::CoinType::BlockSettle_Auth;
   isExtOnly_ = (bool)brrVal.get_uint8_t();

   auto len = brrVal.get_var_int();
   salt_ = brrVal.get_BinaryData(len);

   while (brrVal.getSizeRemaining() > 0) {
      auto key = brrVal.get_uint32_t();
      if (key != AUTH_LEAF_KEY) {
         throw AccountException("unexpected leaf type");
      }
      len = brrVal.get_var_int();
      const auto serLeaf = brrVal.get_BinaryData(len);
      auto leafPair = hd::Leaf::deserialize(serLeaf, netType_, logger_);

      auto leaf = leafPair.first;
      leaf->init(walletPtr_, leafPair.second);
      addLeaf(leaf);
   }
}

std::shared_ptr<hd::Group> hd::AuthGroup::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   //use own walletPtr_ if wltPtr is null
   if (wltPtr == nullptr) {
      wltPtr = walletPtr_;
   }
   auto grpCopy = std::make_shared<hd::AuthGroup>(wltPtr, netType_, logger_);
   grpCopy->setSalt(salt_);

   for (auto& leafPair : leaves_)
   {
      auto leafCopy = leafPair.second->getCopy(wltPtr);
      grpCopy->addLeaf(leafCopy);
   }

   return grpCopy;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::CCGroup::newLeaf(AddressEntryType) const
{
   return std::make_shared<hd::CCLeaf>(netType_, logger_);
}

std::set<AddressEntryType> hd::CCGroup::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2WPKH };
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Leaf> hd::SettlementGroup::newLeaf(AddressEntryType) const
{
   return std::make_shared<hd::SettlementLeaf>(netType_, logger_);
}

std::set<AddressEntryType> hd::SettlementGroup::getAddressTypeSet(void) const
{
   return { AddressEntryType_P2WPKH };
}

void hd::SettlementGroup::initLeaf(std::shared_ptr<hd::Leaf> &leaf,
   const SecureBinaryData& privKey, const SecureBinaryData& pubKey) const
{
   auto settlLeafPtr = std::dynamic_pointer_cast<hd::SettlementLeaf>(leaf);
   if (settlLeafPtr == nullptr)
      throw AccountException("expected auth leaf ptr");

   //setup address account
   auto accTypePtr = std::make_shared<AccountType_ECDH>(privKey, pubKey);

   //address types
   accTypePtr->setAddressTypes({ leaf->addressType() });
   accTypePtr->setDefaultAddressType(leaf->addressType());

   //Lock the underlying armory wallet to allow accounts to derive their root from
   //the wallet's. We assume the passphrase prompt lambda is already set.
   auto lock = walletPtr_->lockDecryptedContainer();
   auto accPtr = walletPtr_->createAccount(accTypePtr);
   auto& accID = accPtr->getID();

   settlLeafPtr->init(walletPtr_, accID);
}

std::shared_ptr<hd::Leaf> hd::SettlementGroup::createLeaf(
   const bs::Address &addr, const bs::hd::Path &path)
{
   /*
   We assume this address belongs to our wallet. We will try to recover the
   asset for that address and extract the key pair to init the ECDH account 
   with. 

   The path argument is not useful on its own as ECDH accounts are not 
   deterministic. However, leaves are keyed by their path within groups, 
   therefor the path provided should be that of the address the account is
   build from.

   Sadly there is no way for a group to resolve the path of addresses 
   belonging to other groups, therefor this method is private and the class
   friends HDWallet, which implements the method to create a leaf from an 
   address and feed the proper path to this method.
   */

   //grab asset id for address
   auto& idPair = walletPtr_->getAssetIDForAddr(addr.prefixed());
   auto assetPtr = walletPtr_->getAssetForID(idPair.first);
   auto assetSingle = std::dynamic_pointer_cast<AssetEntry_Single>(assetPtr);
   if (assetSingle == nullptr)
      throw AssetException("cannot create settlement leaf from this asset type");

   //create the leaf
   auto leaf = newLeaf(AddressEntryType_Default);

   //initialize it
   if (!walletPtr_->isWatchingOnly()) {
      /*
      Full wallet, grab the decrypted private key for this asset. The wallet
      has to be locked for decryption and the passphrase lambda set for this
      to succeed.
      */

      auto& privKey = walletPtr_->getDecryptedPrivateKeyForAsset(assetSingle);
      initLeaf(leaf, privKey, SecureBinaryData());
   }
   else {
      //wo wallet, create ECDH account from the compressed pubkey
      const auto &pubKeyObj = assetSingle->getPubKey();
      initLeaf(leaf, SecureBinaryData(), pubKeyObj->getCompressedKey());
   }

   leaf->setPath(path);

   //add the leaf
   leaves_[path] = leaf;
   needsCommit_ = true;
   commit();

   return leaf;
}

void hd::SettlementGroup::serializeLeaves(BinaryWriter &bw) const
{
   for (const auto &leaf : leaves_) {
      bw.put_uint32_t(SETTLEMENT_LEAF_KEY);
      const BinaryData serLeaf = leaf.second->serialize();
      bw.put_var_int(serLeaf.getSize());
      bw.put_BinaryData(serLeaf);
   }
}

void hd::SettlementGroup::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   index_ = brrVal.get_uint32_t() & ~bs::hd::hardFlag;
   isExtOnly_ = (bool)brrVal.get_uint8_t();

   while (brrVal.getSizeRemaining() > 0) {
      auto key = brrVal.get_uint32_t();
      if (key != SETTLEMENT_LEAF_KEY) {
         throw AccountException("unexpected leaf type");
      }
      const auto len = brrVal.get_var_int();
      const auto serLeaf = brrVal.get_BinaryData(len);
      auto leafPair = hd::Leaf::deserialize(serLeaf, netType_, logger_);

      auto leaf = leafPair.first;
      leaf->init(walletPtr_, leafPair.second);
      leaves_[leaf->path()] = leaf;
   }
}

std::shared_ptr<hd::SettlementLeaf> hd::SettlementGroup::getLeafForSettlementID(
   const SecureBinaryData& id) const
{
   for (auto& leafPair : leaves_) {
      auto settlLeaf = std::dynamic_pointer_cast<hd::SettlementLeaf>(
         leafPair.second);
      
      if (settlLeaf == nullptr) {
         throw AccountException("unexpected leaf type");
      }
      if (settlLeaf->getIndexForSettlementID(id) != UINT32_MAX) {
         return settlLeaf;
      }
   }
   return nullptr;
}

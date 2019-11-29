/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <unordered_map>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "CoreHDLeaf.h"
#include "Wallets.h"

#define ADDR_KEY     0x00002002

using namespace bs::core;

hd::Leaf::Leaf(NetworkType netType,
   std::shared_ptr<spdlog::logger> logger, 
   wallet::Type type)
   : Wallet(logger), netType_(netType), type_(type)
{}

hd::Leaf::~Leaf()
{
   shutdown();
}

void hd::Leaf::init(
   std::shared_ptr<AssetWallet_Single> walletPtr,
   const BinaryData& addrAccId)
{
   reset();
   auto accPtr = walletPtr->getAccountForID(addrAccId);
   if (accPtr == nullptr)
      throw WalletException("invalid account id");

   accountPtr_ = accPtr;
   walletPtr_ = walletPtr;

   auto&& addrMap = accountPtr_->getUsedAddressMap();
   for (auto& addrPair : addrMap)
   {
      auto&& bsAddr = bs::Address::fromAddressEntry(*addrPair.second);
      usedAddresses_.emplace_back(bsAddr);
   }
}

void hd::Leaf::setPath(const bs::hd::Path& path)
{
   if (path == path_)
      return;
 
   path_ = path;
   suffix_.clear();
   suffix_ = bs::hd::Path::elemToKey(index());

   walletName_ = path_.toString();
}

void hd::Leaf::reset()
{
   usedAddresses_.clear();
   addressHashes_.clear();
   accountPtr_ = nullptr;
}

std::shared_ptr<DBIfaceTransaction> hd::Leaf::getDBWriteTx()
{
   return walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, true);
}

std::shared_ptr<DBIfaceTransaction> hd::Leaf::getDBReadTx()
{
   return walletPtr_->beginSubDBTransaction(BS_WALLET_DBNAME, false);
}

std::string hd::Leaf::walletId() const
{
   if (walletId_.empty()) {

      walletId_ = wallet::computeID(getRootId()).toBinStr();
   }
   return walletId_;
}

bool hd::Leaf::containsAddress(const bs::Address &addr)
{
   return (addressIndex(addr) != UINT32_MAX);
}

bool hd::Leaf::containsHiddenAddress(const bs::Address &addr) const
{
   try
   {
      auto& addrPair = accountPtr_->getAssetIDPairForAddr(addr.prefixed());
      if (addrPair.first.getSize() != 0)
         return true;
   }
   catch(std::exception&)
   { }

   return false;
}

BinaryData hd::Leaf::getRootId() const
{
   return accountPtr_->getID();
}

std::vector<bs::Address> hd::Leaf::getPooledAddressList() const
{
   auto& hashMap = accountPtr_->getAddressHashMap();

   std::vector<bs::Address> result;
   for (auto& hashPair : hashMap)
      result.emplace_back(bs::Address::fromHash(hashPair.first));

   return result;
}

ReentrantLock hd::Leaf::lockDecryptedContainer()
{
   return walletPtr_->lockDecryptedContainer();
}

// Return an external-facing address.
bs::Address hd::Leaf::getNewExtAddress()
{
   return newAddress();
}

// Return an internal-facing address.
bs::Address hd::Leaf::getNewIntAddress()
{
   return newInternalAddress();
}

// Return a change address.
bs::Address hd::Leaf::getNewChangeAddress()
{
   return newInternalAddress();
}

std::shared_ptr<AddressEntry> hd::Leaf::getAddressEntryForAddr(const BinaryData &addr)
{
   auto& addrMap = accountPtr_->getAddressHashMap();
   auto iter = addrMap.find(addr);
   if (iter == addrMap.end())
      return nullptr;

   auto assetPtr = walletPtr_->getAssetForID(iter->second.first);
   auto addrPtr = AddressEntry::instantiate(assetPtr, iter->second.second);
   return addrPtr;
}

SecureBinaryData hd::Leaf::getPublicKeyFor(const bs::Address &addr)
{
   auto idPair = accountPtr_->getAssetIDPairForAddr(addr.prefixed());
   auto assetPtr = accountPtr_->getAssetForID(idPair.first.getSliceRef(4, 8));
   
   auto assetSingle = std::dynamic_pointer_cast<AssetEntry_Single>(assetPtr);
   if (assetSingle == nullptr)
      throw AccountException("unexpected asset entry type");

   return assetSingle->getPubKey()->getCompressedKey();
}

bs::Address hd::Leaf::newAddress()
{
   auto addrPtr = accountPtr_->getNewAddress(addressType());

   //this will not work with MS assets nor P2PK (the output script does not use a hash)
   auto&& addr = bs::Address::fromAddressEntry(*addrPtr);
   usedAddresses_.push_back(addr);
   return addr;
}

bs::Address hd::Leaf::newInternalAddress()
{
   auto addrPtr = accountPtr_->getNewChangeAddress(addressType());

   //this will not work with MS assets nor P2PK (the output script does not use a hash)
   auto&& addr = bs::Address::fromAddressEntry(*addrPtr);
   usedAddresses_.push_back(addr);
   return addr;
}

void hd::Leaf::topUpAddressPool(size_t count, bool intExt)
{
   //intExt: true for external, false for internal
   BinaryData accountID;

   if (intExt)
      accountID = accountPtr_->getOuterAccountID();
   else
      accountID = accountPtr_->getInnerAccountID();

   auto accMap = accountPtr_->getAccountMap();
   auto iter = accMap.find(accountID);
   if (iter == accMap.end())
      throw AccountException("unexpected account id");

   iter->second->extendPublicChain(count);
}

std::vector<bs::Address> hd::Leaf::extendAddressChain(unsigned count, bool extInt)
{
   //get previous hash map
   auto addrHashMap_orig = accountPtr_->getAddressHashMap();

   //extend
   topUpAddressPool(count, extInt);

   //get new hash map
   auto& addrHashMap_new = accountPtr_->getAddressHashMap();

   //get diff
   std::map<BinaryData, std::pair<BinaryData, AddressEntryType>> diffMap;
   std::set_difference(
      addrHashMap_new.begin(), addrHashMap_new.end(),
      addrHashMap_orig.begin(), addrHashMap_orig.end(),
      std::inserter(diffMap, diffMap.begin()));

   //convert to address
   std::vector<bs::Address> result;
   for (auto& hashPair : diffMap)
   {
      auto&& addr = bs::Address::fromHash(hashPair.first);
      result.emplace_back(addr);
   }

   return result;
}

bs::hd::Path::Elem hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   /***
   Do not use this method as it may drop the address entry type if a hash is
   passed without the address prefix
   ***/

   throw std::runtime_error("deprecated 3");

   auto&& addrObj = bs::Address::fromHash(addr);
   auto path = getPathForAddress(addrObj);
   if (path.length() == 0)
      return UINT32_MAX;

   return path.get(-1);
}

bs::hd::Path::Elem hd::Leaf::addressIndex(const bs::Address &addr) const
{
   auto path = getPathForAddress(addr);
   if (path.length() == 0)
      return UINT32_MAX;

   return path.get(-1);
}

bs::hd::Path hd::Leaf::getPathForAddress(const bs::Address &addr) const
{
   //grab assetID by prefixed address hash
   try
   {
      auto& assetIDPair = accountPtr_->getAssetIDPairForAddr(addr.prefixed());

      //assetID: BIP32 root ID (4 bytes) | BIP32 node id (4 bytes) | asset index (4 bytes)
      BinaryRefReader brr(assetIDPair.first);
      brr.get_uint32_t(); //skip root id
      auto nodeid = brr.get_uint32_t(BE);
      auto indexid = brr.get_uint32_t(BE);

      bs::hd::Path addrPath;
      addrPath.append(nodeid);
      addrPath.append(indexid);

      return addrPath;
   }
   catch (std::exception&) {
      return {};
   }
}

std::string hd::Leaf::getAddressIndex(const bs::Address &addr)
{
   return getPathForAddress(addr).toString();
}

bool hd::Leaf::isExternalAddress(const bs::Address &addr) const
{
   const auto &path = getPathForAddress(addr);
   if (path.length() < 2) {
      return false;
   }
   return (path.get(-2) == addrTypeExternal_);
}

bs::Address hd::Leaf::getAddressByIndex(unsigned int id, bool ext) const
{
   BinaryWriter accBw;
   accBw.put_BinaryData(accountPtr_->getID());
   if (ext) {
      accBw.put_BinaryData(accountPtr_->getOuterAccountID());
   }
   else {
      accBw.put_BinaryData(accountPtr_->getInnerAccountID());
   }
   accBw.put_uint32_t(id, BE);

   auto addrPtr = accountPtr_->getAddressEntryForID(accBw.getDataRef());
   if (addrPtr->getType() != addressType()) {
      throw AccountException("type mismatch for instantiated address " + std::to_string(id));
   }

   return bs::Address::fromAddressEntry(*addrPtr);
}

bs::hd::Path::Elem hd::Leaf::getLastAddrPoolIndex() const
{
   auto accPtr = accountPtr_->getOuterAccount();
   bs::hd::Path::Elem result = (uint32_t)accPtr->getAssetCount() - 1;
   return result;
}

BinaryData hd::Leaf::serialize() const
{
   BinaryWriter bw;

   // format revision - should always be <= 10
   bw.put_uint32_t(2);

   bw.put_uint32_t(LEAF_KEY);

   //address account id
   auto&& rootID = getRootId();
   bw.put_var_int(rootID.getSize());
   bw.put_BinaryData(rootID);

   //path
   bw.put_var_int(path_.length());
   for (unsigned i = 0; i < path_.length(); i++) {
      bw.put_uint32_t(path_.get(i));
   }
   return bw.getData();
}

std::pair<std::shared_ptr<hd::Leaf>, BinaryData> hd::Leaf::deserialize(
   const BinaryData &ser, NetworkType netType, std::shared_ptr<spdlog::logger> logger)
{
   BinaryRefReader brr(ser);

   //version
   auto ver = brr.get_uint32_t();
   if (ver != 2) {
      throw WalletException("unexpected leaf version " + std::to_string(ver));
   }
   //type
   auto key = brr.get_uint32_t();

   //address account id
   auto len = brr.get_var_int();
   auto id = brr.get_BinaryData(len);

   //path
   auto count = brr.get_var_int();
   bs::hd::Path path;
   for (unsigned i = 0; i < count; i++) {
      path.append(brr.get_uint32_t());
   }
   if (path.length() < 3) {
      throw AccountException("invalid path length " + std::to_string(path.length()));
   }
   std::shared_ptr<hd::Leaf> leafPtr;

   switch (key) {
   case LEAF_KEY:
   {
      const auto groupType = static_cast<bs::hd::CoinType>(path.get(-2) | bs::hd::hardFlag);

      switch (static_cast<bs::hd::Purpose>(path.get(0) & ~bs::hd::hardFlag)) {
      case bs::hd::Purpose::Native:
         if (groupType == bs::hd::CoinType::BlockSettle_CC) {
            leafPtr = std::make_shared<hd::CCLeaf>(netType, logger);
         }
         else {
            leafPtr = std::make_shared<hd::LeafNative>(netType, logger);
         }
         break;
      case bs::hd::Purpose::Nested:
         leafPtr = std::make_shared<hd::LeafNested>(netType, logger);
         break;
      case bs::hd::Purpose::NonSegWit:
         leafPtr = std::make_shared<hd::LeafNonSW>(netType, logger);
         break;
      default:
         throw AccountException("unknown XBT leaf type " + std::to_string(path.get(0)));
      }
      break;
   }

   case AUTH_LEAF_KEY:
   {
      len = brr.get_var_int();
      auto&& salt = brr.get_BinaryData(len);

      auto authPtr = std::make_shared<hd::AuthLeaf>(netType, logger);
      authPtr->setSalt(salt);
      leafPtr = authPtr;
      break;
   }

   case SETTLEMENT_LEAF_KEY:
   {
      leafPtr = std::make_shared<hd::SettlementLeaf>(netType, logger);
      break;
   }

   default:
      throw AccountException("unknown leaf type " + std::to_string(key));
   }

   leafPtr->setPath(path);
   return std::make_pair(leafPtr, id);
}

std::shared_ptr<ResolverFeed> hd::Leaf::getResolver() const
{
   return std::make_shared<ResolverFeed_AssetWalletSingle>(walletPtr_);
}

std::shared_ptr<ResolverFeed> hd::Leaf::getPublicResolver() const
{
   class PublicResolver : public ResolverFeed_AssetWalletSingle
   {
   public:
      PublicResolver(const std::shared_ptr<AssetWallet_Single> &walletPtr)
         : ResolverFeed_AssetWalletSingle(walletPtr)  { }

      const SecureBinaryData &getPrivKeyForPubkey(const BinaryData &pk) override
      {
         throw std::runtime_error("not supported");
      }
   };
   return std::make_shared<PublicResolver>(walletPtr_);
}

bool hd::Leaf::isWatchingOnly() const
{
   auto rootPtr = accountPtr_->getOutterAssetRoot();
   return !rootPtr->hasPrivateKey();
}

bool hd::Leaf::hasExtOnlyAddresses() const
{
   return (accountPtr_->getInnerAccountID() == 
      accountPtr_->getOuterAccountID());
}

std::vector<bs::Address> hd::Leaf::getExtAddressList() const
{
   auto& addressMap = accountPtr_->getOuterAccount()->getAddressHashMap(
         accountPtr_->getAddressTypeSet());

   std::vector<bs::Address> addrVec;
   for (auto& addrPair : addressMap)
   {
      for (auto& innerPair : addrPair.second)
      {
         auto&& bsAddr = bs::Address::fromHash(innerPair.second);
         addrVec.emplace_back(bsAddr);
      }
   }

   return addrVec;
}

unsigned hd::Leaf::getUsedAddressCount() const
{
   return getExtAddressCount() + getIntAddressCount();
}

unsigned hd::Leaf::getExtAddressCount() const
{
   return accountPtr_->getOuterAccount()->getHighestUsedIndex() + 1;
}

std::vector<bs::Address> hd::Leaf::getIntAddressList() const
{
   auto& accID = accountPtr_->getInnerAccountID();
   auto& accMap = accountPtr_->getAccountMap();
   auto iter = accMap.find(accID);
   if (iter == accMap.end())
      throw WalletException("invalid inner account id");

   auto& addressMap = iter->second->getAddressHashMap(
         accountPtr_->getAddressTypeSet());

   std::vector<bs::Address> addrVec;
   for (auto& addrPair : addressMap)
   {
      for (auto& innerPair : addrPair.second)
      {
         auto&& bsAddr = bs::Address::fromHash(innerPair.second);
         addrVec.emplace_back(bsAddr);
      }
   }

   return addrVec;
}

unsigned hd::Leaf::getIntAddressCount() const
{
   auto& accID = accountPtr_->getInnerAccountID();

   //return 0 if the address account does not have an inner chain
   if (accountPtr_->getOuterAccountID() == accID)
      return 0;

   auto& accMap = accountPtr_->getAccountMap();
   auto iter = accMap.find(accID);
   if (iter == accMap.end())
      throw WalletException("invalid inner account id");

   return iter->second->getHighestUsedIndex() + 1;
}

std::string hd::Leaf::getFilename() const
{
   if (walletPtr_ == nullptr)
      throw WalletException("uninitialized wallet");
   return walletPtr_->getDbFilename();
}

void hd::Leaf::shutdown()
{
   walletPtr_ = nullptr;
   accountPtr_ = nullptr;
}

std::pair<bs::Address, bool> hd::Leaf::synchronizeUsedAddressChain(
   const std::string& index)
{
   //decode index to path
   auto&& path = bs::hd::Path::fromString(index);

   //does path belong to our leaf?
   if (path.isAbsolute())
   {
      if (path.length() != path_.length() - 2)
         throw AccountException("address path does not belong to leaf");

      //compare path base
      for (int i = 0; i < path_.length() - 2; i++)
      {
         if (path.get(i) != path_.get(i))
            throw AccountException("address path differs from leaf path");
      }

      //shorten path to non hardened elements
      bs::hd::Path pathShort;
      for (int i = 0; i < 2; i++)
         pathShort.append(path.get(2 - i));

      path = pathShort;
   }

   //is it internal or external?
   bool ext = true;
   auto elem = path.get(-2);
   if (elem == addrTypeInternal_)
      ext = false;
   else if (elem != addrTypeExternal_)
      throw AccountException("invalid address path");

   //is the path ahead of the underlying Armory wallet used index?
   unsigned topIndex;
   if (ext)
      topIndex = getExtAddressCount() - 1;
   else
      topIndex = getIntAddressCount() - 1;
   unsigned addrIndex = path.get(-1);

   std::pair<bs::Address, bool> result;
   int gap; //do not change to unsigned, gap needs to be signed
   if (topIndex != UINT32_MAX && addrIndex <= topIndex)
      gap = -1;
   else
      gap = addrIndex - topIndex; //this is correct wrt to result sign

   if (gap <= 0) {
      //already created this address, grab it, check the type matches
      result.first = getAddressByIndex(addrIndex, ext);
      result.second = false;
   }
   else {
      std::shared_ptr<AddressEntry> addrPtr;
      if (ext) {
         //pull new addresses to fill the gap, using the default type
         for (int i = 1; i < gap; i++) {
            getNewExtAddress();
         }
         //pull the new address using the requested type
         result.first = getNewExtAddress();
      }
      else {
         for (int i = 1; i < gap; i++) {
            getNewIntAddress();
         }
         result.first = getNewIntAddress();
      }
   }

   //sanity check: index and type should match request
   // Temporarily disabled because for P2SH+P2WPKH getType() returns only P2SH
#if 0
   if (result.first.getType() != addressType()) {
      throw AccountException("did not get expected address entry type "
         + std::to_string((int)addressType()) + " (got "
         + std::to_string(int(result.first.getType())) + ")");
   }
#endif //0
   auto resultIndex = addressIndex(result.first);
   if (resultIndex != addrIndex) {
      throw AccountException("did not get expected address index");
   }
   result.second = true;
   return result;
}

std::map<BinaryData, bs::hd::Path> hd::Leaf::indexPath(const std::set<BinaryData> &addrSet)
{
   std::map<BinaryData, bs::hd::Path> result;
   auto& addrHashMap = accountPtr_->getAddressHashMap();

   for (const auto &addr : addrSet) {
      auto iter = addrHashMap.find(addr);
      if (iter == addrHashMap.end()) {
         throw AccountException("unknown scrAddr");
      }
      bs::hd::Path path;

      /*
      IDs in addrHashMap are always 12 bytes long:
        AddressAccountID (4) | AssetAccountID (4) | AssetID (4)
      */
      BinaryRefReader brr(iter->second.first);
      brr.advance(4);

      //account id (ext/int)
      path.append(bs::hd::Path::Elem(brr.get_uint32_t(BE)));
      
      //asset id
      path.append(bs::hd::Path::Elem(brr.get_uint32_t(BE)));

      result.emplace(std::move(addr), std::move(path));
   }

   return result;
}

std::shared_ptr<AssetEntry> hd::Leaf::getRootAsset() const
{
   if (accountPtr_ == nullptr)
      throw AccountException("null account ptr");
   
   auto rootPtr = accountPtr_->getOutterAssetRoot();
   return rootPtr;
}

void hd::Leaf::readMetaData()
{
   MetaData::readFromDB(getDBReadTx());
}


std::shared_ptr<hd::Leaf> hd::LeafNative::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   if (wltPtr == nullptr) {
      throw AccountException("empty wallet ptr");
   }
   auto leafCopy = std::make_shared<hd::LeafNative>(netType_, logger_, type());
   leafCopy->setPath(path());
   leafCopy->init(wltPtr, getRootId());

   return leafCopy;
}

std::shared_ptr<hd::Leaf> hd::LeafNested::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   if (wltPtr == nullptr) {
      throw AccountException("empty wallet ptr");
   }
   auto leafCopy = std::make_shared<hd::LeafNested>(netType_, logger_);
   leafCopy->setPath(path());
   leafCopy->init(wltPtr, getRootId());

   return leafCopy;
}

std::shared_ptr<hd::Leaf> hd::LeafNonSW::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   if (wltPtr == nullptr) {
      throw AccountException("empty wallet ptr");
   }
   auto leafCopy = std::make_shared<hd::LeafNonSW>(netType_, logger_);
   leafCopy->setPath(path());
   leafCopy->init(wltPtr, getRootId());

   return leafCopy;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger)
   : LeafNative(netType, logger, wallet::Type::Authentication)
{}

BinaryData hd::AuthLeaf::serialize() const
{
   BinaryWriter bw;

   // format revision - should always be <= 10
   bw.put_uint32_t(2);

   //type
   bw.put_uint32_t(AUTH_LEAF_KEY);

   //address account id
   auto&& rootID = getRootId();
   bw.put_var_int(rootID.getSize());
   bw.put_BinaryData(rootID);

   //path
   bw.put_var_int(path_.length());
   for (unsigned i = 0; i < path_.length(); i++) {
      bw.put_uint32_t(path_.get(i));
   }

   //salt
   bw.put_var_int(salt_.getSize());
   bw.put_BinaryData(salt_);

   return bw.getData();
}

std::shared_ptr<hd::Leaf> hd::AuthLeaf::getCopy(
   std::shared_ptr<AssetWallet_Single> wltPtr) const
{
   if (wltPtr == nullptr)
      throw AccountException("empty wallet ptr");

   auto leafCopy = std::make_shared<hd::AuthLeaf>(netType_, logger_);
   leafCopy->setSalt(salt_);
   leafCopy->setPath(path());
   leafCopy->init(wltPtr, getRootId());

   return leafCopy;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BinaryData hd::SettlementLeaf::serialize() const
{
   BinaryWriter bw;

   // format revision - should always be <= 10
   bw.put_uint32_t(2);

   bw.put_uint32_t(SETTLEMENT_LEAF_KEY);

   //address account id
   auto&& rootID = getRootId();
   bw.put_var_int(rootID.getSize());
   bw.put_BinaryData(rootID);

   //path
   bw.put_var_int(path_.length());
   for (unsigned i = 0; i < path_.length(); i++) {
      bw.put_uint32_t(path_.get(i));
   }
   return bw.getData();
}

unsigned hd::SettlementLeaf::addSettlementID(const SecureBinaryData& id)
{
   auto assetAcc = std::dynamic_pointer_cast<AssetAccount_ECDH>(
      accountPtr_->getOuterAccount());
   if (assetAcc == nullptr)
      throw AccountException("unexpected settlement asset account type");

   return assetAcc->addSalt(id);
}

unsigned hd::SettlementLeaf::getIndexForSettlementID(const SecureBinaryData& id) const
{
   try {
      auto accountPtr = accountPtr_->getOuterAccount();
      auto accountEcdh = std::dynamic_pointer_cast<AssetAccount_ECDH>(accountPtr);
      if (accountEcdh == nullptr) {
         throw AccountException("unexpected account type");
      }
      return accountEcdh->getSaltIndex(id);
   }
   catch(DerivationSchemeException&)
   {}

   return UINT32_MAX;
}

#include <unordered_map>
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "CoreHDLeaf.h"
#include "CoreHDNode.h"
#include "Wallets.h"

#define ADDR_KEY     0x00002002
const uint32_t kExtConfCount = 6;
const uint32_t kIntConfCount = 1;

using namespace bs::core;

hd::Leaf::Leaf(NetworkType netType,
   std::shared_ptr<spdlog::logger> logger, 
   wallet::Type type)
   : Wallet(logger), netType_(netType), type_(type)
{}

hd::Leaf::~Leaf()
{
   if (db_ != nullptr)
      delete db_;
}

void hd::Leaf::init(
   std::shared_ptr<AssetWallet_Single> walletPtr,
   const BinaryData& addrAccId,
   const bs::hd::Path &path)
{
   if (path != path_) {
      path_ = path;
      suffix_.clear();
      suffix_ = bs::hd::Path::elemToKey(index());

      walletName_ = path_.toString();
   }

   reset();
   auto accPtr = walletPtr->getAccountForID(addrAccId);
   if (accPtr == nullptr)
      throw WalletException("invalid account id");

   accountPtr_ = accPtr;
   db_ = new LMDB(accountPtr_->getDbEnv().get(), BS_WALLET_DBNAME);
   walletPtr_ = walletPtr;

   auto&& addrMap = accountPtr_->getUsedAddressMap();
   for (auto& addrPair : addrMap)
   {
      bs::Address bsAddr(addrPair.second->getHash(), addrPair.second->getType());
      usedAddresses_.emplace_back(bsAddr);
   }
}

bool hd::Leaf::copyTo(std::shared_ptr<hd::Leaf> &leaf) const
{
   leaf->reset();
   leaf->accountPtr_ = accountPtr_;
   return true;
}

void hd::Leaf::reset()
{
   usedAddresses_.clear();
   addressHashes_.clear();
   accountPtr_ = nullptr;
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
      result.emplace_back(bs::Address(hashPair.first, hashPair.second.second));

   return result;
}

// Return an external-facing address.
bs::Address hd::Leaf::getNewExtAddress(AddressEntryType aet)
{
   return newAddress(aet);
}

// Return an internal-facing address.
bs::Address hd::Leaf::getNewIntAddress(AddressEntryType aet)
{
   return newInternalAddress(aet);
}

// Return a change address.
bs::Address hd::Leaf::getNewChangeAddress(AddressEntryType aet)
{
   return newInternalAddress(aet);
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

std::shared_ptr<hd::Node> hd::Leaf::getNodeForAddr(const bs::Address &addr) const
{
   if (addr.isNull()) {
      return nullptr;
   }

   auto& addrMap = accountPtr_->getAddressHashMap();
   auto iter = addrMap.find(addr);
   if (iter == addrMap.end())
      return nullptr;

   auto assetPtr = walletPtr_->getAssetForID(iter->second.first);
   throw std::runtime_error("deprecated 1");

   //TODO: instantiate hd::Node from asset entry
   return nullptr;
}

SecureBinaryData hd::Leaf::getPublicKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubCompressedKey();
}

SecureBinaryData hd::Leaf::getPubChainedKeyFor(const bs::Address &addr)
{
   const auto node = getNodeForAddr(addr);
   if (node == nullptr) {
      return BinaryData();
   }
   return node->pubChainedKey();
}

KeyPair hd::Leaf::getKeyPairFor(const bs::Address &addr)
{
   auto& node = getNodeForAddr(addr);
   if (node == nullptr)
      return {};

   return { node->privChainedKey(), node->pubChainedKey() };
}

bs::Address hd::Leaf::newAddress(AddressEntryType aet)
{
   auto addrPtr = accountPtr_->getNewAddress(aet);

   //this will not work with MS assets nor P2PK (the output script does not use a hash)
   auto addr = Address(addrPtr->getHash(), aet);
   usedAddresses_.push_back(addr);
   return addr;
}

bs::Address hd::Leaf::newInternalAddress(AddressEntryType aet)
{
   auto addrPtr = accountPtr_->getNewChangeAddress(aet);

   //this will not work with MS assets nor P2PK (the output script does not use a hash)
   auto addr = Address(addrPtr->getHash(), aet);
   usedAddresses_.push_back(addr);
   return Address(addrPtr->getHash(), aet);
}


std::vector<hd::Leaf::PooledAddress> hd::Leaf::generateAddresses(
   bs::hd::Path::Elem prefix, bs::hd::Path::Elem start, size_t nb, AddressEntryType aet)
{
   std::vector<PooledAddress> result;
   result.reserve(nb);
   for (bs::hd::Path::Elem i = start; i < start + nb; i++) {
      bs::hd::Path addrPath({ prefix, i });
      const auto &addr = newAddress(aet);
      if (!addr.isNull()) {
         result.emplace_back(PooledAddress({ addrPath, aet }, addr));
      }
   }
   return result;
}

void hd::Leaf::topUpAddressPool(size_t count)
{
   accountPtr_->extendPublicChain(count);
}

std::shared_ptr<AddressEntry> hd::Leaf::getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
   , AddressEntryType ae_type)
{
   if (ae_type == AddressEntryType_Default) {
      ae_type = accountPtr_->getAddressType();
   }

   std::shared_ptr<AddressEntry> aePtr = nullptr;
   switch (ae_type)
   {
   case AddressEntryType_P2PKH:
      aePtr = std::make_shared<AddressEntry_P2PKH>(assetPtr, true);
      break;

   case AddressEntryType_P2SH:
      {
         const auto nested = std::make_shared<AddressEntry_P2WPKH>(assetPtr);
         aePtr = std::make_shared<AddressEntry_P2SH>(nested);
      }
      break;

   case AddressEntryType_P2WPKH:
      aePtr = std::make_shared<AddressEntry_P2WPKH>(assetPtr);
      break;

   default:
      throw WalletException("unsupported address entry type");
   }

   return aePtr;
}

bs::hd::Path::Elem hd::Leaf::getAddressIndexForAddr(const BinaryData &addr) const
{
   /***
   Do not use this method as it may drop the address entry type if a hash is
   passed without the address prefix
   ***/

   throw std::runtime_error("deprecated 3");

   Address addrObj(addr);
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
   catch (std::exception&)
   {
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

bool hd::Leaf::addressIndexExists(const std::string &index) const
{
   const auto path = bs::hd::Path::fromString(index);
   if (path.length() < 2) {
      return false;
   }
   auto&& account_id = WRITE_UINT32_BE(path.get(-2));
   auto& accountMap = accountPtr_->getAccountMap();
   auto iter = accountMap.find(account_id);
   if (iter == accountMap.end())
      return false;

   auto assetId = WRITE_UINT32_BE(path.get(-1));
   try
   {
      if (iter->second->getAssetForID(assetId) != nullptr)
         return true;
   }
   catch(std::exception&)
   { }

   return false;
}

bs::Address hd::Leaf::getAddressByIndex(
   unsigned id, bool extInt, AddressEntryType aet) const
{
   //extInt: true for external/outer account, false for internal/inner account

   BinaryWriter accBw;
   accBw.put_uint32_t(0);
   if (extInt)
      accBw.put_BinaryData(accountPtr_->getOuterAccountID());
   else
      accBw.put_BinaryData(accountPtr_->getInnerAccountID());
   accBw.put_uint32_t(id, BE);

   auto addrPtr = accountPtr_->getAddressEntryForID(accBw.getDataRef());
   if (aet == AddressEntryType_Default)
   {
      if (addrPtr->getType() != accountPtr_->getAddressType())
         throw AccountException("type mismatch for instantiated address");
   }
   else if (addrPtr->getType() != aet)
   {
      throw AccountException("type mismatch for instantiated address");
   }

   return bs::Address(addrPtr->getHash(), aet);
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

   //address account id
   bw.put_var_int(accountPtr_->getID().getSize());
   bw.put_BinaryData(accountPtr_->getID());

   //path
   bw.put_var_int(path_.length());
   for (unsigned i = 0; i < path_.length(); i++)
      bw.put_uint32_t(path_.get(i));

   //size wrapper
   BinaryWriter finalBW;
   finalBW.put_var_int(bw.getSize());
   finalBW.put_BinaryData(bw.getData());
   return finalBW.getData();
}

std::pair<BinaryData, bs::hd::Path> hd::Leaf::deserialize(const BinaryData &ser)
{
   BinaryRefReader brr(ser);

   //version
   auto ver = brr.get_uint32_t();
   if (ver != 2)
      throw WalletException("unexpected leaf version");

   //address account id
   auto len = brr.get_var_int();
   auto id = brr.get_BinaryData(len);

   //path
   auto count = brr.get_var_int();
   bs::hd::Path path;
   for (unsigned i = 0; i < count; i++)
      path.append(brr.get_uint32_t());

   return std::make_pair(id, path);
}

std::shared_ptr<ResolverFeed> hd::Leaf::getResolver() const
{
   return std::make_shared<ResolverFeed_AssetWalletSingle>(walletPtr_);
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
   auto& addressMap =
      accountPtr_->getOuterAccount()->getAddressHashMap(
         accountPtr_->getAddressTypeSet());

   std::vector<bs::Address> addrVec;
   for (auto& addrPair : addressMap)
   {
      for (auto& innerPair : addrPair.second)
      {
         bs::Address bsAddr(innerPair.second, innerPair.first);
         addrVec.emplace_back(bsAddr);
      }
   }

   return addrVec;
}

unsigned hd::Leaf::getExtAddressCount() const
{
   return accountPtr_->getOuterAccount()->getHighestUsedIndex() + 1;
}

unsigned hd::Leaf::getUsedAddressCount() const
{
   return getExtAddressCount();
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
         bs::Address bsAddr(innerPair.second, innerPair.first);
         addrVec.emplace_back(bsAddr);
      }
   }

   return addrVec;
}

unsigned hd::Leaf::getIntAddressCount() const
{
   auto& accID = accountPtr_->getInnerAccountID();
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
   if (db_ != nullptr)
   {
      db_->close();
      delete db_;
      db_ = nullptr;
   }

   walletPtr_ = nullptr;
   accountPtr_ = nullptr;
}

WalletEncryptionLock hd::Leaf::lockForEncryption(const SecureBinaryData& passphrase)
{
   return WalletEncryptionLock(walletPtr_, passphrase);
}

bs::Address hd::Leaf::synchronizeUsedAddressChain(
   const std::string& index, AddressEntryType aeType)
{
   //decode index to path
   auto&& path = bs::hd::Path::fromString(index);

   //does path belong to our leaf?
   if (path.isAbolute())
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

   bs::Address result;
   int gap; //do not change to unsigned, gap needs to be signed
   if (topIndex != UINT32_MAX && addrIndex <= topIndex)
      gap = -1;
   else
      gap = addrIndex - topIndex; //this is correct wrt to result sign

   if (gap <= 0)
   {
      //already created this address, grab it, check the type matches
      result = getAddressByIndex(addrIndex, ext, aeType);
   }
   else
   {
      std::shared_ptr<AddressEntry> addrPtr;
      if (ext)
      {
         //pull new addresses to fill the gap, using the default type
         for (int i = 1; i < gap; i++)
            getNewExtAddress();

         //pull the new address using the requested type
         result = getNewExtAddress(aeType);
      }
      else
      {
         for (int i = 1; i < gap; i++)
            getNewIntAddress();
         
         result = getNewIntAddress(aeType);
      }
   }

   //sanity check: index and type should match request
   if (aeType != AddressEntryType_Default && result.getType() != aeType)
      throw AccountException("did not get expected address entry type");

   auto resultIndex = addressIndex(result);
   if(resultIndex != addrIndex)
      throw AccountException("did not get expected address index");

   return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

hd::AuthLeaf::AuthLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger)
   : Leaf(netType, logger, wallet::Type::Authentication)
{}

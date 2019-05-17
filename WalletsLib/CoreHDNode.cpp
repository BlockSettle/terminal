#include <memory>
#include <botan/serpent.h>
#include "AssetEncryption.h"
#include "EncryptionUtils.h"
#include "CoreHDNode.h"
#include "make_unique.h"

using namespace bs::core;

hd::Node::Node(NetworkType netType)
{
   setNetworkType(netType);
   generateRandomSeed();
   initFromSeed();
}

hd::Node::Node(const wallet::Seed &seed)
{
   setNetworkType(seed.networkType());
   initFrom(seed);
}

hd::Node::Node(const std::string &privKey)
{
   const auto &prefix = privKey.substr(0, 4);
   if (prefix == "tprv") {
      setNetworkType(NetworkType::TestNet);
   }
   else if (prefix == "xprv") {
      setNetworkType(NetworkType::MainNet);
   }
   else {
      throw std::invalid_argument("invalid private key prefix: " + prefix);
   }
   initFromPrivateKey(privKey);
}

hd::Node::Node(const hd::Node &src)
{
   seed_ = src.seed_;
   iv_ = src.iv_.copy();
   memcpy(&node_, &src.node_, sizeof(node_));
   hasPrivKey_ = src.hasPrivKey_;
   encTypes_ = src.encTypes_;
   encKeys_ = src.encKeys_;
   chainParams_ = src.chainParams_;
   netType_ = src.netType_;
}

hd::Node::Node(const btc_hdnode &node, NetworkType netType)
   : node_(node)
{
   setNetworkType(netType);
}

hd::Node::Node(const BinaryData &pubKey, const BinaryData &chainCode, NetworkType netType)
{
   if (pubKey.getSize() != sizeof(node_.public_key)) {
      throw std::invalid_argument("invalid pubkey size");
   }
   if (chainCode.getSize() != sizeof(node_.chain_code)) {
      throw std::invalid_argument("invalid chaincode size");
   }
   hasPrivKey_ = false;
   memset(&node_, 0, sizeof(node_));
   memcpy(node_.public_key, pubKey.getPtr(), pubKey.getSize());
   memcpy(node_.chain_code, chainCode.getPtr(), chainCode.getSize());
   setNetworkType(netType);
}

void hd::Node::setNetworkType(NetworkType netType)
{
   netType_ = netType;
   if (netType == NetworkType::MainNet) {
      chainParams_ = &btc_chainparams_main;
   }
   else {
      chainParams_ = &btc_chainparams_test;
   }
}

void hd::Node::generateRandomSeed()
{
   uint8_t seed[32];
   if (!btc_random_bytes(seed, sizeof(seed), 0)) {
      throw std::runtime_error("failed to get random seed");
   }
   seed_ = BinaryData(seed, sizeof(seed));
}

void hd::Node::initFromSeed()
{
   if (seed_.isNull()) {
      throw std::invalid_argument("seed is empty");
   }
   if (!btc_hdnode_from_seed(seed_.getPtr(), (int)seed_.getSize(), &node_)) {
      throw std::runtime_error("creation of bip32 node failed");
   }
   memset(node_.chain_code, 0, sizeof(node_.chain_code));
   btc_hdnode_fill_public_key(&node_);
}

void hd::Node::initFrom(const wallet::Seed &seed)
{
   if (seed.hasPrivateKey()) {
      if (seed.privateKey().isNull()) {
         throw std::invalid_argument("private key is empty");
      }
      if (seed.privateKey().getSize() != sizeof(node_.private_key)) {
         throw std::invalid_argument("invalid private key size");
      }
      memcpy(node_.private_key, seed.privateKey().getPtr(), seed.privateKey().getSize());

      if (!seed.chainCode().isNull()) {
         if (seed.chainCode().getSize() != sizeof(node_.chain_code)) {
            throw std::invalid_argument("invalid chain code size");
         }
         memcpy(node_.chain_code, seed.chainCode().getPtr(), seed.chainCode().getSize());
      }

      btc_hdnode_fill_public_key(&node_);
   }
   else {
      if (!seed.seed().isNull()) {
         seed_ = seed.seed();
      }
      else {
         generateRandomSeed();
      }
      initFromSeed();
   }
}

void hd::Node::initFromPrivateKey(const std::string &privKey)
{
   if (privKey.empty()) {
      throw std::invalid_argument("private key is empty");
   }
   if (chainParams_ == nullptr) {
      throw std::invalid_argument("chainparams must be set before calling");
   }
   if (!btc_hdnode_deserialize(privKey.c_str(), chainParams_, &node_)) {
      throw std::runtime_error("privkey deser of bip32 node failed");
   }
   btc_hdnode_fill_public_key(&node_);
}

void hd::Node::clearPrivKey()
{
   hasPrivKey_ = false;
   memset(node_.private_key, 0, sizeof(node_.private_key));
   seed_.clear();
}

std::string hd::Node::getPrivateKey() const
{
   if (!hasPrivKey_) {
      return {};
   }
   char sBuf[112];
   btc_hdnode_serialize_private(&node_, chainParams_, sBuf, sizeof(sBuf));
   return std::string(sBuf, sizeof(sBuf) - 1);
}

SecureBinaryData hd::Node::privateKey() const
{
   if (!hasPrivKey_) {
      return {};
   }
   return SecureBinaryData(node_.private_key, sizeof(node_.private_key));
}

wallet::Seed hd::Node::seed() const
{
   wallet::Seed seed(netType_);
   if (hasPrivKey_) {
      seed.setPrivateKey(privateKey());
   }
   else {
      seed.setSeed(seed_);
   }
   return seed;
}

std::string hd::Node::getId() const
{
   return wallet::computeID(pubCompressedKey()).toBinStr();
}

BinaryData hd::Node::pubCompressedKey() const
{
   return BinaryData(node_.public_key, sizeof(node_.public_key));
}

BinaryData hd::Node::chainCode() const
{
   static const uint8_t null_chain_code[BTC_BIP32_CHAINCODE_SIZE] = {};
   if (memcmp(null_chain_code, node_.chain_code, BTC_BIP32_CHAINCODE_SIZE) == 0) {
      return {};
   }
   return BinaryData(node_.chain_code, sizeof(node_.chain_code));
}

std::shared_ptr<AssetEntry_Single> hd::Node::getAsset(int id) const
{
   SecureBinaryData privKeyBin = privChainedKey().copy();
   const auto privKey = std::make_shared<Asset_PrivateKey>(BinaryData::IntToStrLE(id)
      , privKeyBin, make_unique<Cipher_AES>(BinaryData{}, BinaryData{}));
   SecureBinaryData pubKey = pubChainedKey();
   const AssetEntry_Single aes(id, BinaryData{}, pubKey, privKey);
   return std::make_shared<AssetEntry_Single>(aes);
}

std::shared_ptr<hd::Node> hd::Node::create(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_shared<hd::Node>(node, netType);
}

std::unique_ptr<hd::Node> hd::Node::createUnique(const btc_hdnode &node, NetworkType netType) const
{
   return make_unique<hd::Node>(node, netType);
}

std::shared_ptr<hd::Node> hd::Node::derive(const bs::hd::Path &path, bool pubCKD) const
{
   if ((!pubCKD && !hasPrivKey_) || !encTypes_.empty()) {
      return nullptr;
   }
   btc_hdnode newNode;
   if (!btc_hd_generate_key(&newNode, path.toString().c_str(), pubCKD ? node_.public_key : node_.private_key, node_.chain_code, pubCKD)) {
      return nullptr;
   }
   if (!pubCKD) {
      btc_hdnode_fill_public_key(&newNode);
   }
   BinaryData pubKey(newNode.public_key, sizeof(newNode.public_key));

   return create(newNode, netType_);
}

#define  ENCTYPE_BYTE   0x90
#define  ENCKEY_BYTE    0x91

BinaryData hd::Node::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(bs::hd::purpose);     // node type
   bw.put_uint8_t((uint8_t)netType_);

   if (hasPrivKey_) {
      bw.put_var_int(encTypes_.size() + 1);
      bw.put_uint8_t(ENCTYPE_BYTE);
      for (const auto &encType : encTypes_) {
         uint8_t arr[1] = { static_cast<uint8_t>(encType) };
         bw.put_BinaryData(BinaryData(arr, 1));
      }

      BinaryData privKey(getPrivateKey());
      bw.put_var_int(privKey.getSize() + 1);
      bw.put_uint8_t(PRIVKEY_BYTE);
      bw.put_BinaryData(privKey);

      if (!seed_.isNull()) {
         bw.put_var_int(seed_.getSize() + 1);
         bw.put_uint8_t(CIPHER_BYTE);
         bw.put_BinaryData(seed_);
      }

      if (!encTypes_.empty()) {
         bw.put_var_int(iv_.getSize() + 1);
         bw.put_uint8_t(ENCRYPTIONKEY_BYTE);
         bw.put_BinaryData(iv_);

         for (const auto &encKey : encKeys_) {
            bw.put_var_int(encKey.getSize() + 1);
            bw.put_uint8_t(ENCKEY_BYTE);
            bw.put_BinaryData(encKey);
         }
      }
   }
   else {
      BinaryData pubKey(pubCompressedKey());
      bw.put_var_int(pubKey.getSize() + 1);
      bw.put_uint8_t(PUBKEY_COMPRESSED_BYTE);
      bw.put_BinaryData(pubKey);

      BinaryData chainCode(node_.chain_code, sizeof(node_.chain_code));
      bw.put_var_int(chainCode.getSize() + 1);
      bw.put_uint8_t(PUBKEY_UNCOMPRESSED_BYTE);
      bw.put_BinaryData(chainCode);
   }

   return bw.getData();
}

std::shared_ptr<hd::Node> hd::Node::deserialize(BinaryDataRef value)
{
   BinaryRefReader brrVal(value);
   auto nodeType = brrVal.get_uint8_t();
   if (nodeType != bs::hd::purpose) {
      throw std::runtime_error("BIP44-incompatible purpose: " + std::to_string(nodeType));
   }
   const auto netType = static_cast<NetworkType>(brrVal.get_uint8_t());

   std::unordered_map<uint8_t, std::vector<BinaryRefReader>> values;
   while (brrVal.getSizeRemaining() > 0) {
      const auto len = brrVal.get_var_int();
      const auto valRef = brrVal.get_BinaryDataRef(len);
      BinaryRefReader brrData(valRef);
      const auto key = brrData.get_uint8_t();
      values[key].emplace_back(brrData);
   }

   std::shared_ptr<hd::Node> result;
   if (values.find(PRIVKEY_BYTE) != values.end()) {
      auto brrData = values[PRIVKEY_BYTE][0];
      const auto serPrivKey = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining())).toBinStr();
      result = std::make_shared<hd::Node>(serPrivKey);
      if (result->netType_ != netType) {
         throw std::runtime_error("network type mismatch");
      }

      if (values.find(CIPHER_BYTE) != values.end()) {
         brrData = values[CIPHER_BYTE][0];
         result->seed_ = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining()));
      }

      if (values.find(ENCTYPE_BYTE) != values.end()) {
         brrData = values[ENCTYPE_BYTE][0];
         const auto encData = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining()));
         for (size_t i = 0; i < encData.getSize(); ++i) {
            result->encTypes_.emplace_back(static_cast<bs::wallet::EncryptionType>(encData.getPtr()[i]));
         }
      }

      if (values.find(ENCKEY_BYTE) != values.end()) {
         for (auto brrData : values[ENCKEY_BYTE]) {
            result->encKeys_.emplace_back(BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining())));
         }
      }

      if (values.find(ENCRYPTIONKEY_BYTE) != values.end()) {
         if (result->encTypes().empty()) {
            result->encTypes_.push_back(bs::wallet::EncryptionType::Password);
         }
         brrData = values[ENCRYPTIONKEY_BYTE][0];
         result->iv_ = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining()));
      }
   }
   else if (values.find(PUBKEY_COMPRESSED_BYTE) != values.end()) {
      auto brrData = values[PUBKEY_COMPRESSED_BYTE][0];
      const auto pubKey = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining()));
      btc_hdnode node;
      if (pubKey.getSize() != sizeof(node.public_key)) {
         throw std::runtime_error("pubkey size mismatch: " + std::to_string(pubKey.getSize()));
      }
      memset(&node, 0, sizeof(node));
      memcpy(node.public_key, pubKey.getPtr(), pubKey.getSize());

      if (values.find(PUBKEY_UNCOMPRESSED_BYTE) != values.end()) {
         auto brrData = values[PUBKEY_UNCOMPRESSED_BYTE][0];
         const auto chainCode = BinaryData(brrData.get_BinaryDataRef((uint32_t)brrData.getSizeRemaining()));
         if (!chainCode.isNull() && chainCode.getSize() != sizeof(node.chain_code)) {
            throw std::runtime_error("chaincode size mismatch: " + std::to_string(chainCode.getSize()));
         }
         if (!chainCode.isNull()) {
            memcpy(node.chain_code, chainCode.getPtr(), chainCode.getSize());
         }
         result = std::make_shared<hd::Node>(node, netType);
         result->clearPrivKey();
      }
   }

   if (!result) {
      throw std::runtime_error("no keys found");
   }

   return result;
}

static SecureBinaryData PadData(const SecureBinaryData &key
   , size_t pad = AES_BLOCK_SIZE)
{
   const auto keyRem = key.getSize() % pad;
   auto result = key;
   if (keyRem) {
      result.resize(key.getSize() - keyRem + pad);
   }
   return result;
}

static SecureBinaryData LimitData(const SecureBinaryData &key
   , size_t limit = AES_MAX_KEY_LEN)
{
   if (key.getSize() <= limit) {
      return key;
   }
   return key.getSliceCopy(0, (uint32_t)limit);
}

std::unique_ptr<hd::Node> hd::Node::decrypt(const SecureBinaryData &password)
{
   if (encTypes_.empty()) {
      return nullptr;
   }
   auto result = createUnique(node_, netType_);
   const auto key = getKDF()->deriveKey(PadData(LimitData(password)));
   const SecureBinaryData privKey(node_.private_key, sizeof(node_.private_key));

   Botan::Serpent decrypter;
   decrypter.set_key(key.getDataVector());
   std::vector<uint8_t> decrypted(privKey.getSize());
   decrypter.decrypt(privKey.getDataVector(), decrypted);

   if (decrypted.size() != privKey.getSize()) {
      throw std::runtime_error("encrypted key size mismatch");
   }
   memcpy(result->node_.private_key, decrypted.data()
      , std::min<size_t>(BTC_ECKEY_PKEY_LENGTH, decrypted.size()));

   if (!seed_.isNull()) {
      const SecureBinaryData seed = seed_;
      std::vector<uint8_t> decSeed(seed.getSize());
      decrypter.decrypt(seed.getDataVector(), decSeed);
      result->seed_ = { decSeed.data(), decSeed.size() };
   }
   return result;
}

std::shared_ptr<KeyDerivationFunction> hd::Node::getKDF()
{
   if (!iv_.isNull() && !kdf_) {
      try {
         kdf_ = KeyDerivationFunction::deserialize(iv_);
         return kdf_;
      }
      catch (const std::exception &) {
         iv_.clear();
      }
   }
   if (!kdf_) {
      kdf_ = std::make_shared<KeyDerivationFunction_Romix>();
   }
   if (iv_.isNull()) {
      iv_ = kdf_->serialize();
   }
   return kdf_;
}

std::shared_ptr<hd::Node> hd::Node::encrypt(const SecureBinaryData &password
   , const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys)
{
   if (!encTypes_.empty() || password.isNull()) {
      return nullptr;
   }
   auto result = std::make_shared<hd::Node>(node_, netType_);
   result->encTypes_ = encTypes.empty()
      ? std::vector<bs::wallet::EncryptionType>{ bs::wallet::EncryptionType::Password }
      : encTypes;
   result->encKeys_ = encKeys;
   result->seed_.clear();
   memset(result->node_.private_key, 0, sizeof(result->node_.private_key));
   const SecureBinaryData privKey = privateKey();
   const auto key = getKDF()->deriveKey(PadData(LimitData(password)));
   result->iv_ = iv_;

   Botan::Serpent encrypter;
   encrypter.set_key(key.getDataVector());

   std::vector<uint8_t> encrypted(privKey.getSize());
   encrypter.encrypt(privKey.getDataVector(), encrypted);

   if (encrypted.size() != privKey.getSize()) {
      throw std::runtime_error("encrypted key size mismatch");
   }
   memcpy(result->node_.private_key, encrypted.data()
      , std::min<size_t>(BTC_ECKEY_PKEY_LENGTH, encrypted.size()));

   if (!seed_.isNull()) {
      const SecureBinaryData seed = PadData(seed_);
      std::vector<uint8_t> encSeed(seed.getSize());
      encrypter.encrypt(seed.getDataVector(), encSeed);
      result->seed_ = { encSeed.data(), encSeed.size() };
   }
   return result;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Node> hd::ChainedNode::create(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_shared<hd::ChainedNode>(node, netType, chainCode_);
}

std::unique_ptr<hd::Node> hd::ChainedNode::createUnique(const btc_hdnode &node, NetworkType netType) const
{
   return make_unique<hd::ChainedNode>(node, netType, chainCode_);
}

SecureBinaryData hd::ChainedNode::privChainedKey() const
{
   if (privateKey().isNull()) {
      return {};
   }
   CryptoECDSA crypto;
   return crypto.ComputeChainedPrivateKey(privateKey(), chainCode_);
}

BinaryData hd::ChainedNode::pubChainedKey() const
{
   CryptoECDSA crypto;
   return crypto.CompressPoint(crypto.ComputeChainedPublicKey(crypto.UncompressPoint(pubCompressedKey()), chainCode_));
}


std::shared_ptr<hd::Node> hd::Nodes::decrypt(const SecureBinaryData &password) const
{
   if (nodes_.empty()) {
      return nullptr;
   }
   if ((nodes_.size() == 1) && nodes_[0]->encTypes().empty()) {
      return nodes_[0];
   }

   for (const auto &node : nodes_) {
      auto encTypes = node->encTypes();

      std::shared_ptr<hd::Node> decryptedNode = nullptr;

      if (encTypes.empty() || encTypes[0] == bs::wallet::EncryptionType::Unencrypted) {
         decryptedNode = node;
      }
      else {
         if (password.isNull()) {
            continue;
         }

         try {
            decryptedNode = node->decrypt(password);
         }
         catch (const std::exception &) {
            continue;
         }
      }

      if (decryptedNode != nullptr) {
         wallet::Seed seed(decryptedNode->getNetworkType(), decryptedNode->privateKey());
         if (hd::Node(seed).getId() == id_) {
            return decryptedNode;
         }
      }
   }
   return nullptr;
}

std::vector<bs::wallet::EncryptionType> hd::Nodes::encryptionTypes() const
{
   if (nodes_.empty()) {
      return {};
   }
   std::set<bs::wallet::EncryptionType> encTypes;
   for (const auto &node : nodes_) {
      const auto &nodeEncTypes = node->encTypes();
      encTypes.insert(nodeEncTypes.begin(), nodeEncTypes.end());
   }
   std::vector<bs::wallet::EncryptionType> result;
   for (const auto &encType : encTypes) {
      result.emplace_back(encType);
   }
   return result;
}

std::vector<SecureBinaryData> hd::Nodes::encryptionKeys() const
{
   if (nodes_.empty()) {
      return {};
   }
   std::set<SecureBinaryData> encKeys;
   for (const auto &node : nodes_) {
      const auto &nodeEncKeys = node->encKeys();
      encKeys.insert(nodeEncKeys.begin(), nodeEncKeys.end());
   }
   std::vector<SecureBinaryData> result;
   for (const auto &encKey : encKeys) {
      result.emplace_back(encKey);
   }
   return result;
}

hd::Nodes hd::Nodes::chained(const BinaryData &chainKey) const
{
   std::vector<std::shared_ptr<hd::Node>> chainedNodes;
   for (const auto &node : nodes_) {
      chainedNodes.emplace_back(std::make_shared<hd::ChainedNode>(*node, chainKey));
   }
   return hd::Nodes(chainedNodes, rank_, id_);
}

void hd::Nodes::forEach(const std::function<void(std::shared_ptr<Node>)> &cb) const
{
   if (!cb) {
      return;
   }
   for (const auto &node : nodes_) {
      cb(node);
   }
}

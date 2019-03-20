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

std::string hd::Node::getPrivateKey() const
{
   if (hasPrivateKey()) {
      return {};
   }
   char sBuf[112];
   btc_hdnode_serialize_private(&node_, chainParams_, sBuf, sizeof(sBuf));
   return std::string(sBuf, sizeof(sBuf) - 1);
}

SecureBinaryData hd::Node::privateKey() const
{
   if (hasPrivateKey()) {
      return {};
   }
   return SecureBinaryData(node_.private_key, sizeof(node_.private_key));
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

std::shared_ptr<hd::Node> hd::Node::create(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_shared<hd::Node>(node, netType);
}

std::unique_ptr<hd::Node> hd::Node::createUnique(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_unique<hd::Node>(node, netType);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<hd::Node> hd::ChainedNode::create(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_shared<hd::ChainedNode>(node, netType, chainCode_);
}

std::unique_ptr<hd::Node> hd::ChainedNode::createUnique(const btc_hdnode &node, NetworkType netType) const
{
   return std::make_unique<hd::ChainedNode>(node, netType, chainCode_);
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

hd::Nodes hd::Nodes::chained(const BinaryData &chainKey) const
{
   std::vector<std::shared_ptr<hd::Node>> chainedNodes;
   for (const auto &node : nodes_) {
      chainedNodes.emplace_back(std::make_shared<hd::ChainedNode>(*node, chainKey));
   }
   return hd::Nodes(chainedNodes, id_);
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

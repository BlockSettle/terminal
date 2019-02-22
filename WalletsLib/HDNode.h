#ifndef __BS_HD_NODE_H__
#define __BS_HD_NODE_H__

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <btc/bip32.h>
#include <btc/chainparams.h>
#include "BinaryData.h"
#include "BtcDefinitions.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "HDPath.h"
#include "MetaData.h"
#include "WalletEncryption.h"

struct KeyDerivationFunction;

namespace bs {
   namespace hd {

      class Node
      {
      public:
         Node(NetworkType netType);
         Node(const bs::core::wallet::Seed &);
         Node(const std::string &privKey);
         Node(const btc_hdnode &, NetworkType);
         Node(const Node &node);
         Node(const BinaryData &pubKey, const BinaryData &chainCode, NetworkType);
         Node& operator = (const Node&) = delete;
         Node(Node&&) = delete;
         Node& operator = (Node&&) = delete;

         std::string getPrivateKey() const;
         SecureBinaryData privateKey() const;
         virtual SecureBinaryData privChainedKey() const { return privateKey(); }
         BinaryData pubCompressedKey() const;
         virtual BinaryData pubChainedKey() const { return pubCompressedKey(); }
         BinaryData chainCode() const;
         std::shared_ptr<AssetEntry_Single> getAsset(int id) const;
         bs::core::wallet::Seed seed() const;
         std::string getId() const;

         const BinaryData &getSeed() const { return seed_; }
         NetworkType getNetworkType() const { return netType_; }

         BinaryData serialize() const;
         static std::shared_ptr<Node> deserialize(BinaryDataRef);

         std::shared_ptr<Node> derive(const Path &path, bool pubCKD = false) const;

         void clearPrivKey();
         bool hasPrivateKey() const { return hasPrivKey_; }

         std::vector<wallet::EncryptionType> encTypes() const { return encTypes_; }
         std::vector<SecureBinaryData> encKeys() const { return encKeys_; }

         std::unique_ptr<hd::Node> decrypt(const SecureBinaryData &password);
         std::shared_ptr<hd::Node> encrypt(const SecureBinaryData &password
            , const std::vector<wallet::EncryptionType> &encTypes = {}
            , const std::vector<SecureBinaryData> &encKeys = {});

      protected:
         virtual std::shared_ptr<Node> create(const btc_hdnode &, NetworkType) const;
         virtual std::unique_ptr<Node> createUnique(const btc_hdnode &, NetworkType) const;

      protected:
         BinaryData        seed_;
         SecureBinaryData  iv_;
         btc_hdnode        node_ = {};
         bool              hasPrivKey_ = true;
         std::vector<SecureBinaryData> encKeys_;
         std::vector<wallet::EncryptionType> encTypes_;
         const btc_chainparams * chainParams_ = nullptr;
         std::shared_ptr<KeyDerivationFunction> kdf_;
         NetworkType       netType_;

      private:
         void setNetworkType(NetworkType netType);
         void generateRandomSeed();
         void initFromSeed();
         void initFromPrivateKey(const std::string &privKey);
         void initFrom(const bs::core::wallet::Seed &);
         std::shared_ptr<KeyDerivationFunction> getKDF();
      };


      class ChainedNode : public Node
      {
      public:
         ChainedNode(const Node &node, const BinaryData &chainCode) : Node(node), chainCode_(chainCode) {}
         ChainedNode(const btc_hdnode &node, NetworkType netType, const BinaryData &chainCode)
            : Node(node, netType), chainCode_(chainCode) {}

         SecureBinaryData privChainedKey() const override;
         BinaryData pubChainedKey() const override;

      protected:
         std::shared_ptr<Node> create(const btc_hdnode &, NetworkType) const override;
         std::unique_ptr<Node> createUnique(const btc_hdnode &, NetworkType) const override;

      private:
         BinaryData  chainCode_;
      };


      class Nodes
      {
      public:
         Nodes() {}
         Nodes(const std::vector<std::shared_ptr<Node>> &nodes, wallet::KeyRank rank, const std::string &id)
            : nodes_(nodes), rank_(rank), id_(id) {}

         bool empty() const { return nodes_.empty(); }
         std::vector<wallet::EncryptionType> encryptionTypes() const;
         std::vector<SecureBinaryData> encryptionKeys() const;
         wallet::KeyRank rank() const { return rank_; }

         std::shared_ptr<hd::Node> decrypt(const SecureBinaryData &) const;
         Nodes chained(const BinaryData &chainKey) const;

         void forEach(const std::function<void(std::shared_ptr<Node>)> &) const;

      private:
         std::vector<std::shared_ptr<Node>>  nodes_;
         wallet::KeyRank   rank_ = { 0, 0 };
         std::string       id_;
      };

   }  //namespace hd
}  //namespace bs

#endif //__BS_HD_NODE_H__

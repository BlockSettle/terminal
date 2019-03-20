#ifndef BS_CORE_HD_NODE_H
#define BS_CORE_HD_NODE_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "disable_warnings.h"
#include <btc/bip32.h>
#include <btc/chainparams.h>
#include "enable_warnings.h"

#include "BinaryData.h"
#include "BtcDefinitions.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "HDPath.h"

struct KeyDerivationFunction;

namespace bs {
   namespace core {
      namespace hd {

         class Node
         {
         public:
            Node(NetworkType netType);
            Node(const wallet::Seed &);
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
            std::string getId() const;

            NetworkType getNetworkType() const { return netType_; }
            bool hasPrivateKey() const { return false; }

         protected:
            virtual std::shared_ptr<Node> create(const btc_hdnode &, NetworkType) const;
            virtual std::unique_ptr<Node> createUnique(const btc_hdnode &, NetworkType) const;

         protected:
            BinaryData        seed_;
            SecureBinaryData  iv_;
            btc_hdnode        node_ = {};
            std::vector<SecureBinaryData> encKeys_;
            const btc_chainparams * chainParams_ = nullptr;
            std::shared_ptr<KeyDerivationFunction> kdf_;
            NetworkType       netType_;

         private:
            void setNetworkType(NetworkType netType);
            void generateRandomSeed();
            void initFromSeed();
            void initFromPrivateKey(const std::string &privKey);
            void initFrom(const wallet::Seed &);
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
            Nodes(const std::vector<std::shared_ptr<Node>> &nodes, const std::string &id)
               : nodes_(nodes), id_(id) {}

            bool empty() const { return nodes_.empty(); }

            Nodes chained(const BinaryData &chainKey) const;

            void forEach(const std::function<void(std::shared_ptr<Node>)> &) const;

         private:
            std::vector<std::shared_ptr<Node>>  nodes_;
            std::string       id_;
         };


      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_NODE_H

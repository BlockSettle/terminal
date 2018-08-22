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
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "WalletEncryption.h"


namespace bs {
   namespace hd {

      class Path
      {
      public:
         using Elem = uint32_t;

         Path() {}
         Path(const std::vector<Elem> &elems);

         bool operator==(const Path &other) const {
            return (path_ == other.path_);
         }
         bool operator!=(const Path &other) const {
            return (path_ != other.path_);
         }

         void append(Elem elem, bool hardened = false);
         void append(const std::string &key, bool hardened = false);
         size_t length() const { return path_.size(); }
         Elem get(int index) const;   // negative index is an offset from end
         void clear();
         bool isAbolute() const { return isAbsolute_; }

         std::string toString(bool alwaysAbsolute = true) const;

         void setHardened(size_t index);
         bool isHardened(size_t index) const;

         static Path fromString(const std::string &);
         static Elem keyToElem(const std::string &key);

      private:
         std::vector<Elem> path_;
         std::set<size_t>  hardenedIdx_;
         bool isAbsolute_ = false;
      };


      static const Path::Elem purpose = 44;  // BIP44-compatible

      enum CoinType : Path::Elem {
         Bitcoin_main = 0,
         Bitcoin_test = 1,
         BlockSettle_CC = 0x4253,            // "BS" in hex
         BlockSettle_Auth = 0x41757468       // "Auth" in hex
      };


      class Node
      {
      public:
         Node(NetworkType netType);
         Node(const bs::wallet::Seed &);
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
         bs::wallet::Seed seed() const;
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
         NetworkType       netType_;

      private:
         void setNetworkType(NetworkType netType);
         void generateRandomSeed();
         void initFromSeed();
         void initFromPrivateKey(const std::string &privKey);
         void initFrom(const bs::wallet::Seed &);
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

bool operator < (const bs::hd::Path &l, const bs::hd::Path &r);

#endif //__BS_HD_NODE_H__

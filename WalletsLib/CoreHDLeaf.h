#ifndef BS_CORE_HD_LEAF_H
#define BS_CORE_HD_LEAF_H

#include <functional>
#include <unordered_map>
#include <lmdbpp.h>
#include "CoreHDNode.h"

namespace spdlog {
   class logger;
}

namespace bs {
   class TxAddressChecker;

   namespace core {
      namespace hd {
         class Group;

         class Leaf : public bs::core::Wallet
         {
            friend class hd::Group;

         public:
            Leaf(NetworkType, const std::string &name, const std::string &desc
               , const std::shared_ptr<spdlog::logger> &logger = nullptr
               , wallet::Type type = wallet::Type::Bitcoin, bool extOnlyAddresses = false);
            ~Leaf() override = default;

            virtual void init(const std::shared_ptr<Node> &node, const bs::hd::Path &, Nodes rootNodes);
            virtual bool copyTo(std::shared_ptr<hd::Leaf> &) const;

            std::string walletId() const override;
            std::string description() const override;
            void setDescription(const std::string &desc) override { desc_ = desc; }
            std::string shortName() const override { return suffix_; }
            wallet::Type type() const override { return type_; }
            bool isWatchingOnly() const override { return rootNodes_.empty(); }
            std::vector<bs::wallet::EncryptionType> encryptionTypes() const override { return rootNodes_.encryptionTypes(); }
            std::vector<SecureBinaryData> encryptionKeys() const override { return rootNodes_.encryptionKeys(); }
            std::pair<unsigned int, unsigned int> encryptionRank() const override { return rootNodes_.rank(); }
            bool hasExtOnlyAddresses() const override { return isExtOnly_; }

            bool containsAddress(const bs::Address &addr) override;
            bool containsHiddenAddress(const bs::Address &addr) const override;
            BinaryData getRootId() const override;
            BinaryData getPubKey() const { return node_ ? node_->pubCompressedKey() : BinaryData(); }
            BinaryData getChainCode() const { return node_ ? node_->chainCode() : BinaryData(); }

            std::vector<bs::Address> getPooledAddressList() const override;
            std::vector<bs::Address> getExtAddressList() const override { return extAddresses_; }
            std::vector<bs::Address> getIntAddressList() const override { return intAddresses_; }
            size_t getExtAddressCount() const override { return extAddresses_.size(); }
            size_t getIntAddressCount() const override { return intAddresses_.size(); }
            bool isExternalAddress(const Address &) const override;
            bs::Address getNewExtAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewIntAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getRandomChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
            std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
            std::string getAddressIndex(const bs::Address &) override;
            bool addressIndexExists(const std::string &index) const override;
            bs::Address createAddressWithIndex(const std::string &index, bool persistent = true
               , AddressEntryType = AddressEntryType_Default) override;

            std::shared_ptr<ResolverFeed> getResolver(const SecureBinaryData &password) override;
            std::shared_ptr<ResolverFeed> getPublicKeyResolver() override;

            SecureBinaryData getPublicKeyFor(const bs::Address &) override;
            SecureBinaryData getPubChainedKeyFor(const bs::Address &) override;
            KeyPair getKeyPairFor(const bs::Address &, const SecureBinaryData &password) override;

            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }
            BinaryData serialize() const;
            void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db);

            std::shared_ptr<LMDBEnv> getDBEnv() override { return dbEnv_; }
            LMDB *getDB() override { return db_; }

            void addAddress(const bs::Address &, const BinaryData &pubChainedKey, const bs::hd::Path &path);

         protected:
            virtual bs::Address createAddress(const bs::hd::Path &path, bs::hd::Path::Elem index
               , AddressEntryType aet, bool persistent = true);
            virtual BinaryData serializeNode() const { return node_ ? node_->serialize() : BinaryData{}; }
            virtual void setRootNodes(Nodes);
            void reset();

            bs::hd::Path getPathForAddress(const bs::Address &) const;
            std::shared_ptr<Node> getNodeForAddr(const bs::Address &) const;
            std::shared_ptr<hd::Node> getPrivNodeFor(const bs::Address &, const SecureBinaryData &password);
            bs::Address createAddressWithPath(const bs::hd::Path &, bool persistent, AddressEntryType);

            struct AddrPoolKey {
               bs::hd::Path      path;
               AddressEntryType  aet;

               bool operator==(const AddrPoolKey &other) const {
                  return ((path == other.path) && (aet == other.aet));
               }
            };
            using PooledAddress = std::pair<AddrPoolKey, bs::Address>;
            std::vector<PooledAddress> generateAddresses(bs::hd::Path::Elem prefix, bs::hd::Path::Elem start
               , size_t nb, AddressEntryType aet);

         protected:
            const bs::hd::Path::Elem  addrTypeExternal = 0u;
            const bs::hd::Path::Elem  addrTypeInternal = 1u;
            const AddressEntryType defaultAET_ = AddressEntryType_P2WPKH;

            mutable std::string     walletId_, walletIdInt_;
            wallet::Type            type_;
            std::shared_ptr<Node>   node_;
            Nodes                   rootNodes_;
            bs::hd::Path            path_;
            std::string name_, desc_;
            std::string suffix_;
            bool        isExtOnly_ = false;

            bs::hd::Path::Elem  lastIntIdx_ = 0;
            bs::hd::Path::Elem  lastExtIdx_ = 0;

            size_t intAddressPoolSize_ = 100;
            size_t extAddressPoolSize_ = 100;
            const std::vector<AddressEntryType> poolAET_ = { AddressEntryType_P2SH, AddressEntryType_P2WPKH };

            std::map<BinaryData, BinaryData> hashToPubKey_;
            std::map<BinaryData, bs::hd::Path>   pubKeyToPath_;
            using TempAddress = std::pair<bs::hd::Path, AddressEntryType>;
            std::unordered_map<bs::hd::Path::Elem, TempAddress>  tempAddresses_;

            struct AddrPoolHasher {
               std::size_t operator()(const AddrPoolKey &key) const {
                  return (std::hash<std::string>()(key.path.toString()) ^ std::hash<int>()((int)key.aet));
               }
            };
            std::unordered_map<AddrPoolKey, bs::Address, AddrPoolHasher>   addressPool_;
            std::map<bs::Address, AddrPoolKey>           poolByAddr_;

         private:
            std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
            LMDB* db_ = nullptr;
            using AddressTuple = std::tuple<bs::Address, std::shared_ptr<Node>, bs::hd::Path>;
            std::unordered_map<bs::hd::Path::Elem, AddressTuple> addressMap_;
            std::vector<bs::Address>                     intAddresses_;
            std::vector<bs::Address>                     extAddresses_;
            std::map<BinaryData, bs::hd::Path::Elem>     addrToIndex_;

         private:
            bs::Address newAddress(const bs::hd::Path &path, AddressEntryType aet);
            bs::Address createAddress(AddressEntryType aet, bool isInternal = false);
            std::shared_ptr<AddressEntry> getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
               , AddressEntryType ae_type = AddressEntryType_Default);
            bs::hd::Path::Elem getAddressIndexForAddr(const BinaryData &addr) const;
            bs::hd::Path::Elem addressIndex(const bs::Address &addr) const;
            void topUpAddressPool(size_t intAddresses = 0
               , size_t extAddresses = 0);
            bs::hd::Path::Elem getLastAddrPoolIndex(bs::hd::Path::Elem) const;

            static void serializeAddr(BinaryWriter &bw, bs::hd::Path::Elem index
               , AddressEntryType, const bs::hd::Path &);
            bool deserialize(const BinaryData &ser, Nodes rootNodes);
         };


         class AuthLeaf : public Leaf
         {
         public:
            AuthLeaf(NetworkType, const std::string &name, const std::string &desc
               , const std::shared_ptr<spdlog::logger> &logger = nullptr);

            void init(const std::shared_ptr<Node> &node, const bs::hd::Path &, Nodes rootNodes) override;
            void setChainCode(const BinaryData &) override;

         protected:
            bs::Address createAddress(const bs::hd::Path &path, bs::hd::Path::Elem index
               , AddressEntryType aet, bool persistent = true) override;
            BinaryData serializeNode() const override {
               return unchainedNode_ ? unchainedNode_->serialize() : BinaryData{};
            }
            void setRootNodes(Nodes) override;

         private:
            std::shared_ptr<Node>   unchainedNode_;
            Nodes                   unchainedRootNodes_;
            BinaryData              chainCode_;
         };


         class CCLeaf : public Leaf
         {
         public:
            CCLeaf(NetworkType netType, const std::string &name, const std::string &desc,
               const std::shared_ptr<spdlog::logger> &logger = nullptr,
               bool extOnlyAddresses = false)
               : hd::Leaf(netType, name, desc, logger, wallet::Type::ColorCoin, extOnlyAddresses) {}
            ~CCLeaf() override = default;

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_LEAF_H

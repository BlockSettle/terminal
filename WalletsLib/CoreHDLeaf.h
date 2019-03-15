#ifndef BS_CORE_HD_LEAF_H
#define BS_CORE_HD_LEAF_H

#include <functional>
#include <unordered_map>
#include <lmdbpp.h>
#include "CoreHDNode.h"
#include "Accounts.h"

#define BS_WALLET_DBNAME "bs_wallet_name"

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
            Leaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger, 
               wallet::Type type = wallet::Type::Bitcoin);
            ~Leaf();

            virtual void init(
               std::shared_ptr<AssetWallet_Single>, 
               const BinaryData& addrAccId,
               const bs::hd::Path &);
            virtual bool copyTo(std::shared_ptr<hd::Leaf> &) const;

            std::string walletId() const override;
            std::string shortName() const override { return suffix_; }
            wallet::Type type() const override { return type_; }
            bool isWatchingOnly() const;
            bool hasExtOnlyAddresses() const override;
            NetworkType networkType(void) const { return netType_; }

            bool containsAddress(const bs::Address &addr) override;
            bool containsHiddenAddress(const bs::Address &addr) const override;
            BinaryData getRootId() const override;

            std::vector<bs::Address> getPooledAddressList() const override;
            std::vector<bs::Address> getExtAddressList() const override;
            std::vector<bs::Address> getIntAddressList() const override;
            size_t getExtAddressCount() const override;
            size_t getIntAddressCount() const override;
            bool isExternalAddress(const Address &) const override;
            bs::Address getNewExtAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewIntAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
            std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
            std::string getAddressIndex(const bs::Address &) override;
            bool addressIndexExists(const std::string &index) const override;

            SecureBinaryData getPublicKeyFor(const bs::Address &) override;
            SecureBinaryData getPubChainedKeyFor(const bs::Address &) override;
            KeyPair getKeyPairFor(const bs::Address &);
            std::shared_ptr<ResolverFeed> getResolver(void) const;

            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }
            BinaryData serialize() const;

            static std::pair<BinaryData, bs::hd::Path> deserialize(const BinaryData &ser);

         protected:
            void reset();

            bs::hd::Path getPathForAddress(const bs::Address &) const;
            std::shared_ptr<Node> getNodeForAddr(const bs::Address &) const;

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

            std::shared_ptr<LMDBEnv> getDBEnv() { return accountPtr_->getDbEnv(); }
            LMDB* getDB() { return db_; }

         protected:
            const bs::hd::Path::Elem  addrTypeExternal = 0u;
            const bs::hd::Path::Elem  addrTypeInternal = 1u;

            mutable std::string     walletId_, walletIdInt_;
            wallet::Type            type_;
            bs::hd::Path            path_;
            std::string suffix_;
            LMDB* db_ = nullptr;
            const NetworkType netType_;

         private:
            std::shared_ptr<::AddressAccount> accountPtr_;
            std::function<std::shared_ptr<ResolverFeed>(void)> getResolverLambda_;

         private:
            bs::Address newAddress(AddressEntryType aet);
            bs::Address newInternalAddress(AddressEntryType aet);

            std::shared_ptr<AddressEntry> getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
               , AddressEntryType ae_type = AddressEntryType_Default);
            bs::hd::Path::Elem getAddressIndexForAddr(const BinaryData &addr) const;
            bs::hd::Path::Elem addressIndex(const bs::Address &addr) const;
            void topUpAddressPool(size_t count = 0);
            bs::hd::Path::Elem getLastAddrPoolIndex() const;
         };


         class AuthLeaf : public Leaf
         {
         public:
            AuthLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger);

         private:
            std::shared_ptr<Node>   unchainedNode_;
            Nodes                   unchainedRootNodes_;
            BinaryData              chainCode_;
         };


         class CCLeaf : public Leaf
         {
         public:
            CCLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger)
               : hd::Leaf(netType, logger, wallet::Type::ColorCoin) {}
            ~CCLeaf() override = default;

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_LEAF_H

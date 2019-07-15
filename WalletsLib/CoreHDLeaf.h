#ifndef BS_CORE_HD_LEAF_H
#define BS_CORE_HD_LEAF_H

#include <functional>
#include <unordered_map>
#include <lmdbpp.h>
#include "Accounts.h"
#include "CoreWallet.h"
#include "HDPath.h"

#define LEAF_KEY              0x00002001
#define AUTH_LEAF_KEY         0x00002002
#define SETTLEMENT_LEAF_KEY   0x00002003

namespace spdlog {
   class logger;
}

namespace bs {
   class TxAddressChecker;

   namespace core {
      namespace hd {
         class AuthGroup;
         class Group;
         class Wallet;
         class SettlementLeaf;

         class Leaf : public bs::core::Wallet
         {
            friend class hd::Group;
            friend class hd::Wallet;
            friend class SettlementLeaf;

         public:
            Leaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger, 
               wallet::Type type = wallet::Type::Bitcoin);
            ~Leaf();

            virtual void init(
               std::shared_ptr<AssetWallet_Single>, 
               const BinaryData& addrAccId);
            virtual std::shared_ptr<hd::Leaf> getCopy(
               std::shared_ptr<AssetWallet_Single>) const;
            
            void setPath(const bs::hd::Path&);

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
            
            unsigned getExtAddressCount() const override;
            unsigned getUsedAddressCount() const override;
            unsigned getIntAddressCount() const override;

            bool isExternalAddress(const Address &) const override;
            bs::Address getNewExtAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewIntAddress(AddressEntryType aet = AddressEntryType_Default) override;
            bs::Address getNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
            std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
                        
            std::string getAddressIndex(const bs::Address &) override;
            bs::hd::Path::Elem getAddressIndexForAddr(const BinaryData &addr) const;
            bs::hd::Path::Elem addressIndex(const bs::Address &addr) const;
            bool addressIndexExists(const std::string &index) const override;
  
            std::pair<bs::Address, bool> synchronizeUsedAddressChain(
               const std::string&, AddressEntryType) override;

            //index as asset derivation id
            //bool as external (true) or interal (false)
            bs::Address getAddressByIndex(unsigned, bool, 
               AddressEntryType aet = AddressEntryType_Default) const;

            SecureBinaryData getPublicKeyFor(const bs::Address &) override;
            std::shared_ptr<ResolverFeed> getResolver(void) const;

            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }
            virtual BinaryData serialize() const;

            static std::pair<std::shared_ptr<hd::Leaf>, BinaryData> deserialize(
               const BinaryData &ser, NetworkType netType, std::shared_ptr<spdlog::logger> logger);

            void shutdown(void);
            std::string getFilename(void) const;
            WalletEncryptionLock lockForEncryption(const SecureBinaryData& passphrase);
            std::vector<bs::Address> extendAddressChain(unsigned count, bool extInt) override;

            std::map<BinaryData, std::pair<bs::hd::Path, AddressEntryType>> indexPathAndTypes(
               const std::set<BinaryData>&) override;

            virtual bs::hd::Path::Elem getExtPath(void) const { return addrTypeExternal_; }
            virtual bs::hd::Path::Elem getIntPath(void) const { return addrTypeInternal_; }

            std::shared_ptr<AssetEntry> getRootAsset(void) const;

         public:
            static const bs::hd::Path::Elem  addrTypeExternal_ = 0u;
            static const bs::hd::Path::Elem  addrTypeInternal_ = 1u;

         protected:
            void reset();

            bs::hd::Path getPathForAddress(const bs::Address &) const;

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
            void readMetaData();

         protected:
            mutable std::string     walletId_, walletIdInt_;
            wallet::Type            type_;
            bs::hd::Path            path_;
            std::string suffix_;
            LMDB* db_ = nullptr;
            const NetworkType netType_;

         private:
            std::shared_ptr<AssetWallet_Single> walletPtr_;
            std::shared_ptr<::AddressAccount> accountPtr_;

         private:
            bs::Address newAddress(AddressEntryType aet);
            bs::Address newInternalAddress(AddressEntryType aet);

            std::shared_ptr<AddressEntry> getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
               , AddressEntryType ae_type = AddressEntryType_Default);
            void topUpAddressPool(size_t count, bool intExt);
            bs::hd::Path::Elem getLastAddrPoolIndex() const;
         };


         class AuthLeaf : public Leaf
         {
            friend class hd::Leaf;
            friend class hd::AuthGroup;

         private:
            SecureBinaryData salt_;

         private:
            void setSalt(const SecureBinaryData& salt) { salt_= salt; }

         public:
            AuthLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger);
            
            std::shared_ptr<hd::Leaf> getCopy(
               std::shared_ptr<AssetWallet_Single>) const override;
            BinaryData serialize() const override;

            const SecureBinaryData& getSalt(void) const { return salt_; }
         };


         class CCLeaf : public Leaf
         {
         public:
            CCLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger)
               : hd::Leaf(netType, logger, wallet::Type::ColorCoin) {}
            ~CCLeaf() override = default;

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
         };


         class SettlementLeaf : public Leaf
         {
         public:
            SettlementLeaf(NetworkType netType, std::shared_ptr<spdlog::logger> logger)
               : hd::Leaf(netType, logger, wallet::Type::ColorCoin) {}
            ~SettlementLeaf() override = default;

            BinaryData serialize() const override;

            wallet::Type type() const override { return wallet::Type::Settlement; }
            unsigned addSettlementID(const SecureBinaryData&);

            BinaryData signTXRequest(const wallet::TXSignRequest &
               , bool keepDuplicatedRecipients = false) override
            {
               throw std::runtime_error("invalid for settlement leaves, \
                  use bs::core::hd::Wallet::signSettlementTXRequest");
            }

            unsigned getIndexForSettlementID(const SecureBinaryData&) const;
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_LEAF_H

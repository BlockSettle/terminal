#ifndef BS_CORE_HD_GROUP_H
#define BS_CORE_HD_GROUP_H

#include <unordered_map>
#include "CoreHDLeaf.h"
#include "Wallets.h"

#define BS_GROUP_PREFIX 0xE1

namespace spdlog {
   class logger;
}

namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
         class SettlementGroup;

         ///////////////////////////////////////////////////////////////////////
         class Group
         {
            friend class hd::Wallet;
            friend class hd::SettlementGroup;

         public:
            Group(std::shared_ptr<AssetWallet_Single>, bs::hd::Path::Elem, NetworkType netType
               , bool isExtOnly, const std::shared_ptr<spdlog::logger> &logger = nullptr);

            ~Group(void);

            size_t getNumLeaves() const { return leaves_.size(); }
            std::shared_ptr<hd::Leaf> getLeafByPath(const bs::hd::Path &) const;
            std::shared_ptr<hd::Leaf> getLeafById(const std::string &id) const;
            std::vector<std::shared_ptr<Leaf>> getLeaves() const;
            std::vector<std::shared_ptr<Leaf>> getAllLeaves() const;
            
            virtual std::shared_ptr<Leaf> createLeaf(const bs::hd::Path &
               , unsigned lookup = UINT32_MAX);
            virtual std::shared_ptr<Leaf> createLeaf(AddressEntryType
               , bs::hd::Path::Elem, unsigned lookup = UINT32_MAX);
            virtual std::shared_ptr<Leaf> createLeaf(AddressEntryType
               , const std::string &key, unsigned lookup = UINT32_MAX);
            
            virtual std::shared_ptr<Leaf> newLeaf(AddressEntryType) const;
            virtual bool addLeaf(const std::shared_ptr<Leaf> &);
            bool deleteLeaf(const std::shared_ptr<bs::core::hd::Leaf> &);
            bool deleteLeaf(const bs::hd::Path &);

            virtual wallet::Type type() const { return wallet::Type::Bitcoin; }
            bs::hd::Path::Elem index() const { return index_; }

            virtual void shutdown(void);
            virtual std::set<AddressEntryType> getAddressTypeSet(void) const;
            bool isExtOnly(void) const { return isExtOnly_; }

            virtual std::shared_ptr<hd::Group> getCopy(
               std::shared_ptr<AssetWallet_Single>) const;

         protected:
            bool needsCommit() const { return needsCommit_; }
            void committed() { needsCommit_ = false; }

            virtual void serializeLeaves(BinaryWriter &) const;
            virtual void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &, 
               unsigned lookup = UINT32_MAX) const;

         protected:
            bs::hd::Path::Elem   index_;
            std::shared_ptr<spdlog::logger>  logger_;
            bool        needsCommit_ = true;
            NetworkType netType_;
            bool isExtOnly_ = false;
            std::map<bs::hd::Path, std::shared_ptr<hd::Leaf>> leaves_;

            std::shared_ptr<AssetWallet_Single> walletPtr_;
            LMDB* db_ = nullptr;

         private:
            virtual BinaryData serialize() const;

            static std::shared_ptr<Group> deserialize(
               std::shared_ptr<AssetWallet_Single>, 
               BinaryDataRef key, BinaryDataRef val
               , const std::string &name
               , const std::string &desc
               , NetworkType netType
               , const std::shared_ptr<spdlog::logger> &logger);
            virtual void deserialize(BinaryDataRef value);
            void commit(bool force = false);
            void putDataToDB(const BinaryData&, const BinaryData&);
         };

         ///////////////////////////////////////////////////////////////////////
         class AuthGroup : public Group
         {
         public:
            AuthGroup(std::shared_ptr<AssetWallet_Single>,
               NetworkType netType,
               const std::shared_ptr<spdlog::logger> &);

            wallet::Type type() const override { return wallet::Type::Authentication; }

            void shutdown(void) override;
            std::set<AddressEntryType> getAddressTypeSet(void) const override;
            void setSalt(const SecureBinaryData&);
            const SecureBinaryData& getSalt(void) const { return salt_; }

            std::shared_ptr<hd::Group> getCopy(
               std::shared_ptr<AssetWallet_Single>) const override;

         protected:
            bool addLeaf(const std::shared_ptr<Leaf> &) override;
            std::shared_ptr<Leaf> newLeaf(AddressEntryType) const override;
            void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &,
               unsigned lookup = UINT32_MAX) const override;
            void serializeLeaves(BinaryWriter &) const override;

            SecureBinaryData salt_;

         private:
            BinaryData serialize() const override;
            void deserialize(BinaryDataRef value) override;
         };

         ///////////////////////////////////////////////////////////////////////
         class CCGroup : public Group
         {
         public:
            CCGroup(std::shared_ptr<AssetWallet_Single> walletPtr
               , NetworkType netType, const std::shared_ptr<spdlog::logger> &logger)
               : Group(walletPtr, bs::hd::CoinType::BlockSettle_CC, netType, true, logger)
            {} //CC groups are always ext only

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
            std::set<AddressEntryType> getAddressTypeSet(void) const override;

         protected:
            std::shared_ptr<Leaf> newLeaf(AddressEntryType) const override;
         };

         ///////////////////////////////////////////////////////////////////////
         class SettlementGroup : public Group
         {
            friend class hd::Wallet;

         public:
            SettlementGroup(std::shared_ptr<AssetWallet_Single> walletPtr
               , NetworkType netType, const std::shared_ptr<spdlog::logger> &logger)
               : Group(walletPtr, bs::hd::CoinType::BlockSettle_Settlement
                  , netType, true, logger)
            {} //Settlement groups are always ext only

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
            std::set<AddressEntryType> getAddressTypeSet(void) const override;

            //these will throw on purpose, settlement leafs arent deterministic,
            //they need a dedicated setup routine
            std::shared_ptr<hd::Leaf> createLeaf(AddressEntryType
               , bs::hd::Path::Elem, unsigned lookup = UINT32_MAX) override
            { return nullptr; }

            std::shared_ptr<hd::Leaf> createLeaf(AddressEntryType
               , const std::string &, unsigned lookup = UINT32_MAX) override
            { return nullptr; }

            std::shared_ptr<hd::SettlementLeaf>
               getLeafForSettlementID(const SecureBinaryData&) const;

         protected:
            std::shared_ptr<Leaf> newLeaf(AddressEntryType) const override;

            //
            void initLeaf(std::shared_ptr<hd::Leaf> &, const bs::hd::Path &,
               unsigned lookup = UINT32_MAX) const override
            {
               throw AccountException("cannot setup ECDH accounts from HD account routines");
            }

            void initLeaf(std::shared_ptr<hd::Leaf> &, 
               const SecureBinaryData&, const SecureBinaryData&) const;
            void serializeLeaves(BinaryWriter &) const override;

         private:
            std::shared_ptr<hd::Leaf> createLeaf(const bs::Address&, const bs::hd::Path&);
            void deserialize(BinaryDataRef value) override;
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_GROUP_H

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

         class Group
         {
            friend class hd::Wallet;

         public:
            Group(std::shared_ptr<AssetWallet_Single>, const bs::hd::Path &path, 
               NetworkType netType, bool isExtOnly, 
               const std::shared_ptr<spdlog::logger> &logger = nullptr);

            size_t getNumLeaves() const { return leaves_.size(); }
            std::shared_ptr<hd::Leaf> getLeaf(bs::hd::Path::Elem) const;
            std::shared_ptr<hd::Leaf> getLeaf(const std::string &key) const;
            std::vector<std::shared_ptr<hd::Leaf>> getLeaves() const;
            std::vector<std::shared_ptr<bs::core::Wallet>> getAllLeaves() const;
            std::shared_ptr<Leaf> createLeaf(bs::hd::Path::Elem);
            std::shared_ptr<Leaf> createLeaf(const std::string &key);
            virtual std::shared_ptr<Leaf> newLeaf() const;
            virtual bool addLeaf(const std::shared_ptr<Leaf> &);
            bool deleteLeaf(const std::shared_ptr<bs::core::Wallet> &);
            bool deleteLeaf(const bs::hd::Path::Elem &);
            bool deleteLeaf(const std::string &key);

            virtual wallet::Type type() const { return wallet::Type::Bitcoin; }
            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }

            virtual void setChainCode(const BinaryData &) {}
            virtual void shutdown(void);
            virtual std::set<AddressEntryType> getAddressTypeSet(void) const;
            bool isExtOnly(void) const { return isExtOnly_; }

         protected:
            bool needsCommit() const { return needsCommit_; }
            void committed() { needsCommit_ = false; }

            virtual void serializeLeaves(BinaryWriter &) const;
            virtual void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &) const;

            bs::hd::Path   path_;
            std::shared_ptr<spdlog::logger>  logger_;
            bool        needsCommit_ = true;
            NetworkType netType_;
            bool isExtOnly_ = false;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<hd::Leaf>> leaves_;

            std::shared_ptr<AssetWallet_Single> walletPtr_;

         private:
            BinaryData serialize() const;
            void copyLeaves(hd::Group*);

            static std::shared_ptr<Group> deserialize(
               std::shared_ptr<AssetWallet_Single>, 
               BinaryDataRef key, BinaryDataRef val
               , const std::string &name
               , const std::string &desc
               , NetworkType netType
               , const std::shared_ptr<spdlog::logger> &logger);
            void deserialize(BinaryDataRef value);
         };

         class AuthGroup : public Group
         {
         public:
            AuthGroup(std::shared_ptr<AssetWallet_Single>,
               const bs::hd::Path &path,
               NetworkType netType,
               const std::shared_ptr<spdlog::logger> &);

            wallet::Type type() const override { return wallet::Type::Authentication; }

            void setChainCode(const BinaryData &) override;
            void shutdown(void) override;
            std::set<AddressEntryType> getAddressTypeSet(void) const override;

         protected:
            bool addLeaf(const std::shared_ptr<Leaf> &) override;
            std::shared_ptr<Leaf> newLeaf() const override;
            void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &) const;
            void serializeLeaves(BinaryWriter &) const override;

            BinaryData  chainCode_;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<Leaf>>  tempLeaves_;
         };


         class CCGroup : public Group
         {
         public:
            CCGroup(std::shared_ptr<AssetWallet_Single> walletPtr,
               const bs::hd::Path &path, NetworkType netType,
               const std::shared_ptr<spdlog::logger> &logger)
               : Group(walletPtr, path, netType, true, logger) 
            {} //CC groups are always ext only

            wallet::Type type() const override { return wallet::Type::ColorCoin; }
            std::set<AddressEntryType> getAddressTypeSet(void) const override;

         protected:
            std::shared_ptr<Leaf> newLeaf() const override;
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_GROUP_H

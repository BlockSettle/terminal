#ifndef BS_CORE_HD_GROUP_H
#define BS_CORE_HD_GROUP_H

#include <unordered_map>
#include "CoreHDLeaf.h"

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
            Group(Nodes rootNodes, const bs::hd::Path &path, const std::string &walletName
               , const std::string &desc
               , const std::shared_ptr<spdlog::logger> &logger = nullptr
               , bool extOnlyAddresses = false);

            std::shared_ptr<Group> createWatchingOnly(const std::shared_ptr<Node> &extNode) const;

            size_t getNumLeaves() const { return leaves_.size(); }
            std::shared_ptr<hd::Leaf> getLeaf(bs::hd::Path::Elem) const;
            std::shared_ptr<hd::Leaf> getLeaf(const std::string &key) const;
            std::vector<std::shared_ptr<hd::Leaf>> getLeaves() const;
            std::vector<std::shared_ptr<bs::core::Wallet>> getAllLeaves() const;
            std::shared_ptr<Leaf> createLeaf(bs::hd::Path::Elem, const std::shared_ptr<Node> &extNode = nullptr);
            std::shared_ptr<Leaf> createLeaf(const std::string &key, const std::shared_ptr<Node> &extNode = nullptr);
            virtual std::shared_ptr<Leaf> newLeaf() const;
            virtual bool addLeaf(const std::shared_ptr<Leaf> &);
            bool deleteLeaf(const std::shared_ptr<bs::core::Wallet> &);
            bool deleteLeaf(const bs::hd::Path::Elem &);
            bool deleteLeaf(const std::string &key);

            virtual wallet::Type type() const { return wallet::Type::Bitcoin; }
            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }
            std::string desc() const { return desc_; }
            virtual void updateRootNodes(Nodes, const std::shared_ptr<Node> &decrypted);

            virtual void setChainCode(const BinaryData &) {}

         protected:
            bool needsCommit() const { return needsCommit_; }
            void committed() { needsCommit_ = false; }

            virtual void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db);
            virtual void serializeLeaves(BinaryWriter &) const;
            virtual void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &, const std::shared_ptr<Node> &extNode) const;
            void copyLeaf(std::shared_ptr<hd::Group> &target, bs::hd::Path::Elem leafIndex, const std::shared_ptr<hd::Leaf> &
               , const std::shared_ptr<Node> &extNode) const;
            virtual std::shared_ptr<Group> createWO() const;
            virtual void fillWO(std::shared_ptr<hd::Group> &, const std::shared_ptr<Node> &extNode) const;

            NetworkType    netType_;
            Nodes          rootNodes_;
            bs::hd::Path   path_;
            std::string    walletName_, desc_;
            LMDB  *db_ = nullptr;
            std::shared_ptr<LMDBEnv>         dbEnv_;
            std::shared_ptr<spdlog::logger>  logger_;
            bool        extOnlyAddresses_;
            bool        needsCommit_ = true;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<hd::Leaf>> leaves_;

         private:
            BinaryData serialize() const;

            static std::shared_ptr<Group> deserialize(BinaryDataRef key
               , BinaryDataRef val
               , Nodes rootNodes
               , const std::string &name
               , const std::string &desc
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses);
            void deserialize(BinaryDataRef value);
         };


         class AuthGroup : public Group
         {
         public:
            AuthGroup(Nodes rootNodes, const bs::hd::Path &path, const std::string &walletName
               , const std::string &desc, const std::shared_ptr<spdlog::logger> &
               , bool extOnlyAddresses = false);

            wallet::Type type() const override { return wallet::Type::Authentication; }

            void setChainCode(const BinaryData &) override;
            void updateRootNodes(Nodes, const std::shared_ptr<Node> &decrypted) override;

         protected:
            void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db) override;
            bool addLeaf(const std::shared_ptr<Leaf> &) override;
            std::shared_ptr<Leaf> newLeaf() const override;
            void initLeaf(std::shared_ptr<Leaf> &, const bs::hd::Path &, const std::shared_ptr<Node> &extNode) const override;
            std::shared_ptr<Group> createWO() const override;
            void fillWO(std::shared_ptr<hd::Group> &, const std::shared_ptr<Node> &extNode) const override;
            void serializeLeaves(BinaryWriter &) const override;

            BinaryData  chainCode_;
            std::unordered_map<bs::hd::Path::Elem, std::shared_ptr<Leaf>>  tempLeaves_;
         };


         class CCGroup : public Group
         {
         public:
            CCGroup(Nodes rootNodes, const bs::hd::Path &path
               , const std::string &walletName, const std::string &desc
               , const std::shared_ptr<spdlog::logger> &logger
               , bool extOnlyAddresses = false)
               : Group(rootNodes, path, walletName, desc, logger, extOnlyAddresses) {}

            wallet::Type type() const override { return wallet::Type::ColorCoin; }

         protected:
            std::shared_ptr<Leaf> newLeaf() const override;
            std::shared_ptr<Group> createWO() const override;
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_GROUP_H

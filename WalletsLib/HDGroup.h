#ifndef __BS_HD_GROUP_H__
#define __BS_HD_GROUP_H__

#include <unordered_map>
#include <QObject>
#include <lmdbpp.h>
#include "HDNode.h"
#include "HDLeaf.h"


namespace bs {
   namespace hd {
      class Wallet;

      class Group : public QObject
      {
         Q_OBJECT
         friend class bs::hd::Wallet;

      public:
         Group(Nodes rootNodes, const Path &path, const std::string &walletName, const std::string &name, const std::string &desc
            , bool extOnlyAddresses = false)
            : QObject(nullptr), rootNodes_(rootNodes), path_(path), walletName_(walletName), name_(name), desc_(desc), extOnlyAddresses_(extOnlyAddresses) {}

         std::shared_ptr<Group> CreateWatchingOnly(const std::shared_ptr<Node> &extNode) const;

         size_t getNumLeaves() const { return leaves_.size(); }
         std::shared_ptr<hd::Leaf> getLeaf(Path::Elem) const;
         std::shared_ptr<hd::Leaf> getLeaf(const std::string &key) const;
         std::vector<std::shared_ptr<bs::hd::Leaf>> getLeaves() const;
         std::vector<std::shared_ptr<bs::Wallet>> getAllLeaves() const;
         std::shared_ptr<Leaf> createLeaf(Path::Elem, const std::shared_ptr<Node> &extNode = nullptr);
         std::shared_ptr<Leaf> createLeaf(const std::string &key, const std::shared_ptr<Node> &extNode = nullptr);
         virtual std::shared_ptr<Leaf> newLeaf() const;
         virtual bool addLeaf(const std::shared_ptr<Leaf> &, bool signal = false);
         bool deleteLeaf(const std::shared_ptr<bs::Wallet> &);
         bool deleteLeaf(const Path::Elem &);
         bool deleteLeaf(const std::string &key);
         static std::string nameForType(CoinType ct);

         virtual bs::wallet::Type getType() const { return bs::wallet::Type::Bitcoin; }
         const Path &getPath() const { return path_; }
         Path::Elem getIndex() const { return static_cast<Path::Elem>(path_.get(-1)); }
         std::string getName() const { return name_; }
         std::string getDesc() const { return desc_; }
         virtual void updateRootNodes(Nodes, const std::shared_ptr<Node> &decrypted);

         bool needsCommit() const { return needsCommit_; }
         void committed() { needsCommit_ = false; }

         virtual void setUserID(const BinaryData &) {}

      signals:
         void changed();
         void leafAdded(QString id);
         void leafDeleted(QString id);

      private slots:
         void onLeafChanged();

      protected:
         using cb_scan_notify = std::function<void(Group *, Path::Elem wallet, bool isValid)>;
         using cb_scan_read_last = std::function<unsigned int(const std::string &walletId)>;
         using cb_scan_write_last = std::function<void(const std::string &walletId, unsigned int idx)>;

         virtual void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db);
         virtual void serializeLeaves(BinaryWriter &) const;
         virtual void rescanBlockchain(const cb_scan_notify &, const cb_scan_read_last &cbr = nullptr
            , const cb_scan_write_last &cbw = nullptr);
         virtual void initLeaf(std::shared_ptr<Leaf> &, const Path &, const std::shared_ptr<Node> &extNode) const;
         void copyLeaf(std::shared_ptr<hd::Group> &target, hd::Path::Elem leafIndex, const std::shared_ptr<hd::Leaf> &
            , const std::shared_ptr<Node> &extNode) const;
         virtual std::shared_ptr<Group> CreateWO() const;
         virtual void FillWO(std::shared_ptr<hd::Group> &, const std::shared_ptr<Node> &extNode) const;

      protected:
         Nodes       rootNodes_;
         Path        path_;
         std::string walletName_, name_, desc_;
         LMDB  *     db_ = nullptr;
         shared_ptr<LMDBEnv>     dbEnv_ = nullptr;
         bool        extOnlyAddresses_;
         bool        needsCommit_ = true;
         std::unordered_map<Path::Elem, std::shared_ptr<hd::Leaf>>  leaves_;
         unsigned int   scanPortion_ = 100;

      private:
         BinaryData serialize() const;
         static std::shared_ptr<Group> deserialize(BinaryDataRef key, BinaryDataRef val, Nodes rootNodes
            , const std::string &name, const std::string &desc, bool extOnlyAddresses);
         void deserialize(BinaryDataRef value);
      };


      class AuthGroup : public Group
      {
         Q_OBJECT
      
      public:
         AuthGroup(Nodes rootNodes, const Path &path, const std::string &name
            , const std::string &desc, bool extOnlyAddresses = false);

         bs::wallet::Type getType() const override { return bs::wallet::Type::Authentication; }

         void setUserID(const BinaryData &usedId) override;
         void updateRootNodes(Nodes, const std::shared_ptr<Node> &decrypted) override;

      protected:
         void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db) override;
         bool addLeaf(const std::shared_ptr<Leaf> &, bool signal = false) override;
         std::shared_ptr<Leaf> newLeaf() const override;
         void initLeaf(std::shared_ptr<Leaf> &, const Path &, const std::shared_ptr<Node> &extNode) const override;
         std::shared_ptr<Group> CreateWO() const override;
         void FillWO(std::shared_ptr<hd::Group> &, const std::shared_ptr<Node> &extNode) const override;
         void serializeLeaves(BinaryWriter &) const override;

      protected:
         BinaryData  userId_;
         std::unordered_map<Path::Elem, std::shared_ptr<Leaf>>  tempLeaves_;
      };


      class CCGroup : public Group
      {
         Q_OBJECT

      public:
         CCGroup(Nodes rootNodes, const Path &path, const std::string &name
            , const std::string &desc, bool extOnlyAddresses = false)
            : Group(rootNodes, path, name, nameForType(CoinType::BlockSettle_CC), desc, extOnlyAddresses) {}

         bs::wallet::Type getType() const override { return bs::wallet::Type::ColorCoin; }

      protected:
         std::shared_ptr<Leaf> newLeaf() const override;
         std::shared_ptr<Group> CreateWO() const override;
      };

   }  //namespace hd
}  //namespace bs

#endif //__BS_HD_GROUP_H__

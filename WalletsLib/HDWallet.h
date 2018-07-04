#ifndef __BS_HD_WALLET_H__
#define __BS_HD_WALLET_H__

#include <functional>
#include <lmdbpp.h>
#include "HDGroup.h"
#include "HDLeaf.h"
#include "HDNode.h"


namespace bs {
   namespace hd {

      class Wallet : public QObject
      {
         Q_OBJECT
      public:
         using cb_scan_notify = std::function<void(Group *, Path::Elem wallet, bool isValid)>;
         using cb_scan_read_last = std::function<unsigned int(const std::string &walletId)>;
         using cb_scan_write_last = std::function<void(const std::string &walletId, unsigned int idx)>;

         Wallet(const std::string &name, const std::string &desc
            , const bs::wallet::Seed &, bool extOnlyAddresses = false);
         Wallet(const std::string &filename, bool extOnlyAddresses = false);
         Wallet(const std::string &walletId, NetworkType netType, bool extOnlyAddresses
            , const std::string &name, const std::string &desc);
         ~Wallet() override;

         Wallet(const Wallet&) = delete;
         Wallet& operator = (const Wallet&) = delete;
         Wallet(Wallet&&) = delete;
         Wallet& operator = (Wallet&&) = delete;

         std::shared_ptr<hd::Wallet> CreateWatchingOnly(const SecureBinaryData &password) const;
         bool isWatchingOnly() const { return (rootNode_ == nullptr); }
         bool isEncrypted() const { return (rootNode_ && rootNode_->isEncrypted()); }
         bool isPrimary() const;
         NetworkType networkType() const { return netType_; }

         std::shared_ptr<Group> getGroup(CoinType ct) const;
         std::shared_ptr<Group> createGroup(CoinType ct);
         void addGroup(const std::shared_ptr<Group> &group);
         size_t getNumGroups() const { return groups_.size(); }
         std::vector<std::shared_ptr<Group>> getGroups() const;
         virtual size_t getNumLeaves() const;
         std::vector<std::shared_ptr<bs::Wallet>> getLeaves() const;
         std::shared_ptr<bs::Wallet> getLeaf(const std::string &id) const;

         virtual std::string getWalletId() const;
         std::string getName() const { return name_; }
         std::string getDesc() const { return desc_; }
         const std::shared_ptr<Node> &getNode() const { return rootNode_; }

         void createStructure();
         void setUserId(const BinaryData &usedId);
         bool eraseFile();
         bool changePassword(const SecureBinaryData &newPass, const SecureBinaryData &oldPass = {});

         void RegisterWallet(const std::shared_ptr<PyBlockDataManager>& bdm, bool asNew = false);
         void SetBDM(const std::shared_ptr<PyBlockDataManager> &bdm);
         bool startRescan(const cb_scan_notify &, const cb_scan_read_last &cbr = nullptr, const cb_scan_write_last &cbw = nullptr);
         void saveToDir(const std::string &targetDir);
         void saveToFile(const std::string &filename, bool force = false);
         void copyToFile(const std::string &filename);
         static std::string fileNamePrefix(bool watchingOnly);
         CoinType getXBTGroupType() const { return ((netType_ == NetworkType::MainNet) ? CoinType::Bitcoin_main : CoinType::Bitcoin_test); }
         void updatePersistence();

      signals:
         void leafAdded(QString id);
         void leafDeleted(QString id);
         void scanComplete(const std::string &walletId);

      private slots:
         void onGroupChanged();
         void onLeafAdded(QString id);
         void onLeafDeleted(QString id);
         void onScanComplete(const std::string &leafId);

      protected:
         std::string    walletId_;
         std::string    name_, desc_;
         NetworkType    netType_;
         bool           extOnlyAddresses_;
         std::string    dbFilename_;
         LMDB  *        db_ = nullptr;
         shared_ptr<LMDBEnv>     dbEnv_ = nullptr;
         std::shared_ptr<Node>   rootNode_;
         std::map<Path::Elem, std::shared_ptr<Group>>                groups_;
         mutable std::map<std::string, std::shared_ptr<bs::Wallet>>  leaves_;
         mutable QMutex    mtxGroups_;
         BinaryData        userId_;

      protected:
         void initNew(const bs::wallet::Seed &);
         void loadFromFile(const std::string &filename);
         std::string getFileName(const std::string &dir) const;
         void openDBEnv(const std::string &filename);
         void openDB();
         void initDB();
         void putDataToDB(LMDB* db, const BinaryData& key, const BinaryData& data);
         BinaryDataRef getDataRefForKey(LMDB* db, const BinaryData& key) const;
         BinaryDataRef getDataRefForKey(uint32_t key) const;
         void writeToDB(bool force = false);
         void readFromDB();
         void setDBforDependants();
         void rescanBlockchain(const cb_scan_notify &, const cb_scan_read_last &, const cb_scan_write_last &);

      private:
         std::unordered_set<std::string>  scannedLeaves_;
      };

      class DummyWallet : public bs::hd::Wallet    // Just a container for old-style wallets
      {
      public:
         explicit DummyWallet() : bs::hd::Wallet(tr("Armory Wallets").toStdString(), "") {}

         size_t getNumLeaves() const override { return leaves_.size(); }
         void add(const std::shared_ptr<bs::Wallet> wallet) { leaves_[wallet->GetWalletId()] = wallet; }
         std::string getWalletId() const override { return "Dummy"; }
      };


   }  //namespace hd
}  //namespace bs

#endif //__BS_HD_WALLET_H__

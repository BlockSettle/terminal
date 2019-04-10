#ifndef BS_CORE_HD_WALLET_H__
#define BS_CORE_HD_WALLET_H__

#include <memory>
#include "CoreHDGroup.h"
#include "CoreHDLeaf.h"
#include "CoreHDNode.h"

namespace spdlog {
   class logger;
}

namespace bs {
   namespace core {
      namespace hd {

         class Wallet
         {
         public:
            Wallet(const std::string &name, const std::string &desc
               , const wallet::Seed &
               , const std::shared_ptr<spdlog::logger> &logger = nullptr
               , bool extOnlyAddresses = false);
            Wallet(const std::string &filename
               , const std::shared_ptr<spdlog::logger> &logger = nullptr
               , bool extOnlyAddresses = false);
            Wallet(const std::string &walletId, NetworkType netType
               , bool extOnlyAddresses, const std::string &name
               , const std::shared_ptr<spdlog::logger> &logger = nullptr
               , const std::string &desc = {});
            virtual ~Wallet();

            Wallet(const Wallet&) = delete;
            Wallet& operator = (const Wallet&) = delete;
            Wallet(Wallet&&) = delete;
            Wallet& operator = (Wallet&&) = delete;

            std::shared_ptr<hd::Wallet> createWatchingOnly(const SecureBinaryData &password) const;
            bool isWatchingOnly() const { return rootNodes_.empty(); }
            std::vector<bs::wallet::EncryptionType> encryptionTypes() const { return rootNodes_.encryptionTypes(); }
            std::vector<SecureBinaryData> encryptionKeys() const { return rootNodes_.encryptionKeys(); }
            bs::wallet::KeyRank encryptionRank() const { return rootNodes_.rank(); }
            bool isPrimary() const;
            NetworkType networkType() const { return netType_; }

            std::shared_ptr<Node> getRootNode(const SecureBinaryData &password) const { return rootNodes_.decrypt(password); }
            std::shared_ptr<Group> getGroup(bs::hd::CoinType ct) const;
            std::shared_ptr<Group> createGroup(bs::hd::CoinType ct);
            void addGroup(const std::shared_ptr<Group> &group);
            size_t getNumGroups() const { return groups_.size(); }
            std::vector<std::shared_ptr<Group>> getGroups() const;
            virtual size_t getNumLeaves() const;
            std::vector<std::shared_ptr<bs::core::Wallet>> getLeaves() const;
            std::shared_ptr<bs::core::Wallet> getLeaf(const std::string &id) const;

            virtual std::string walletId() const { return walletId_; }
            std::string name() const { return name_; }
            std::string description() const { return desc_; }

            void createStructure();
            void setChainCode(const BinaryData &);
            bool eraseFile();

            // addNew: add new encryption key without asking for all old keys (used with multiple Auth eID devices).
            // removeOld: remove missed keys comparing encKey field without asking for all old keys
            // (newPass password fields should be empty). Used with multiple Auth eID devices.
            // dryRun: check that old password valid. No password change happens.
            bool changePassword(const std::vector<bs::wallet::PasswordData> &newPass, bs::wallet::KeyRank
               , const SecureBinaryData &oldPass, bool addNew, bool removeOld, bool dryRun);

            void saveToDir(const std::string &targetDir);
            void saveToFile(const std::string &filename, bool force = false);
            void copyToFile(const std::string &filename);
            static std::string fileNamePrefix(bool watchingOnly);
            bs::hd::CoinType getXBTGroupType() const { return ((netType_ == NetworkType::MainNet)
               ? bs::hd::CoinType::Bitcoin_main : bs::hd::CoinType::Bitcoin_test); }
            void updatePersistence();

         protected:
            std::string    walletId_;
            std::string    name_, desc_;
            NetworkType    netType_ = NetworkType::Invalid;
            bool           extOnlyAddresses_;
            std::string    dbFilename_;
            LMDB  *        db_ = nullptr;
            std::shared_ptr<LMDBEnv>     dbEnv_ = nullptr;
            Nodes    rootNodes_;
            std::map<bs::hd::Path::Elem, std::shared_ptr<Group>>              groups_;
            mutable std::map<std::string, std::shared_ptr<bs::core::Wallet>>  leaves_;
            BinaryData        chainCode_;
            std::shared_ptr<spdlog::logger>     logger_;

         protected:
            void initNew(const wallet::Seed &);
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
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_WALLET_H__

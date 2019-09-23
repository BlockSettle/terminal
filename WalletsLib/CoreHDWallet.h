#ifndef BS_CORE_HD_WALLET_H__
#define BS_CORE_HD_WALLET_H__

#include <deque>
#include <memory>
#include "CoreHDGroup.h"
#include "CoreHDLeaf.h"
#include "WalletEncryption.h"


namespace spdlog {
   class logger;
}

namespace bs {
   namespace core {
      namespace hd {

         class Wallet
         {
         private:
            Wallet(void) {}
            
            std::shared_ptr<AddressEntry_P2WSH> getAddressPtrForSettlement(
               const SecureBinaryData& settlementID,
               const SecureBinaryData& counterPartyPubKey,
               bool isMyKeyFirst) const;

            std::shared_ptr<hd::SettlementLeaf> getLeafForSettlementID(
               const SecureBinaryData &settlementID) const;

            std::shared_ptr<AssetEntry> getAssetForAddress(const bs::Address &);

         public:
            //init from seed
            Wallet(const std::string &name, const std::string &desc
               , const wallet::Seed &, const bs::wallet::PasswordData &
               , const std::string& folder = "./"
               , const std::shared_ptr<spdlog::logger> &logger = nullptr);

            //load existing wallet
            Wallet(const std::string &filename, NetworkType netType,
               const std::string& folder = "",
               const std::shared_ptr<spdlog::logger> &logger = nullptr);

            //generate random seed and init
            Wallet(const std::string &name, const std::string &desc
               , NetworkType netType, const bs::wallet::PasswordData &
               , const std::string& folder = "./"
               , const std::shared_ptr<spdlog::logger> &logger = nullptr);

            std::vector<bs::wallet::EncryptionType> encryptionTypes() const;
            std::vector<BinaryData> encryptionKeys() const;
            bs::wallet::KeyRank encryptionRank() const { return {1, (unsigned int)pwdMeta_.size() }; }

            ~Wallet(void);

            Wallet(const Wallet&) = delete;
            Wallet& operator = (const Wallet&) = delete;
            Wallet(Wallet&&) = delete;
            Wallet& operator = (Wallet&&) = delete;

            std::shared_ptr<hd::Wallet> createWatchingOnly(void) const;
            bool isWatchingOnly() const;
            bool isPrimary() const;
            NetworkType networkType() const { return netType_; }
            void setExtOnly(void);
            bool isExtOnly() const { return extOnlyFlag_; }

            std::shared_ptr<Group> getGroup(bs::hd::CoinType ct) const;
            std::shared_ptr<Group> createGroup(bs::hd::CoinType ct);
            void addGroup(const std::shared_ptr<Group> &group);
            size_t getNumGroups() const { return groups_.size(); }
            std::vector<std::shared_ptr<Group>> getGroups() const;
            size_t getNumLeaves() const;
            std::vector<std::shared_ptr<Leaf>> getLeaves() const;
            std::shared_ptr<Leaf> getLeaf(const std::string &id) const;

            std::string walletId() const { return walletPtr_->getID(); }
            std::string name() const { return name_; }
            std::string description() const { return desc_; }

            void createStructure(unsigned lookup = UINT32_MAX);
            void shutdown();
            bool eraseFile();
            const std::string& getFileName(void) const;
            void copyToFile(const std::string& filename);

            bool changePassword(const bs::wallet::PasswordMetaData &oldPD
               , const bs::wallet::PasswordData &newPass);
            bool addPassword(const bs::wallet::PasswordData &);

            void pushPasswordPrompt(const std::function<SecureBinaryData()> &);
            void popPasswordPrompt();

            static std::string fileNamePrefix(bool watchingOnly);
            bs::hd::CoinType getXBTGroupType() const { 
               return ((netType_ == NetworkType::MainNet)
               ? bs::hd::CoinType::Bitcoin_main : bs::hd::CoinType::Bitcoin_test); 
            }

            bs::core::wallet::Seed getDecryptedSeed(void) const;
            SecureBinaryData getDecryptedRootXpriv(void) const;

            //settlement leaves methods
            std::shared_ptr<hd::Leaf> createSettlementLeaf(const bs::Address&);
            std::shared_ptr<hd::Leaf> getSettlementLeaf(const bs::Address&);

            bs::Address getSettlementPayinAddress(const wallet::SettlementData &) const;

            BinaryData signSettlementTXRequest(const wallet::TXSignRequest &
               , const wallet::SettlementData &);

         protected:
            std::string    name_, desc_;
            NetworkType    netType_ = NetworkType::Invalid;
            std::map<bs::hd::Path::Elem, std::shared_ptr<Group>>  groups_;
            std::vector<bs::wallet::PasswordMetaData>             pwdMeta_;
            std::shared_ptr<spdlog::logger>     logger_;
            bool extOnlyFlag_ = false;

            std::shared_ptr<AssetWallet_Single> walletPtr_;
            
            std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
            LMDB* db_ = nullptr;

            std::deque<std::function<SecureBinaryData(const std::set<BinaryData> &)>>  lbdPwdPrompts_;

         protected:
            void initNew(const wallet::Seed &, const bs::wallet::PasswordData &
               , const std::string &folder);
            void loadFromFile(const std::string &filename, const std::string& folder);
            void putDataToDB(const BinaryData& key, const BinaryData& data);
            BinaryDataRef getDataRefForKey(LMDB* db, const BinaryData& key) const;
            BinaryDataRef getDataRefForKey(uint32_t key) const;
            void writeToDB(bool force = false);

            bs::hd::Path getPathForAddress(const bs::Address &);

            void initializeDB();
            void readFromDB();
         };

      }  //namespace hd

      struct WalletPasswordScoped
      {
      private:
         WalletPasswordScoped(const WalletPasswordScoped&) = delete;
         WalletPasswordScoped(WalletPasswordScoped&& lock) = delete;
         WalletPasswordScoped& operator=(const WalletPasswordScoped&) = delete;
         WalletPasswordScoped& operator=(WalletPasswordScoped&&) = delete;

      public:
         WalletPasswordScoped(const std::shared_ptr<hd::Wallet> &wallet
            , const SecureBinaryData &passphrase);

         ~WalletPasswordScoped()
         {
            wallet_->popPasswordPrompt();
         }

      private:
         std::shared_ptr<hd::Wallet>   wallet_;
         const unsigned int   maxTries_ = 32;   // Too low values may cause unexpected failures
         unsigned int         nbTries_ = 0;     // when creating many wallets at once, for example
      };

   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_WALLET_H__

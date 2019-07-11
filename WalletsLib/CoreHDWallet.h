#ifndef BS_CORE_HD_WALLET_H__
#define BS_CORE_HD_WALLET_H__

#include <memory>
#include "CoreHDGroup.h"
#include "CoreHDLeaf.h"


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
               const SecureBinaryData&) const;

            std::shared_ptr<AssetEntry> getAssetForAddress(const bs::Address&);

         public:

            //init from seed
            Wallet(const std::string &name, const std::string &desc
               , const wallet::Seed &, const SecureBinaryData& passphrase
               , const std::string& folder = "./"
               , const std::shared_ptr<spdlog::logger> &logger = nullptr);

            //load existing wallet
            Wallet(const std::string &filename, NetworkType netType,
               const std::string& folder = "",
               const std::shared_ptr<spdlog::logger> &logger = nullptr);

            //generate random seed and init
            Wallet(const std::string &name, const std::string &desc
               , NetworkType netType, const SecureBinaryData& passphrase
               , const std::string& folder = "./"
               , const std::shared_ptr<spdlog::logger> &logger = nullptr);

            //stand in for the botched bs encryption code. too expensive to clean up after this mess
            virtual std::vector<bs::wallet::EncryptionType> encryptionTypes() const { return { bs::wallet::EncryptionType::Password }; }
            virtual std::vector<SecureBinaryData> encryptionKeys() const { return {}; }
            virtual std::pair<unsigned int, unsigned int> encryptionRank() const { return { 1, 1 }; }

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
            virtual size_t getNumLeaves() const;
            std::vector<std::shared_ptr<Leaf>> getLeaves() const;
            std::shared_ptr<Leaf> getLeaf(const std::string &id) const;

            virtual std::string walletId() const { return walletPtr_->getID(); }
            std::string name() const { return name_; }
            std::string description() const { return desc_; }

            void createStructure(unsigned lookup = UINT32_MAX);
            void shutdown();
            bool eraseFile();
            const std::string& getFileName(void) const;
            void copyToFile(const std::string& filename);

            bool changePassword(const SecureBinaryData& newPass);
            WalletEncryptionLock lockForEncryption(const SecureBinaryData& passphrase);

            static std::string fileNamePrefix(bool watchingOnly);
            bs::hd::CoinType getXBTGroupType() const { 
               return ((netType_ == NetworkType::MainNet)
               ? bs::hd::CoinType::Bitcoin_main : bs::hd::CoinType::Bitcoin_test); 
            }

            bs::core::wallet::Seed getDecryptedSeed(void) const;
            SecureBinaryData getDecryptedRootXpriv(void) const;

            //settlement leaves methods
            std::shared_ptr<hd::Leaf> createSettlementLeaf(const bs::Address&);

            bs::Address getSettlementPayinAddress(
               const SecureBinaryData&,
               const SecureBinaryData&,
               bool) const;

            BinaryData signSettlementTXRequest(
               const wallet::TXSignRequest &,
               const BinaryData& settlementId,
               const SecureBinaryData& counterPartyPubKey,
               bool myKeyComesFirst);


         protected:
            std::string    name_, desc_;
            NetworkType    netType_ = NetworkType::Invalid;
            std::map<bs::hd::Path::Elem, std::shared_ptr<Group>> groups_;
            std::shared_ptr<spdlog::logger>     logger_;
            bool extOnlyFlag_ = false;

            std::shared_ptr<AssetWallet_Single> walletPtr_;
            
            std::shared_ptr<LMDBEnv> dbEnv_ = nullptr;
            LMDB* db_ = nullptr;


         protected:
            void initNew(const wallet::Seed &, 
               const SecureBinaryData& passphrase, const std::string& folder);
            void loadFromFile(const std::string &filename, const std::string& folder);
            void putDataToDB(const BinaryData& key, const BinaryData& data);
            BinaryDataRef getDataRefForKey(LMDB* db, const BinaryData& key) const;
            BinaryDataRef getDataRefForKey(uint32_t key) const;
            void writeGroupsToDB(bool force = false);

            void initializeDB();
            void readFromDB();
         };

      }  //namespace hd
   }  //namespace core
}  //namespace bs

#endif //BS_CORE_HD_WALLET_H__

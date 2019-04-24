#ifndef BS_CORE_PLAIN_WALLET_H
#define BS_CORE_PLAIN_WALLET_H

#include <atomic>
#include <unordered_map>
#include <BinaryData.h>

#include "BlockDataManagerConfig.h"
#include "BtcDefinitions.h"
#include "CoreWallet.h"


namespace spdlog {
   class logger;
};

namespace bs {
   namespace core {
      class PlainAsset : public GenericAsset
      {
      public:
         PlainAsset(int id, const bs::Address &addr, const SecureBinaryData &privKey = {})
            : GenericAsset(AssetEntryType_Single, id), address_(addr), privKey_(privKey) {}
         ~PlainAsset() override = default;

         static std::pair<bs::Address, std::shared_ptr<PlainAsset>> generateRandom(AddressEntryType);
         static std::pair<bs::Address, std::shared_ptr<PlainAsset>> deserialize(BinaryDataRef value);
         BinaryData serialize(void) const override;

         SecureBinaryData publicKey() const;
         SecureBinaryData privKey() const { return privKey_; }
         bs::Address address() const { return address_; }

         bool hasPrivateKey(void) const override { return !privKey_.isNull(); }
         const BinaryData &getPrivateEncryptionKeyId(void) const override { return address_; }

      private:
         bs::Address       address_;
         SecureBinaryData  privKey_;
         mutable SecureBinaryData   pubKey_;
      };

      // A base wallet that can be used by other wallets, or for very basic
      // functionality (e.g., creating a bare wallet that can be registered and get
      // info on addresses added to the wallet). The wallet may or may not be able
      // to access the wallet DB.
      class PlainWallet : public Wallet
      {
      public:
         PlainWallet(NetworkType, const std::string &name, const std::string &desc
            , const std::shared_ptr<spdlog::logger> &logger = nullptr);
         PlainWallet(NetworkType, const std::string &filename
            , const std::shared_ptr<spdlog::logger> &logger = nullptr);
         PlainWallet(NetworkType, const std::shared_ptr<spdlog::logger> &logger = nullptr);
         ~PlainWallet() override;

         PlainWallet(const PlainWallet&) = delete;
         PlainWallet(PlainWallet&&) = delete;
         PlainWallet& operator = (const PlainWallet&) = delete;
         PlainWallet& operator = (PlainWallet&&) = delete;

         static std::string fileNamePrefix(bool) { return "plain_"; }
         void saveToDir(const std::string &targetDir);
         void saveToFile(const std::string &filename);
         void setLogger(const std::shared_ptr<spdlog::logger> &logger) {
            logger_ = logger;
         }

         virtual int addAddress(const bs::Address &, const std::shared_ptr<GenericAsset> &asset = nullptr);
         bool containsAddress(const bs::Address &addr) override;

         std::string walletId() const override { return walletId_; }
         std::string description() const { return desc_; }
         void setDescription(const std::string &desc) { desc_ = desc; }
         wallet::Type type() const override { return wallet::Type::Bitcoin; }

         BinaryData getRootId() const override { return BinaryData(); }

         std::shared_ptr<ResolverFeed> getResolver(void) const;

         bs::Address getNewExtAddress(AddressEntryType) override { return {}; }
         bs::Address getNewIntAddress(AddressEntryType) override { return {}; }

         unsigned getUsedAddressCount() const override { return usedAddresses_.size(); }
         std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
         std::string getAddressIndex(const bs::Address &) override;
         bool addressIndexExists(const std::string &index) const override;

         SecureBinaryData getPublicKeyFor(const bs::Address &) override;
         KeyPair getKeyPairFor(const bs::Address &, const SecureBinaryData &password);

         bool isWatchingOnly() const { return true; }
         NetworkType networkType(void) const { return netType_; }

         void shutdown(void);
         std::string getFilename(void) const { return dbFilename_; }
      protected:
         std::shared_ptr<LMDBEnv> getDBEnv() { return dbEnv_; }
         LMDB *getDB() { return db_.get(); }

         void loadFromFile(const std::string &filename);
         void openDBEnv(const std::string &filename);
         void openDB();
         void writeDB();
         void readFromDB();
         virtual std::string getFileName(const std::string &dir) const;

         virtual std::pair<bs::Address, std::shared_ptr<GenericAsset>> deserializeAsset(BinaryDataRef ref) {
            return PlainAsset::deserialize(ref);
         }

         void putDataToDB(const std::shared_ptr<LMDB> &db, const BinaryData& key, const BinaryData& data);
         BinaryDataRef getDataRefForKey(const std::shared_ptr<LMDB> &db, const BinaryData& key) const;
         BinaryDataRef getDataRefForKey(uint32_t key) const;

      protected:
         std::map<bs::Address, std::shared_ptr<GenericAsset>>  assetByAddr_;
         std::unordered_map<int, std::shared_ptr<GenericAsset>>   assets_;
         std::atomic_int   lastAssetIndex_ = { 0 };

      private:
         int addressIndex(const bs::Address &) const;

      private:
         std::string    walletId_;
         std::string    desc_;
         std::shared_ptr<LMDBEnv>   dbEnv_;
         std::shared_ptr<LMDB>      db_;
         std::string                dbFilename_;
         const NetworkType netType_;
      };

   }  //namespace core
}  //namespace bs

#endif // BS_CORE_PLAIN_WALLET_H

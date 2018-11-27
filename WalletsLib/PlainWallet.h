#ifndef BS_PLAIN_WALLET_H
#define BS_PLAIN_WALLET_H

#include <atomic>
#include <unordered_map>
#include <QObject>
#include <BinaryData.h>

#include "BlockDataManagerConfig.h"
#include "BtcDefinitions.h"
#include "MetaData.h"


namespace spdlog {
   class logger;
};

namespace bs {
   class GenericAsset : public AssetEntry
   {
   public:
      GenericAsset(AssetEntryType type, int id = -1) :
         AssetEntry(type, id, {}), id_(id) {}

      void setId(int id) {
         id_ = id;
         ID_ = WRITE_UINT32_BE(id);
      }
      int id() const { return id_; }

   protected:
      int id_;
   };

   class PlainAsset : public GenericAsset
   {
   public:
      PlainAsset(int id, const bs::Address &addr, const SecureBinaryData &privKey = {})
         : GenericAsset(AssetEntryType_Single, id), address_(addr), privKey_(privKey) {}
      ~PlainAsset() override = default;

      static std::pair<bs::Address, std::shared_ptr<PlainAsset>> GenerateRandom(AddressEntryType);
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
      Q_OBJECT

   public:
      PlainWallet(const std::string &name, const std::string &desc
                  , const std::shared_ptr<spdlog::logger> &logger = nullptr);
      PlainWallet(const std::string &filename
                  , const std::shared_ptr<spdlog::logger> &logger = nullptr);
      PlainWallet(const std::shared_ptr<spdlog::logger> &logger = nullptr);
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

      virtual int addAddress(const bs::Address &, std::shared_ptr<GenericAsset> asset = nullptr);
      bool containsAddress(const bs::Address &addr) override;

      std::string GetWalletId() const override { return walletId_; }
      std::string GetWalletDescription() const override { return desc_; }
      void SetDescription(const std::string &desc) override { desc_ = desc; }
      wallet::Type GetType() const override { return wallet::Type::Bitcoin; }

      BinaryData getRootId() const override { return BinaryData(); }

      std::shared_ptr<ResolverFeed> GetResolver(const SecureBinaryData &) override;
      std::shared_ptr<ResolverFeed> GetPublicKeyResolver() override;

      bs::Address GetNewExtAddress(AddressEntryType) override { return {}; }
      bs::Address GetNewIntAddress(AddressEntryType) override { return {}; }

      size_t GetUsedAddressCount() const override { return usedAddresses_.size(); }
      std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
      std::string GetAddressIndex(const bs::Address &) override;
      bool AddressIndexExists(const std::string &index) const override;
      bs::Address CreateAddressWithIndex(const std::string &, AddressEntryType, bool) override { return {}; }

      SecureBinaryData GetPublicKeyFor(const bs::Address &) override;
      KeyPair GetKeyPairFor(const bs::Address &, const SecureBinaryData &password) override;

      bool EraseFile() override;

   protected:
      std::shared_ptr<LMDBEnv> getDBEnv() override { return dbEnv_; }
      LMDB *getDB() override { return db_.get(); }

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

      std::set<BinaryData> getAddrHashSet() override;
      AddressEntryType getAddrTypeForAddr(const BinaryData &) override;

   protected:
      std::map<bs::Address, std::shared_ptr<GenericAsset>>  assetByAddr_;
      std::unordered_map<int, std::shared_ptr<GenericAsset>>   assets_;
      std::atomic_int   lastAssetIndex_ = { 0 };

   private:
      int getAddressIndex(const bs::Address &) const;

   private:
      std::string    walletId_;
      std::string    desc_;
      std::shared_ptr<LMDBEnv>   dbEnv_;
      std::shared_ptr<LMDB>      db_;
      std::string                dbFilename_;
   };

}  //namespace bs

#endif // BS_PLAIN_WALLET_H

#ifndef __BS_SETTLEMENT_WALLET_H__
#define __BS_SETTLEMENT_WALLET_H__

#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>

#include <QObject>

#include <BinaryData.h>

#include "BlockDataManagerConfig.h"
#include "BtcDefinitions.h"
#include "MetaData.h"

class PyBlockDataManager;
class SafeBtcWallet;

namespace spdlog {
   class logger;
};

namespace bs {

   class SettlementAssetEntry : public AssetEntry
   {
   public:
      SettlementAssetEntry(int id, const BinaryData &settlementId, const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey)
         : AssetEntry(AssetEntryType_Multisig, id, {})
         , settlementId_(settlementId), buyAuthPubKey_(buyAuthPubKey), sellAuthPubKey_(sellAuthPubKey)
         , addrType_(AddressEntryType_P2WSH)
      {}
      ~SettlementAssetEntry() override = default;

      static std::shared_ptr<SettlementAssetEntry> deserialize(BinaryDataRef key, BinaryDataRef value);
      BinaryData serialize(void) const override;

      bool hasPrivateKey(void) const override { return false; }
      const BinaryData &getPrivateEncryptionKeyId(void) const override { return {}; }

      const BinaryData &settlementId() const { return settlementId_; }
      const BinaryData &buyAuthPubKey() const { return buyAuthPubKey_; }
      const BinaryData &buyChainedPubKey() const;
      const BinaryData &sellAuthPubKey() const { return sellAuthPubKey_; }
      const BinaryData &sellChainedPubKey() const;

      AddressEntryType addressType() const { return addrType_; }

      const BinaryData &script() const;
      void setScript(const BinaryData &script) { script_ = script; }
      const BinaryData &p2wshScript() const;

      BinaryData hash() const { return BtcUtils::hash160(script()); }
      const BinaryData &prefixedHash() const;
      const BinaryData &p2wsHash() const;
      const BinaryData &prefixedP2SHash() const;

      const std::vector<BinaryData> &supportedAddresses() const;
      const std::vector<BinaryData> &supportedAddrHashes() const;

   private:
      BinaryData  settlementId_;
      BinaryData  buyAuthPubKey_;
      BinaryData  sellAuthPubKey_;
      AddressEntryType     addrType_;
      mutable BinaryData   script_;
      mutable BinaryData   p2wshScript_;
      mutable BinaryData   p2wshScriptH160_;
      mutable BinaryData   hash_;
      mutable BinaryData   p2wsHash_;
      mutable BinaryData   prefixedP2SHash_;
      mutable BinaryData   buyChainedPubKey_;
      mutable BinaryData   sellChainedPubKey_;
      mutable std::vector<BinaryData>  supportedHashes_, supportedAddresses_;
   };


   class SettlementAddressEntry : public AddressEntry
   {
   public:
      SettlementAddressEntry(const std::shared_ptr<SettlementAssetEntry> &ae, AddressEntryType aeType = AddressEntryType_Multisig)
         : AddressEntry(aeType), ae_(ae) {}
      ~SettlementAddressEntry() noexcept override = default;

      const std::shared_ptr<SettlementAssetEntry>  getAsset() const { return ae_; }
      const BinaryData &getAddress() const override { return ae_->script(); }
      const BinaryData &getScript() const override { return ae_->script(); }
      const BinaryData &getPreimage(void) const override { return ae_->script(); }
      const BinaryData &getPrefixedHash() const override { return ae_->prefixedHash(); }
      const BinaryData &getHash() const override { return ae_->hash(); }
      shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return bs::Address(getPrefixedHash()).getRecipient(val); }
      size_t getInputSize(void) const override { return getAddress().getSize() + 2 + 73 * 1/*m*/ + 40; }

      const BinaryData& getID(void) const override { return ae_->getID(); }
      int getIndex() const { return ae_->getIndex(); }

   protected:
      std::shared_ptr<SettlementAssetEntry>  ae_;
   };

   class SettlementAddressEntry_P2SH : public SettlementAddressEntry
   {
   public:
      SettlementAddressEntry_P2SH(const std::shared_ptr<SettlementAssetEntry> &ae)
         : SettlementAddressEntry(ae, AddressEntryType_P2SH) {}

      const BinaryData &getPrefixedHash(void) const override { return ae_->prefixedP2SHash(); }
      const BinaryData &getHash() const override { return ae_->p2wsHash(); }
      shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return std::make_shared<Recipient_P2SH>(ae_->p2wsHash(), val); }
      size_t getInputSize(void) const override { return 75; }
   };

   class SettlementAddressEntry_P2WSH : public SettlementAddressEntry
   {
   public:
      SettlementAddressEntry_P2WSH(const std::shared_ptr<SettlementAssetEntry> &ae)
         : SettlementAddressEntry(ae, AddressEntryType_P2WSH) {}

      const BinaryData &getPrefixedHash(void) const override;
      const BinaryData &getHash() const override;
      const BinaryData &getAddress() const override { return BtcUtils::scrAddrToSegWitAddress(getHash()); }
      shared_ptr<ScriptRecipient> getRecipient(uint64_t val) const override { return std::make_shared<Recipient_PW2SH>(getHash(), val); }
      size_t getInputSize(void) const override { return 41; }

   private:
      mutable BinaryData   hash_;
      mutable BinaryData   prefixedHash_;
   };


   class SettlementMonitor;

   class SettlementWallet : public Wallet, public AssetWallet
   {
      Q_OBJECT

   public:
      SettlementWallet(std::shared_ptr<WalletMeta> meta, NetworkType networkType, BinaryData masterID);
      ~SettlementWallet() override;

      SettlementWallet(const SettlementWallet&) = delete;
      SettlementWallet(SettlementWallet&&) = delete;
      SettlementWallet& operator = (const SettlementWallet&) = delete;
      SettlementWallet& operator = (SettlementWallet&&) = delete;

      std::shared_ptr<bs::SettlementAddressEntry> getExistingAddress(const BinaryData &settlementId);

      std::shared_ptr<SettlementAddressEntry> newAddress(const BinaryData &settlementId
         , const BinaryData &buyAuthPubKey, const BinaryData &sellAuthPubKey, const std::string &comment = {});
      bool containsAddress(const bs::Address &addr) override;

      std::string GetWalletId() const override { return walletID_.toBinStr(); }
      std::string GetWalletDescription() const override { return "Settlement Wallet"; }
      void SetDescription(const std::string &) override {}
      bool hasWalletId(const std::string &id) const;
      bool isTempWalletId(const std::string &id) const;
      wallet::Type GetType() const override { return wallet::Type::Settlement; }

      static std::string fileNamePrefix() { return "settlement_"; }
      static bool exists(const std::string &folder, NetworkType);
      static std::shared_ptr<SettlementWallet> create(const std::string& folder, NetworkType);
      static std::shared_ptr<SettlementWallet> loadFromFolder(const std::string &folder, NetworkType netType);

      UTXO GetInputFor(const shared_ptr<SettlementAddressEntry> &addr, bool allowZC = true);
      uint64_t GetEstimatedFeeFor(UTXO input, const bs::Address &recvAddr, float feePerByte);

      bs::wallet::TXSignRequest CreatePayoutTXRequest(const UTXO &, const bs::Address &recvAddr, float feePerByte);
      UTXO GetInputFromTX(const std::shared_ptr<SettlementAddressEntry> &, const BinaryData &payinHash, const double amount) const;
      BinaryData SignPayoutTXRequest(const bs::wallet::TXSignRequest &, const KeyPair &, const BinaryData &settlementId
         , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey);

      BinaryData getRootId() const override { return BinaryData(); }   // stub
      std::vector<UTXO> getSpendableZCList() const override;

      std::shared_ptr<ResolverFeed> GetResolver(const SecureBinaryData &) override { return nullptr; }   // stub
      std::shared_ptr<ResolverFeed> GetPublicKeyResolver() override { return nullptr; }   //stub

      bs::Address GetNewExtAddress(AddressEntryType) override { return {}; }  // stub
      const SecureBinaryData& getDecryptedValue(shared_ptr<Asset_PrivateKey>) override { return {}; }

      size_t GetUsedAddressCount() const override { return addresses_.size(); }
      std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
      std::string GetAddressIndex(const bs::Address &) override;
      bool AddressIndexExists(const std::string &index) const override;
      bs::Address CreateAddressWithIndex(const std::string &index, AddressEntryType, bool signal = true) override;

      SecureBinaryData GetPublicKeyFor(const bs::Address &) override;
      KeyPair GetKeyPairFor(const bs::Address &, const SecureBinaryData &password) override;

      bool EraseFile() override;

      std::shared_ptr<SettlementMonitor> createMonitor(const shared_ptr<SettlementAddressEntry> &addr
         , const std::shared_ptr<spdlog::logger>& logger);

   protected:
      void readFromFile();
      std::shared_ptr<LMDBEnv> getDBEnv() override { return dbEnv_; }
      LMDB *getDB() override { return db_; }

      std::set<BinaryData> getAddrHashSet() override;

      void putHeaderData(const BinaryData& parentID, const BinaryData& walletID, int topUsedIndex, const std::string &name);
      shared_ptr<AddressEntry> getAddressEntryForAsset(const shared_ptr<SettlementAssetEntry> &);
      AddressEntryType getAddrTypeForAddr(const BinaryData &addr) override;
      int getAssetIndexByAddr(const BinaryData& scrAddr);

      void fillHashes(const std::shared_ptr<SettlementAssetEntry> &asset, const BinaryData &addrPrefixedHash);

      void writeAssetEntry(shared_ptr<AssetEntry> entryPtr);

      static shared_ptr<SettlementWallet> initWalletDb(shared_ptr<WalletMeta>, SecureBinaryData&& privateRoot
         , NetworkType, const std::string &name);

      static std::string mkFileName(NetworkType);

   private:
      std::shared_ptr<bs::SettlementAddressEntry> getAddressBySettlementId(const BinaryData &settlementId) const;
      void saveAddressBySettlementId(const std::shared_ptr<bs::SettlementAddressEntry>& address);

      void createTempWalletForAsset(const std::shared_ptr<SettlementAssetEntry>& asset);
      void addAsset(const std::shared_ptr<SettlementAssetEntry> &);

   private:
      mutable std::atomic_flag                           lockAddressMap_ = ATOMIC_FLAG_INIT;
      std::unordered_map<BinaryData, std::shared_ptr<bs::SettlementAddressEntry>>   addressBySettlementId_;
      std::unordered_map<int, std::shared_ptr<SettlementAssetEntry>>                assets_;
      std::map<int, std::shared_ptr<SafeBtcWallet> >     rtWallets_;
      std::unordered_map<std::string, int>               rtWalletsById_;
      std::unordered_map<BinaryData, int>                assetIndexByAddr_;
      int   lastIndex_ = 0;
   };


   struct PayoutSigner
   {
      enum Type {
         SignatureUndefined,
         SignedByBuyer,
         SignedBySeller
      };

      static const char *toString(const Type t) {
         switch (t) {
         case SignedByBuyer:        return "buyer";
         case SignedBySeller:       return "seller";
         case SignatureUndefined:
         default:                   return "undefined";
         }
      }

      static Type WhichSignature(const Tx &
         , uint64_t value
         , const std::shared_ptr<bs::SettlementAddressEntry> &
         , const std::shared_ptr<spdlog::logger>& logger);
   };

   class SettlementMonitor : public QObject
   {
      Q_OBJECT
      friend class SettlementWallet;
   public:
      SettlementMonitor(const std::shared_ptr<SafeBtcWallet> rtWallet
         , const shared_ptr<bs::SettlementAddressEntry> &addr
         , const std::shared_ptr<spdlog::logger>& logger
         , QObject *parent = nullptr);

      ~SettlementMonitor() noexcept override;

      void start();
      void stop();

      int getPayinConfirmations() const { return payinConfirmations_; }
      int getPayoutConfirmations() const { return payoutConfirmations_; }

      PayoutSigner::Type getPayoutSignerSide() const { return payoutSignedBy_; }

      int confirmedThreshold() const { return 6; }

   signals:
      // payin detected is sent on ZC and once it's get to block.
      // if payin is already on chain before monitor started, payInDetected will
      // emited only once
      void payInDetected(int confirmationsNumber, const BinaryData &txHash);

      void payOutDetected(int confirmationsNumber, PayoutSigner::Type signedBy);
      void payOutConfirmed(PayoutSigner::Type signedBy);

   protected slots:
      void checkNewEntries();

   private:
      std::atomic_bool                 stopped_;
      std::atomic_flag                 walletLock_ = ATOMIC_FLAG_INIT;
      std::shared_ptr<SafeBtcWallet>   rtWallet_;
      std::set<BinaryData>             ownAddresses_;
      int         id_;

      std::string                            addressString_;
      shared_ptr<bs::SettlementAddressEntry> addressEntry_;

      int payinConfirmations_;
      int payoutConfirmations_;

      bool payinInBlockChain_;
      bool payoutConfirmedFlag_;

      PayoutSigner::Type payoutSignedBy_ = PayoutSigner::Type::SignatureUndefined;

      std::shared_ptr<spdlog::logger> logger_;

   protected:
      bool IsPayInTransaction(const LedgerEntryData& entry) const;
      bool IsPayOutTransaction(const LedgerEntryData& entry) const;

      PayoutSigner::Type CheckPayoutSignature(const LedgerEntryData& entry) const;

      void SendPayInNotification(const int confirmationsNumber, const BinaryData &txHash);
      void SendPayOutNotification(const LedgerEntryData& entry);
   };

}  //namespace bs

#endif //__BS_SETTLEMENT_WALLET_H__

#ifndef __BS_HD_LEAF_H__
#define __BS_HD_LEAF_H__

#include <atomic>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <QThreadPool>
#include "HDNode.h"
#include "MetaData.h"
#include "PyBlockDataManager.h"


namespace bs {
   class TxAddressChecker;

   namespace hd {
      class Group;

      class BlockchainScanner
      {
      protected:
         struct AddrPoolKey {
            Path  path;
            AddressEntryType  aet;

            bool operator==(const AddrPoolKey &other) const {
               return ((path == other.path) && (aet == other.aet));
            }
         };
         struct AddrPoolHasher {
            std::size_t operator()(const AddrPoolKey &key) const {
               return (std::hash<std::string>()(key.path.toString()) ^ std::hash<int>()((int)key.aet));
            }
         };

         using PooledAddress = std::pair<BlockchainScanner::AddrPoolKey, bs::Address>;
         using cb_completed = std::function<void()>;
         using cb_save_to_wallet = std::function<void(const std::vector<PooledAddress> &)>;
         using cb_write_last = std::function<void(const std::string &walletId, unsigned int idx)>;

      protected:
         BlockchainScanner(const cb_save_to_wallet &, const cb_completed &);
         ~BlockchainScanner();

         void init(const std::shared_ptr<Node> &node, const std::string &walletId);
         void stop();

         std::vector<PooledAddress> generateAddresses(hd::Path::Elem prefix, hd::Path::Elem start
            , size_t nb, AddressEntryType aet);
         void scanAddresses(unsigned int startIdx, unsigned int portionSize = 100, const cb_write_last &cbw = nullptr);
         void onRefresh(const BinaryDataVector &ids);

      private:
         struct Portion {
            std::vector<PooledAddress>  addresses;
            Path::Elem        start;
            Path::Elem        end;
            std::atomic_bool  registered;
            Portion() : start(0), end(0), registered(false) {}
         };

         std::shared_ptr<Node>   node_;
         std::string             walletId_;
         std::string             rescanWalletId_;
         unsigned int            portionSize_ = 100;
         const cb_save_to_wallet cbSaveToWallet_;
         const cb_completed      cbCompleted_;
         cb_write_last           cbWriteLast_ = nullptr;
         Portion                 currentPortion_;
         std::atomic_int         processing_;
         std::atomic_bool        stopped_;
         QThreadPool             threadPool_;

      private:
         bs::Address newAddress(const Path &path, AddressEntryType aet);
         static std::vector<BinaryData> getRegAddresses(const std::vector<PooledAddress> &src);
         void fillPortion(Path::Elem start, unsigned int size = 100);
         void processPortion();
      };


      class Leaf : public bs::Wallet, protected BlockchainScanner
      {
         Q_OBJECT
         friend class bs::hd::Group;

      public:
         using cb_complete_notify = std::function<void(Path::Elem wallet, bool isValid)>;

         Leaf(const std::string &name, const std::string &desc
            , bs::wallet::Type type = bs::wallet::Type::Bitcoin, bool extOnlyAddresses = false);
         ~Leaf() override;
         virtual void init(const std::shared_ptr<Node> &node, const hd::Path &
            , const std::shared_ptr<Node> &rootNode);
         virtual bool copyTo(std::shared_ptr<hd::Leaf> &) const;
         virtual void setData(const std::string &) {}
         virtual void setData(uint64_t) {}

         void firstInit() override;
         std::string GetWalletId() const override;
         std::string GetWalletDescription() const override;
         void SetDescription(const std::string &desc) override { desc_ = desc; }
         std::string GetShortName() const override { return suffix_; }
         bs::wallet::Type GetType() const override { return type_; }
         bool isWatchingOnly() const override { return (rootNode_ == nullptr); }
         bool isEncrypted() const override { return (rootNode_ && rootNode_->isEncrypted()); }
         bool hasExtOnlyAddresses() const override { return isExtOnly_; }

         std::vector<UTXO> getSpendableTxOutList(uint64_t val = UINT64_MAX) const override;

         bool containsAddress(const bs::Address &addr) override;
         bool containsHiddenAddress(const bs::Address &addr) const override;
         BinaryData getRootId() const override;
         BinaryData getPubKey() const { return node_ ? node_->pubCompressedKey() : BinaryData(); }
         BinaryData getChainCode() const { return node_ ? node_->chainCode() : BinaryData(); }

         std::vector<bs::Address> GetExtAddressList() const override { return extAddresses_; }
         std::vector<bs::Address> GetIntAddressList() const override { return intAddresses_; }
         size_t GetExtAddressCount() const override { return extAddresses_.size(); }
         size_t GetIntAddressCount() const override { return intAddresses_.size(); }
         bs::Address GetNewExtAddress(AddressEntryType aet = AddressEntryType_Default) override;
         bs::Address GetNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
         bs::Address GetRandomChangeAddress(AddressEntryType aet = AddressEntryType_Default) override;
         std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) override;
         std::string GetAddressIndex(const bs::Address &) override;
         bool AddressIndexExists(const std::string &index) const override;
         bs::Address CreateAddressWithIndex(const std::string &index, AddressEntryType, bool signal = true) override;

         std::shared_ptr<ResolverFeed> GetResolver(const SecureBinaryData &password) override;
         std::shared_ptr<ResolverFeed> GetPublicKeyResolver() override;

         SecureBinaryData GetPublicKeyFor(const bs::Address &) override;
         SecureBinaryData GetPubChainedKeyFor(const bs::Address &) override;
         KeyPair GetKeyPairFor(const bs::Address &, const SecureBinaryData &password) override;

         const Path &path() const { return path_; }
         Path::Elem index() const { return static_cast<Path::Elem>(path_.get(-1)); }
         BinaryData serialize() const;
         void setDB(const std::shared_ptr<LMDBEnv> &env, LMDB *db);

         void SetBDM(const std::shared_ptr<PyBlockDataManager> &) override;

         std::shared_ptr<LMDBEnv> getDBEnv() override { return dbEnv_; }
         LMDB *getDB() override { return db_; }

         AddressEntryType getAddrTypeForAddr(const BinaryData &) override;
         std::set<BinaryData> getAddrHashSet() override;
         void addAddress(const bs::Address &, const BinaryData &pubChainedKey, const Path &path);

         void setScanCompleteCb(const cb_complete_notify &cb) { cbScanNotify_ = cb; }
         void scanAddresses(unsigned int startIdx = 0, unsigned int portionSize = 100
            , const BlockchainScanner::cb_write_last &cbw = nullptr) {
            BlockchainScanner::scanAddresses(startIdx, portionSize, cbw);
         }

      signals:
         void scanComplete(const std::string &walletId);

      protected slots:
         virtual void onZeroConfReceived(const std::vector<LedgerEntryData> &);
         virtual void onRefresh(const BinaryDataVector &ids);

      protected:
         virtual bs::Address createAddress(const Path &path, Path::Elem index, AddressEntryType aet
            , bool signal = true);
         virtual BinaryData serializeNode() const { return node_ ? node_->serialize() : BinaryData{}; }
         virtual void setRootNode(const std::shared_ptr<hd::Node> &rootNode) { rootNode_ = rootNode; }
         void stop() override;
         void reset();
         Path getPathForAddress(const bs::Address &) const;
         std::shared_ptr<Node> getNodeForAddr(const bs::Address &) const;
         std::shared_ptr<hd::Node> GetPrivNodeFor(const bs::Address &, const SecureBinaryData &password);
         void activateAddressesFromLedger(const std::vector<LedgerEntryData> &);
         void activateHiddenAddress(const bs::Address &);
         bs::Address createAddressWithPath(const hd::Path &, AddressEntryType, bool signal = true);

      protected:
         const Path::Elem  addrTypeExternal = 0u;
         const Path::Elem  addrTypeInternal = 1u;
         const AddressEntryType defaultAET_ = AddressEntryType_P2WPKH;

         bs::wallet::Type        type_;
         std::shared_ptr<Node>   node_;
         std::shared_ptr<Node>   rootNode_;
         hd::Path                path_;
         bool        isExtOnly_ = false;
         std::string name_, desc_;
         std::string suffix_;
         Path::Elem  lastIntIdx_ = 0;
         Path::Elem  lastExtIdx_ = 0;

         std::unordered_map<BinaryData, BinaryData> hashToPubKey_;
         std::unordered_map<BinaryData, hd::Path>   pubKeyToPath_;
         using TempAddress = std::pair<Path, AddressEntryType>;
         std::unordered_map<Path::Elem, TempAddress>  tempAddresses_;

      private:
         shared_ptr<LMDBEnv> dbEnv_ = nullptr;
         LMDB* db_ = nullptr;
         using AddressTuple = std::tuple<bs::Address, std::shared_ptr<Node>, Path>;
         std::unordered_map<Path::Elem, AddressTuple> addressMap_;
         std::vector<bs::Address>                     intAddresses_;
         std::vector<bs::Address>                     extAddresses_;
         std::unordered_map<BinaryData, Path::Elem>   addrToIndex_;
         cb_complete_notify                           cbScanNotify_ = nullptr;
         std::unordered_map<AddrPoolKey, bs::Address, AddrPoolHasher>   addressPool_;
         std::map<bs::Address, AddrPoolKey>           poolByAddr_;
         const size_t addressPoolSize_ = 100;
         volatile bool activateAddressesInvoked_ = false;

      private:
         bs::Address createAddress(AddressEntryType aet, bool isInternal = false);
         std::shared_ptr<AddressEntry> getAddressEntryForAsset(std::shared_ptr<AssetEntry> assetPtr
            , AddressEntryType ae_type = AddressEntryType_Default);
         Path::Elem getAddressIndexForAddr(const BinaryData &addr) const;
         Path::Elem getAddressIndex(const bs::Address &addr) const;
         void onScanComplete();
         void onSaveToWallet(const std::vector<PooledAddress> &);
         void topUpAddressPool(size_t intAddresses = 0, size_t extAddresses = 0);
         Path::Elem getLastAddrPoolIndex(Path::Elem) const;

         static void serializeAddr(BinaryWriter &bw, Path::Elem index, AddressEntryType, const Path &);
         bool deserialize(const BinaryData &ser, const std::shared_ptr<hd::Node> &rootNode);
      };


      class AuthLeaf : public Leaf
      {
      public:
         AuthLeaf(const std::string &name, const std::string &desc)
            : Leaf(name, desc, bs::wallet::Type::Authentication) {}
         void init(const std::shared_ptr<Node> &node, const hd::Path &
            , const std::shared_ptr<Node> &rootNode) override;
         void SetUserID(const BinaryData &) override;

      protected:
         bs::Address createAddress(const Path &path, Path::Elem index, AddressEntryType aet
            , bool signal = true) override;
         BinaryData serializeNode() const override {
            return unchainedNode_ ? unchainedNode_->serialize() : BinaryData{};
         }
         void setRootNode(const std::shared_ptr<hd::Node> &rootNode) override;

      private:
         std::shared_ptr<Node>   unchainedNode_;
         std::shared_ptr<Node>   unchainedRootNode_;
         BinaryData              userId_;
      };


      class CCLeaf : public Leaf
      {
         Q_OBJECT

      public:
         CCLeaf(const std::string &name, const std::string &desc, bool extOnlyAddresses = false);
         ~CCLeaf() override;

         wallet::Type GetType() const override { return wallet::Type::ColorCoin; }

         void setData(const std::string &) override;
         void setData(uint64_t data) override { lotSizeInSatoshis_ = data; }
         void firstInit() override;

         std::vector<UTXO> getSpendableTxOutList(uint64_t val = UINT64_MAX) const override;
         std::vector<UTXO> getSpendableZCList() const override;
         bool isBalanceAvailable() const override;
         void UpdateBalanceFromDB() override;
         BTCNumericTypes::balance_type GetSpendableBalance() const override;
         BTCNumericTypes::balance_type GetUnconfirmedBalance() const override;
         BTCNumericTypes::balance_type GetTotalBalance() const override;
         std::vector<uint64_t> getAddrBalance(const bs::Address &) const override;

         BTCNumericTypes::balance_type GetTxBalance(int64_t) const override;
         QString displayTxValue(int64_t val) const override;
         QString displaySymbol() const override;
         bool isTxValid(const BinaryData &) const override;

      private slots:
         void onZeroConfReceived(const std::vector<LedgerEntryData> &) override;
         void onRefresh(const BinaryDataVector &ids) override;

      private:
         void validationProc();
         void findInvalidUTXOs(const std::vector<UTXO> &);
         BTCNumericTypes::balance_type correctBalance(BTCNumericTypes::balance_type
            , bool applyCorrection = true) const;
         std::vector<UTXO> filterUTXOs(const std::vector<UTXO> &) const;

      private:
         std::shared_ptr<TxAddressChecker>   checker_;
         uint64_t       lotSizeInSatoshis_ = 0;
         volatile bool  validationStarted_, validationEnded_;
         double         balanceCorrection_ = 0;
         std::set<UTXO> invalidTx_;
         std::unordered_set<BinaryData> invalidTxHash_;
         QThreadPool    threadPool_;
      };

   }  //namespace hd
}  //namespace bs

#endif //__BS_HD_LEAF_H__

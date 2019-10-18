#ifndef BS_SYNC_HD_LEAF_H
#define BS_SYNC_HD_LEAF_H

#include <atomic>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ArmoryConnection.h"
#include "ColoredCoinLogic.h"
#include "HDPath.h"
#include "SyncWallet.h"

namespace spdlog {
   class logger;
}

namespace bs {
   class TxAddressChecker;

   namespace sync {
      namespace hd {
         class Leaf : public bs::sync::Wallet
         {
         protected:
            Leaf(const std::string &walletId, const std::string &name, const std::string &desc
               , WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &
               , bs::core::wallet::Type type
               , bool extOnlyAddresses);

         public:
            using cb_complete_notify = std::function<void(bs::hd::Path::Elem wallet, bool isValid)>;
            ~Leaf() override;

            virtual void setPath(const bs::hd::Path &);
            void synchronize(const std::function<void()> &cbDone) override;

            void init(bool force = false) override;

            const std::string& walletId() const override;
            const std::string& walletIdInt() const override;

            std::string description() const override;
            void setDescription(const std::string &desc) override { desc_ = desc; }
            std::string shortName() const override;
            bs::core::wallet::Type type() const override { return type_; }
            std::vector<bs::wallet::EncryptionType> encryptionTypes() const override { return encryptionTypes_; }
            std::vector<BinaryData> encryptionKeys() const override { return encryptionKeys_; }
            bs::wallet::KeyRank encryptionRank() const override { return encryptionRank_; }
            bool hasExtOnlyAddresses() const override { return isExtOnly_; }
            bool hasId(const std::string &) const override;

            bool getSpendableTxOutList(const ArmoryConnection::UTXOsCb &, uint64_t val) override;
            BTCNumericTypes::balance_type getSpendableBalance() const override;
            bool getHistoryPage(uint32_t id, std::function<void(const bs::sync::Wallet *wallet
               , std::vector<ClientClasses::LedgerEntry>)>, bool onlyNew = false) const;

            bool containsAddress(const bs::Address &addr) override;
            bool containsHiddenAddress(const bs::Address &addr) const override;

            std::vector<bs::Address> getExtAddressList() const override { return extAddresses_; }
            std::vector<bs::Address> getIntAddressList() const override { return intAddresses_; }

            size_t getExtAddressCount() const override { return extAddresses_.size(); }
            size_t getIntAddressCount() const override { return intAddresses_.size(); }
            size_t getAddressPoolSize() const;

            bool isExternalAddress(const Address &) const override;
            void getNewExtAddress(const CbAddress &) override;
            void getNewIntAddress(const CbAddress &) override;
            void getNewChangeAddress(const CbAddress &) override;
            std::string getAddressIndex(const bs::Address &) override;
            bool getLedgerDelegateForAddress(const bs::Address &
               , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &) override;

            int addAddress(const bs::Address &, const std::string &index, bool sync = true) override;

            const bs::hd::Path &path() const { return path_; }
            bs::hd::Path::Elem index() const { return static_cast<bs::hd::Path::Elem>(path_.get(-1)); }

            std::vector<std::string> registerWallet(const std::shared_ptr<ArmoryConnection> &armory = nullptr
               , bool asNew = false) override;

            std::vector<BinaryData> getAddrHashes() const override;
            std::vector<BinaryData> getAddrHashesExt() const;
            std::vector<BinaryData> getAddrHashesInt() const;

            virtual void merge(const std::shared_ptr<Wallet>) override;
            void scan(const std::function<void(bs::sync::SyncState)> &cb) override;

            virtual std::vector<std::string> setUnconfirmedTarget(void);

            std::shared_ptr<ResolverFeed> getPublicResolver() const override;

         protected:
            struct AddrPoolKey {
               bs::hd::Path  path;

               bool empty() const { return (path.length() == 0); }
               bool operator < (const AddrPoolKey &other) const {
                  return (path < other.path);
               }
               bool operator==(const AddrPoolKey &other) const {
                  return (path == other.path);
               }
            };

            using PooledAddress = std::pair<AddrPoolKey, bs::Address>;

         protected:
            void onRefresh(const std::vector<BinaryData> &ids, bool online) override;
            virtual void createAddress(const CbAddress &cb, const AddrPoolKey &);
            void reset();
            bs::hd::Path getPathForAddress(const bs::Address &) const;
            virtual void topUpAddressPool(bool extInt, const std::function<void()> &cb = nullptr);
            void postOnline();
            bool isOwnId(const std::string &) const override;

            virtual void onRegistrationCompleted() {};

         protected:
            const bs::hd::Path::Elem   addrTypeExternal = 0u;
            const bs::hd::Path::Elem   addrTypeInternal = 1u;

            mutable std::string     walletId_, walletIdInt_;
            bs::core::wallet::Type  type_;
            bs::hd::Path            path_;
            std::string name_, desc_;
            std::string suffix_;
            bool  isExtOnly_ = false;
            std::vector<bs::wallet::EncryptionType>   encryptionTypes_;
            std::vector<BinaryData> encryptionKeys_;
            bs::wallet::KeyRank     encryptionRank_{1, 1};

            std::shared_ptr<AsyncClient::BtcWallet>   btcWallet_;
            std::shared_ptr<AsyncClient::BtcWallet>   btcWalletInt_;

            bs::hd::Path::Elem  lastIntIdx_ = 0;
            bs::hd::Path::Elem  lastExtIdx_ = 0;

            size_t intAddressPoolSize_ = 100;
            size_t extAddressPoolSize_ = 100;

            mutable std::atomic_flag            addressPoolLock_ = ATOMIC_FLAG_INIT;
            std::map<AddrPoolKey, bs::Address>  addressPool_;
            std::map<bs::Address, AddrPoolKey>  poolByAddr_;

         private:
            std::vector<bs::Address>                     intAddresses_;
            std::vector<bs::Address>                     extAddresses_;
            std::map<BinaryData, AddrPoolKey>            addrToIndex_;
            cb_complete_notify                           cbScanNotify_ = nullptr;
            std::function<void(const std::string &walletId, unsigned int idx)> cbWriteLast_ = nullptr;
            BTCNumericTypes::balance_type spendableBalanceCorrection_ = 0;

            struct AddrPrefixedHashes {
               std::set<BinaryData> external;
               std::set<BinaryData> internal;

               void clear() {
                  external.clear();
                  internal.clear();
               }
            };
            mutable AddrPrefixedHashes addrPrefixedHashes_;

            std::string regIdExt_, regIdInt_;
            std::mutex  regMutex_;
            std::vector<std::string> unconfTgtRegIds_;

            std::unordered_map<std::string, std::function<void(bs::sync::SyncState)>>  cbScanMap_;
            std::shared_ptr<AsyncClient::BtcWallet>   scanWallet_;
            std::string scanRegId_;
            bool  scanExt_ = true;
            std::set<BinaryData> activeScannedAddresses_;

         private:
            void createAddress(const CbAddress &, bool isInternal = false);
            AddrPoolKey getAddressIndexForAddr(const BinaryData &addr) const;
            AddrPoolKey addressIndex(const bs::Address &) const;
            void resumeScan(const std::string &refreshId);

            static std::vector<BinaryData> getRegAddresses(const std::vector<PooledAddress> &src);
         };


         class XBTLeaf : public Leaf
         {
         public:
            XBTLeaf(const std::string &walletId, const std::string &name, const std::string &desc
               , WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &, bool extOnlyAddresses);
            ~XBTLeaf() override = default;
         };


         class AuthLeaf : public Leaf
         {
         public:
            AuthLeaf(const std::string &walletId, const std::string &name, const std::string &desc
               , WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &);
         };


         class CCLeaf : public Leaf
         {
         public:
            CCLeaf(const std::string &walletId, const std::string &name, const std::string &desc
               , WalletSignerContainer *,const std::shared_ptr<spdlog::logger> &);
            ~CCLeaf() override;

            bs::core::wallet::Type type() const override { return bs::core::wallet::Type::ColorCoin; }
            std::string shortName() const override { return suffix_; }

            void setCCDataResolver(const std::shared_ptr<CCDataResolver> &resolver);
            void init(bool force) override;
            void setPath(const bs::hd::Path &) override;

            bool getSpendableTxOutList(const ArmoryConnection::UTXOsCb &, uint64_t val) override;
            bool getSpendableZCList(const ArmoryConnection::UTXOsCb &) const override;
            bool isBalanceAvailable() const override;
            BTCNumericTypes::balance_type getSpendableBalance() const override;
            BTCNumericTypes::balance_type getUnconfirmedBalance() const override;
            BTCNumericTypes::balance_type getTotalBalance() const override;
            std::vector<uint64_t> getAddrBalance(const bs::Address &addr) const override;

            BTCNumericTypes::balance_type getTxBalance(int64_t) const override;
            QString displayTxValue(int64_t val) const override;
            QString displaySymbol() const override;
            bool isTxValid(const BinaryData &) const override;

            void setArmory(const std::shared_ptr<ArmoryConnection> &) override;

            std::vector<std::string> setUnconfirmedTarget(void) override;

         protected:
            void onTrackerUpdated();

            void onZeroConfReceived(const std::vector<bs::TXEntry> &zcs) override;
            void onNewBlock(unsigned int, unsigned int) override;
            void onRefresh(const std::vector<BinaryData> &, bool) override;

         private:
            void validationProc();
            void findInvalidUTXOs(const std::vector<UTXO> &
               , const ArmoryConnection::UTXOsCb &);
            void refreshInvalidUTXOs(const bool& ZConly = false);
            BTCNumericTypes::balance_type correctBalance(BTCNumericTypes::balance_type
               , bool applyCorrection = true) const;
            std::vector<UTXO> filterUTXOs(const std::vector<UTXO> &) const;

            class CCWalletACT : public WalletACT
            {
            public:
               CCWalletACT(Wallet *leaf) : WalletACT(leaf) {}
               void onStateChanged(ArmoryState) override;
            };

            std::shared_ptr<TxAddressChecker>   checker_;      //TODO: remove
            std::unique_ptr<ColoredCoinTracker> tracker_;
            std::shared_ptr<CCDataResolver>     ccResolver_;
            std::atomic_bool validationStarted_{false};        //TODO: remove
            std::atomic_bool validationEnded_{false};          //TODO: remove
            double         balanceCorrection_ = 0;
            std::set<BinaryData> validTxHash_;
            std::map<std::string, std::function<void()>> refreshCb_;
         };


         class SettlementLeaf : public Leaf
         {
         public:
            SettlementLeaf(const std::string &walletId, const std::string &name,
               const std::string &desc, WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &);

            void getRootPubkey(const std::function<void(const SecureBinaryData &)> &) const;
            void setSettlementID(const SecureBinaryData &, const std::function<void(bool)> &);

            std::vector<std::string> registerWallet(const std::shared_ptr<ArmoryConnection> &armory = nullptr
               , bool asNew = false) override
            {
               if (wct_) {
                  wct_->walletReady(walletId());
               }
               return {};
            }

         protected:
            void createAddress(const CbAddress &, const AddrPoolKey &) override;
            void topUpAddressPool(bool extInt, const std::function<void()> &cb = nullptr) override;
         };

      }  //namespace hd
   }  //namespace sync
}  //namespace bs

#endif //BS_SYNC_HD_LEAF_H

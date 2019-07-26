#ifndef BS_SYNC_WALLET_H
#define BS_SYNC_WALLET_H

#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <QString>
#include "Address.h"
#include "ArmoryConnection.h"
#include "AsyncClient.h"
#include "Assets.h"
#include "BtcDefinitions.h"
#include "ClientClasses.h"
#include "CoreWallet.h"
#include "LedgerEntry.h"
#include "UtxoReservation.h"
#include "WalletEncryption.h"

namespace spdlog {
   class logger;
}
class WalletSignerContainer;

namespace bs {
   namespace sync {
      class CCDataResolver
      {
      public:
         virtual ~CCDataResolver() = default;
         virtual std::string nameByWalletIndex(bs::hd::Path::Elem) const = 0;
         virtual uint64_t lotSizeFor(const std::string &cc) const = 0;
         virtual bs::Address genesisAddrFor(const std::string &cc) const = 0;
         virtual std::string descriptionFor(const std::string &cc) const = 0;
         virtual std::vector<std::string> securities() const = 0;
      };

      namespace wallet {

         constexpr uint64_t kMinRelayFee = 1000;

         struct Comment
         {
            enum Type {
               ChangeAddress,
               AuthAddress,
               SettlementPayOut
            };
            static const char *toString(Type t)
            {
               switch (t)
               {
                  case ChangeAddress:     return "--== Change Address ==--";
                  case AuthAddress:       return "--== Auth Address ==--";
                  case SettlementPayOut:  return "--== Settlement Pay-Out ==--";
               }
               return "";
            }
         };

         bs::core::wallet::TXSignRequest createTXRequest(const std::string &walletId
            , const std::vector<UTXO> &inputs
            , const std::vector<std::shared_ptr<ScriptRecipient>> &
            , const std::function<bs::Address(std::string &index)> &cbChangeAddr = nullptr
            , const uint64_t fee = 0, bool isRBF = false, const uint64_t& origFee = 0);

      }  // namepsace wallet

      class WalletACT;
      class WalletCallbackTarget;

      class Wallet
      {
         friend class WalletACT;

      public:
         Wallet(WalletSignerContainer *, const std::shared_ptr<spdlog::logger> &logger = nullptr);
         virtual ~Wallet();

         using CbAddress = std::function<void(const bs::Address &)>;
         using CbAddresses = std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)>;

         virtual void synchronize(const std::function<void()> &cbDone);

         virtual const std::string& walletId(void) const = 0;
         virtual const std::string& walletIdInt(void) const;

         virtual std::string name() const { return walletName_; }
         virtual std::string shortName() const { return name(); }
         virtual std::string description() const = 0;
         virtual void setDescription(const std::string &) = 0;
         virtual core::wallet::Type type() const { return core::wallet::Type::Bitcoin; }
         NetworkType networkType() const { return netType_; }
         virtual bool hasId(const std::string &id) const { return (walletId() == id); }

         virtual void setArmory(const std::shared_ptr<ArmoryConnection> &);
         virtual void setUserId(const BinaryData &) {}

         bool operator ==(const Wallet &w) const { return (w.walletId() == walletId()); }
         bool operator !=(const Wallet &w) const { return (w.walletId() != walletId()); }

         virtual bool containsAddress(const bs::Address &addr) = 0;
         virtual bool containsHiddenAddress(const bs::Address &) const { return false; }

         virtual std::vector<std::string> registerWallet(
            const std::shared_ptr<ArmoryConnection> &armory = nullptr, bool asNew = false);
         virtual void unregisterWallet();

         virtual bool isBalanceAvailable() const;
         virtual BTCNumericTypes::balance_type getSpendableBalance() const;
         virtual BTCNumericTypes::balance_type getUnconfirmedBalance() const;
         virtual BTCNumericTypes::balance_type getTotalBalance() const;
         virtual void init(bool force = false);

         virtual std::vector<uint64_t> getAddrBalance(const bs::Address &addr) const;
         virtual uint64_t getAddrTxN(const bs::Address &addr) const;

         virtual bool isWatchingOnly() const { return false; }
         virtual std::vector<bs::wallet::EncryptionType> encryptionTypes() const { return {}; }
         virtual std::vector<SecureBinaryData> encryptionKeys() const { return {}; }
         virtual std::pair<unsigned int, unsigned int> encryptionRank() const { return { 0, 0 }; }
         virtual bool hasExtOnlyAddresses() const { return false; }
         virtual std::string getAddressComment(const bs::Address& address) const;
         virtual bool setAddressComment(const bs::Address &addr, const std::string &comment, bool sync = true);
         virtual std::string getTransactionComment(const BinaryData &txHash);
         virtual bool setTransactionComment(const BinaryData &txOrHash, const std::string &comment, bool sync = true);

         virtual std::vector<bs::Address> getUsedAddressList() const { return usedAddresses_; }
         virtual std::vector<bs::Address> getExtAddressList() const { return usedAddresses_; }
         virtual std::vector<bs::Address> getIntAddressList() const { return usedAddresses_; }
         virtual bool isExternalAddress(const Address &) const { return true; }
         virtual size_t getUsedAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getExtAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getIntAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getWalletAddressCount() const { return addrCount_; }

         virtual void getNewExtAddress(const CbAddress &, AddressEntryType aet = AddressEntryType_Default) = 0;
         virtual void getNewIntAddress(const CbAddress &, AddressEntryType aet = AddressEntryType_Default) = 0;
         virtual void getNewChangeAddress(const CbAddress &cb, AddressEntryType aet = AddressEntryType_Default)
            { getNewExtAddress(cb, aet); }

         virtual std::string getAddressIndex(const bs::Address &) = 0;
         virtual bool addressIndexExists(const std::string &index) const = 0;

         //Adds an arbitrary address identified by index
         virtual int addAddress(const bs::Address &, const std::string &index, AddressEntryType, bool sync = true);

         void syncAddresses();

         virtual bool getLedgerDelegateForAddress(const bs::Address &
            , const std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)> &);

         virtual BTCNumericTypes::balance_type getTxBalance(int64_t val) const { return val / BTCNumericTypes::BalanceDivider; }
         virtual QString displayTxValue(int64_t val) const;
         virtual QString displaySymbol() const { return QLatin1String("XBT"); }
         virtual bool isTxValid(const BinaryData &) const { return true; }

         virtual core::wallet::TXSignRequest createTXRequest(const std::vector<UTXO> &
            , const std::vector<std::shared_ptr<ScriptRecipient>> &
            , const uint64_t fee = 0, bool isRBF = false
            , bs::Address changeAddress = {}, const uint64_t& origFee = 0);
         virtual core::wallet::TXSignRequest createPartialTXRequest(uint64_t spendVal
            , const std::vector<UTXO> &inputs = {}, bs::Address changeAddress = {}
            , float feePerByte = 0
            , const std::vector<std::shared_ptr<ScriptRecipient>> &recipients = {}
            , const BinaryData prevPart = {});

         virtual bool deleteRemotely() { return false; } //stub
         virtual void merge(const std::shared_ptr<Wallet>) = 0;

         void newAddresses(const std::vector<std::pair<std::string, AddressEntryType>> &inData,
            const CbAddresses &cb);
         void trackChainAddressUse(std::function<void(bs::sync::SyncState)> cb);
         size_t getActiveAddressCount(void);

         /***
         Baseline db fetch methods using combined get logic. These come 
         with the default implementation but remain virtual to leave 
         room for custom behavior. 
         ***/

         //balance and count
         virtual bool updateBalances(const std::function<void(void)> & = nullptr);
         virtual bool getAddressTxnCounts(std::function<void(void)> cb = nullptr);
         
         //utxos
         virtual bool getSpendableTxOutList(const ArmoryConnection::UTXOsCb &, uint64_t val);
         virtual bool getSpendableZCList(const ArmoryConnection::UTXOsCb &) const;
         virtual bool getRBFTxOutList(const ArmoryConnection::UTXOsCb &) const;

         //custom ACT
         template<class U> void setCustomACT(
            const std::shared_ptr<ArmoryConnection> &armory)
         {
            act_ = std::make_unique<U>(armory.get(), this);
         }

         void setWCT(WalletCallbackTarget *wct) { wct_ = wct; }

      protected:
         virtual void onZeroConfReceived(const std::vector<bs::TXEntry>&);
         virtual void onNewBlock(unsigned int);
         virtual void onRefresh(const std::vector<BinaryData> &ids, bool online);

         virtual std::vector<BinaryData> getAddrHashes() const = 0;

         virtual bool isOwnId(const std::string &wId) const { return (wId == walletId()); }

         template <typename MapT> void updateMap(const MapT &src, MapT &dst) const 
         {
            std::unique_lock<std::mutex> lock(addrMapsMtx_);
            for (const auto &elem : src) {     // std::map::insert doesn't replace elements
               dst[elem.first] = std::move(elem.second);
            }
         }

         bool getHistoryPage(const std::shared_ptr<AsyncClient::BtcWallet> &
            , uint32_t id, std::function<void(const Wallet *wallet
               , std::vector<ClientClasses::LedgerEntry>)>, bool onlyNew = false) const;

      public:
         bool isRegistered(void) const { return isRegistered_; }

      protected:
         std::string                walletName_;
         WalletSignerContainer*     signContainer_;
         BTCNumericTypes::balance_type spendableBalance_ = 0;
         BTCNumericTypes::balance_type unconfirmedBalance_ = 0;
         BTCNumericTypes::balance_type totalBalance_ = 0;
         std::shared_ptr<ArmoryConnection>   armory_;
         std::shared_ptr<spdlog::logger>     logger_; // May need to be set manually.
         mutable std::vector<bs::Address>    usedAddresses_;
         NetworkType netType_ = NetworkType::Invalid;
         mutable std::mutex addrMapsMtx_;
         size_t addrCount_ = 0;

         std::map<bs::Address, std::string>  addrComments_;
         std::map<BinaryData, std::string>   txComments_;

         mutable std::map<BinaryData, std::vector<uint64_t> >  addressBalanceMap_;
         mutable std::map<BinaryData, uint64_t>                addressTxNMap_;

         class UtxoFilterAdapter : public bs::UtxoReservation::Adapter
         {
         public:
            UtxoFilterAdapter(const std::string &walletId) : walletId_(walletId) {}
            void filter(std::vector<UTXO> &utxos) { parent_->filter(walletId_, utxos); }
         private:
            const std::string walletId_;
         };
         std::shared_ptr<UtxoFilterAdapter>  utxoAdapter_;

         std::unique_ptr<WalletACT>   act_;
         WalletCallbackTarget       * wct_ = nullptr;

      private:
         std::string regId_;
         mutable std::map<uint32_t, std::vector<ClientClasses::LedgerEntry>>  historyCache_;
         std::vector<std::function<void(void)>> cbTxNs_;
         std::vector<std::function<void(void)>> cbBalances_;

      protected:
         bool firstInit_ = false;
         bool isRegistered_ = false;

         mutable std::mutex   cbMutex_;
         std::map<bs::Address, std::function<void(const std::shared_ptr<AsyncClient::LedgerDelegate> &)>>   cbLedgerByAddr_;
      };

      class WalletACT : public ArmoryCallbackTarget
      {
      public:
         WalletACT(ArmoryConnection *armory, Wallet *leaf)
            : ArmoryCallbackTarget(armory), parent_(leaf) {}
         virtual void onRefresh(const std::vector<BinaryData> &ids, bool online) override {
            parent_->onRefresh(ids, online);
         }
         virtual void onZCReceived(const std::vector<bs::TXEntry> &zcs) override {
            parent_->onZeroConfReceived(zcs);
         }
         virtual void onNewBlock(unsigned int block) override {
            parent_->onNewBlock(block);
         }
         void onCombinedBalances(const std::map<std::string, CombinedBalances> &) override;
         void onCombinedTxnCounts(const std::map<std::string, CombinedCounts> &) override;
         void onLedgerForAddress(const bs::Address &, const std::shared_ptr<AsyncClient::LedgerDelegate> &) override;
      protected:
         Wallet *parent_;
      };

      class WalletCallbackTarget
      {
      public:  // all virtual methods have empty implementation by default
         virtual ~WalletCallbackTarget() = default;

         virtual void addressAdded(const std::string &walletId) {}
         virtual void walletReady(const std::string &walletId) {}
         virtual void balanceUpdated(const std::string &walletId) {}
         virtual void metadataChanged(const std::string &walletId) {}
         virtual void walletCreated(const std::string &walletId) {}
         virtual void walletDestroyed(const std::string &walletId) {}
         virtual void walletReset(const std::string &walletId) {}
         virtual void scanComplete(const std::string &walletId) {}
      };

      struct Transaction
      {
         enum Direction {
            Unknown,
            Received,
            Sent,
            Internal,
            Auth,
            PayIn,
            PayOut,
            Revoke,
            Delivery,
            Payment
         };

         static const char *toString(Direction dir) {
            switch (dir)
            {
            case Received:    return QT_TR_NOOP("Received");
            case Sent:        return QT_TR_NOOP("Sent");
            case Internal:    return QT_TR_NOOP("Internal");
            case Auth:        return QT_TR_NOOP("AUTHENTICATION");
            case PayIn:       return QT_TR_NOOP("PAY-IN");
            case PayOut:      return QT_TR_NOOP("PAY-OUT");
            case Revoke:      return QT_TR_NOOP("REVOKE");
            case Delivery:    return QT_TR_NOOP("Delivery");
            case Payment:     return QT_TR_NOOP("Payment");
            case Unknown:     return QT_TR_NOOP("Undefined");
            }
            return QT_TR_NOOP("Undefined");
         }
         static const char *toStringDir(Direction dir) {
            switch (dir)
            {
            case Received: return QT_TR_NOOP("Received with");
            case Sent:     return QT_TR_NOOP("Sent to");
            default:       return toString(dir);
            }
         }
      };

   }  //namespace sync
}  //namespace bs

#endif //BS_SYNC_WALLET_H

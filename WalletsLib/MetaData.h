#ifndef __BS_WALLET_METADATA_H__
#define __BS_WALLET_METADATA_H__

#include <string>
#include <vector>
#include <unordered_map>
#include <QObject>
#include <QMutex>
#include <QThreadPool>
#include "Address.h"
#include "BtcDefinitions.h"
#include "EasyCoDec.h"
#include "lmdbpp.h"
#include "Script.h"
#include "Signer.h"
#include "UtxoReservation.h"


class PyBlockDataManager;
class SafeBtcWallet;


#define WALLETTYPE_KEY          0x00000001
#define ROOTASSET_KEY           0x00000007
#define WALLETNAME_KEY          0x00000020
#define WALLETDESCRIPTION_KEY   0x00000021
#define MASTERID_KEY            0x000000A0
#define MAINWALLET_KEY          0x000000A1
#define WALLETMETA_PREFIX       0xB0
#define WALLETMETA_DBNAME       "WalletHeader"


namespace bs {
   class Wallet;

   namespace wallet {
      class AssetEntryMeta : public AssetEntry
      {
      public:
         enum Type {
            Unknown = 0,
            Comment = 4
         };
         AssetEntryMeta(Type type, int id) : AssetEntry(AssetEntryType_Single, id, {}), type_(type) {}
         virtual ~AssetEntryMeta() = default;

         Type type() const { return type_; }
         virtual BinaryData key() const {
            BinaryWriter bw;
            bw.put_uint8_t(ASSETENTRY_PREFIX);
            bw.put_int32_t(index_);
            return bw.getData();
         }
         static std::shared_ptr<AssetEntryMeta> deserialize(int index, BinaryDataRef value);
         virtual bool deserialize(BinaryRefReader brr) = 0;

         bool hasPrivateKey(void) const override { return false; }
         const BinaryData& getPrivateEncryptionKeyId(void) const override { return {}; }


      private:
         Type  type_;
      };

      class AssetEntryComment : public AssetEntryMeta
      {
         BinaryData  key_;
         std::string comment_;
      public:
         AssetEntryComment(int id, const BinaryData &key, const std::string &comment)
            : AssetEntryMeta(AssetEntryMeta::Comment, id), key_(key), comment_(comment) {}
         AssetEntryComment() : AssetEntryMeta(AssetEntryMeta::Comment, 0) {}

         BinaryData key() const override { return key_; }
         const std::string &comment() const { return comment_; }
         BinaryData serialize() const override;
         bool deserialize(BinaryRefReader brr) override;
      };

      class MetaData
      {
         std::unordered_map<BinaryData, shared_ptr<AssetEntryMeta>>   data_;

      protected:
         unsigned int      nbMetaData_;

         MetaData() : nbMetaData_(0) {}

         shared_ptr<AssetEntryMeta> get(const BinaryData &key) const {
            const auto itData = data_.find(key);
            if (itData != data_.end()) {
               return itData->second;
            }
            return nullptr;
         }
         void set(const shared_ptr<AssetEntryMeta> &value);
         void write(const std::shared_ptr<LMDBEnv> env, LMDB *db);
         void readFromDB(const std::shared_ptr<LMDBEnv> env, LMDB *db);
      };

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
               default:                return "";
            }
         }
      };

      struct TXSignRequest
      {
         std::string       walletId;
         bs::Wallet     *  wallet = nullptr;
         std::vector<UTXO> inputs;
         std::vector<std::shared_ptr<ScriptRecipient>>   recipients;
         struct {
            bs::Address address;
            std::string index;
            uint64_t    value = 0;
         }  change;
         uint64_t    fee = 0;
         bool        RBF = false;
         std::vector<BinaryData>       prevStates;
         std::shared_ptr<ResolverFeed> resolver;
         bool        populateUTXOs = false;
         std::string comment;

         bool isValid() const noexcept;
         BinaryData serializeState() const { return getSigner().serializeState(); }
         BinaryData txId() const { return getSigner().getTxId(); }
         size_t estimateTxSize() const;

      private:
         Signer   getSigner() const;
      };


      struct TXMultiSignRequest
      {
         std::map<UTXO, std::shared_ptr<bs::Wallet>>     inputs;     // per-wallet UTXOs
         std::vector<std::shared_ptr<ScriptRecipient>>   recipients;
         std::vector<std::shared_ptr<bs::Wallet>>        wallets;
         BinaryData  prevState;

         bool isValid() const noexcept;
         size_t estimateTxSize() const;
         void addInput(const UTXO &utxo, const std::shared_ptr<bs::Wallet> &wallet) { inputs[utxo] = wallet; }
      };


      class Seed
      {
      public:
         Seed(NetworkType netType) : netType_(netType) {}
         Seed(const std::string &seed, NetworkType netType);
         Seed(NetworkType netType, const SecureBinaryData &privKey, const BinaryData &chainCode = {})
            : privKey_(privKey), chainCode_(chainCode), netType_(netType) {}

         bool empty() const { return (privKey_.isNull() && seed_.isNull()); }
         bool hasPrivateKey() const { return (!privKey_.isNull()); }
         const SecureBinaryData &privateKey() const { return privKey_; }
         const BinaryData &chainCode() const { return chainCode_; }
         const BinaryData &seed() const { return seed_; }
         NetworkType networkType() const { return netType_; }
         void setNetworkType(NetworkType netType) { netType_ = netType; }

         EasyCoDec::Data toEasyCodeChecksum(size_t ckSumSize = 2) const;
         static SecureBinaryData decodeEasyCodeChecksum(const EasyCoDec::Data &, size_t ckSumSize = 2);
         static BinaryData decodeEasyCodeLineChecksum(const std::string&easyCodeHalf, size_t ckSumSize = 2, size_t keyValueSize = 16);
         static Seed fromEasyCodeChecksum(const EasyCoDec::Data &, NetworkType, size_t ckSumSize = 2);
         static Seed fromEasyCodeChecksum(const EasyCoDec::Data &privKey, const EasyCoDec::Data &chainCode
            , NetworkType, size_t ckSumSize = 2);

      private:
         SecureBinaryData  privKey_;
         BinaryData        chainCode_;
         BinaryData        seed_;
         NetworkType       netType_ = NetworkType::Invalid;
      };

      enum class Type {
         Unknown,
         Bitcoin,
         ColorCoin,
         Authentication,
         Settlement
      };

      size_t getInputScrSize(const std::shared_ptr<AddressEntry> &);
   }  // namepsace wallet


   struct KeyPair
   {
      SecureBinaryData  privKey;
      BinaryData        pubKey;
   };


   class Wallet : public QObject, protected wallet::MetaData   // Abstract parent for terminal wallet classes
   {
      Q_OBJECT

   public:
      Wallet();
      ~Wallet() override;

      virtual std::string GetWalletId() const = 0;
      virtual std::string GetWalletName() const { return walletName_; }
      virtual std::string GetShortName() const { return GetWalletName(); }
      virtual std::string GetWalletDescription() const = 0;
      virtual void SetDescription(const std::string &) = 0;
      virtual wallet::Type GetType() const { return wallet::Type::Bitcoin; }

      bool operator ==(const Wallet &w) const { return (w.GetWalletId() == GetWalletId()); }
      bool operator !=(const Wallet &w) const { return (w.GetWalletId() != GetWalletId()); }

      virtual bool containsAddress(const bs::Address &addr) = 0;
      virtual bool containsHiddenAddress(const bs::Address &addr) const { return false; }
      virtual std::vector<uint64_t> getAddrBalance(const bs::Address &addr) const;
      virtual uint32_t getAddrTxN(const bs::Address &addr) const;
      virtual BinaryData getRootId() const = 0;
      virtual std::vector<UTXO> getSpendableTxOutList(uint64_t val = UINT64_MAX) const;
      virtual std::vector<UTXO> getUTXOsToSpend(uint64_t val) const;
      virtual std::vector<UTXO> getSpendableZCList() const;
      virtual std::vector<UTXO> getRBFTxOutList() const;
      virtual void RegisterWallet(const std::shared_ptr<PyBlockDataManager>& bdm = nullptr, bool asNew = false);
      virtual void SetBDM(const std::shared_ptr<PyBlockDataManager>& bdm);
      virtual void SetUserID(const BinaryData &) {}
      virtual std::vector<LedgerEntryData> getHistoryPage(uint32_t id) const;

      virtual bool isBalanceAvailable() const;
      virtual BTCNumericTypes::balance_type GetSpendableBalance() const;
      virtual BTCNumericTypes::balance_type GetUnconfirmedBalance() const;
      virtual BTCNumericTypes::balance_type GetTotalBalance() const;
      virtual void firstInit();

      virtual void AddUnconfirmedBalance(BTCNumericTypes::balance_type delta);
      virtual bool isInitialized() const { return inited_; }
      virtual bool isWatchingOnly() const { return false; }
      virtual bool isEncrypted() const { return false; }
      virtual bool hasExtOnlyAddresses() const { return false; }
      virtual std::string GetAddressComment(const bs::Address& address) const;
      virtual bool SetAddressComment(const bs::Address &addr, const std::string &comment);
      virtual std::string GetTransactionComment(const BinaryData &txHash);
      virtual bool SetTransactionComment(const BinaryData &txOrHash, const std::string &comment);

      virtual std::vector<bs::Address> GetUsedAddressList() const { return usedAddresses_; }
      virtual std::vector<bs::Address> GetExtAddressList() const { return usedAddresses_; }
      virtual std::vector<bs::Address> GetIntAddressList() const { return usedAddresses_; }
      virtual size_t GetUsedAddressCount() const { return usedAddresses_.size(); }
      virtual size_t GetExtAddressCount() const { return usedAddresses_.size(); }
      virtual size_t GetIntAddressCount() const { return usedAddresses_.size(); }
      virtual size_t GetWalletAddressCount() const { return addrCount_; }
      virtual bs::Address GetNewExtAddress(AddressEntryType aet = AddressEntryType_Default) = 0;
      virtual bs::Address GetNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) { return GetNewExtAddress(aet); }
      virtual bs::Address GetRandomChangeAddress(AddressEntryType aet = AddressEntryType_Default);
      virtual std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) = 0;
      virtual void addAddresses(const std::vector<bs::Address> &);
      virtual std::string GetAddressIndex(const bs::Address &) = 0;
      virtual bool AddressIndexExists(const std::string &index) const = 0;
      virtual bs::Address CreateAddressWithIndex(const std::string &index, AddressEntryType aet = AddressEntryType_Default, bool signal = true) = 0;

      virtual BTCNumericTypes::balance_type GetTxBalance(int64_t val) const { return val / BTCNumericTypes::BalanceDivider; }
      virtual QString displayTxValue(int64_t val) const;
      virtual QString displaySymbol() const { return QLatin1String("XBT"); }
      virtual bool isTxValid(const BinaryData &) const { return true; }

      virtual std::shared_ptr<ResolverFeed> GetResolver(const SecureBinaryData &password) = 0;
      virtual std::shared_ptr<ResolverFeed> GetPublicKeyResolver() = 0;

      virtual wallet::TXSignRequest CreateTXRequest(const std::vector<UTXO> &
         , const std::vector<std::shared_ptr<ScriptRecipient>> &
         , const uint64_t fee = 0, bool isRBF = false, bs::Address changeAddress = {});
      virtual BinaryData SignTXRequest(const wallet::TXSignRequest &, const SecureBinaryData &password = {});
      virtual BinaryData SignPartialTXRequest(const wallet::TXSignRequest &, const SecureBinaryData &password = {});

      virtual wallet::TXSignRequest CreatePartialTXRequest(uint64_t spendVal, const std::vector<UTXO> &inputs = {}, bs::Address changeAddress = {}
         , float feePerByte = 0, const std::vector<std::shared_ptr<ScriptRecipient>> &recipients = {}, const BinaryData prevPart = {});

      virtual void UpdateBalanceFromDB();

      virtual bool IsSegWitInput(const UTXO& input);
      virtual SecureBinaryData GetPublicKeyFor(const bs::Address &) = 0;
      virtual SecureBinaryData GetPubChainedKeyFor(const bs::Address &addr) { return GetPublicKeyFor(addr); }
      virtual KeyPair GetKeyPairFor(const bs::Address &, const SecureBinaryData &password) = 0;

      virtual bool EraseFile() { return false; }

   signals:
      void addressAdded();
      void walletReset();
      void walletReady(const QString &id);

   protected:
      virtual std::shared_ptr<LMDBEnv> getDBEnv() = 0;
      virtual LMDB *getDB() = 0;

      virtual AddressEntryType getAddrTypeForAddr(const BinaryData &) = 0;
      virtual std::set<BinaryData> getAddrHashSet() = 0;
      virtual void stop();

   private:
      bool isSegWitScript(const BinaryData &script);
      void doRegister(const std::shared_ptr<PyBlockDataManager>& bdm, bool asNew);
      Signer getSigner(const wallet::TXSignRequest &, const SecureBinaryData &password);

   protected:
      std::string       walletName_;
      BTCNumericTypes::balance_type spendableBalance_;
      BTCNumericTypes::balance_type unconfirmedBalance_;
      BTCNumericTypes::balance_type totalBalance_;
      bool inited_ = false;
      std::shared_ptr<PyBlockDataManager>    bdm_;
      std::shared_ptr<SafeBtcWallet>         btcWallet_;
      mutable std::vector<bs::Address>       usedAddresses_;
      mutable std::set<BinaryData>           addrPrefixedHashes_, addressHashes_;
      mutable QMutex    addrMapsMtx_;
      QThreadPool       threadPool_;
      size_t            addrCount_ = 0;

      mutable std::map<BinaryData, std::vector<uint64_t> >  addressBalanceMap_;
      mutable std::map<BinaryData, uint32_t>                addressTxNMap_;
      mutable std::atomic_bool   updateAddrBalance_;
      mutable std::atomic_bool   updateAddrTxN_;
      std::atomic_bool           registered_;

   private:
      class UtxoFilterAdapter : public bs::UtxoReservation::Adapter
      {
      public:
         UtxoFilterAdapter(const std::string &walletId) : walletId_(walletId) {}
         void filter(std::vector<UTXO> &utxos) { parent_->filter(walletId_, utxos); }
      private:
         const std::string walletId_;
      };
      std::shared_ptr<UtxoFilterAdapter>  utxoAdapter_;
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
            case Unknown:
            default:          return "";
         }
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

   using cbPassForWallet = std::function<SecureBinaryData(const std::string &walletId)>;
   BinaryData SignMultiInputTX(const wallet::TXMultiSignRequest &, const cbPassForWallet &);

}  //namespace bs


bool operator ==(const bs::Wallet &, const bs::Wallet &);
bool operator !=(const bs::Wallet &, const bs::Wallet &);

#endif //__BS_WALLET_METADATA_H__

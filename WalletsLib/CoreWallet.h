#ifndef BS_CORE_WALLET_H
#define BS_CORE_WALLET_H

#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <lmdbpp.h>
#include "Address.h"
#include "Assets.h"
#include "BtcDefinitions.h"
#include "EasyCoDec.h"
#include "Script.h"
#include "Signer.h"
#include "WalletEncryption.h"
#include "Wallets.h"
#include "BIP32_Node.h"
#include "HDPath.h"

#define WALLETNAME_KEY          0x00000020
#define WALLETDESCRIPTION_KEY   0x00000021
#define WALLET_EXTONLY_KEY      0x00000030
#define WALLET_PWD_META_KEY     0x00000031

#define BS_WALLET_DBNAME "bs_wallet_name"

namespace spdlog {
   class logger;
}

namespace bs {
   namespace sync {
      enum class SyncState
      {
         Success,
         NothingToDo,
         Failure
      };
   }

   namespace core {
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
            const BinaryData& getPrivateEncryptionKeyId(void) const override { return emptyKey_; }

         private:
            Type  type_;
            BinaryData  emptyKey_;
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
            std::map<BinaryData, std::shared_ptr<AssetEntryMeta>>   data_;

         protected:
            unsigned int      nbMetaData_;

            MetaData() : nbMetaData_(0) {}

            std::shared_ptr<AssetEntryMeta> get(const BinaryData &key) const {
               const auto itData = data_.find(key);
               if (itData != data_.end()) {
                  return itData->second;
               }
               return nullptr;
            }
            void set(const std::shared_ptr<AssetEntryMeta> &value);
            bool write(const std::shared_ptr<LMDBEnv> env, LMDB *db);
            void readFromDB(const std::shared_ptr<LMDBEnv> env, LMDB *db);
            std::map<BinaryData, std::shared_ptr<AssetEntryMeta>> fetchAll() const { return data_; }
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

         class Seed
         {
         public:
            Seed(NetworkType netType) :
               seed_(SecureBinaryData()), netType_(netType)
            {}

            Seed(const SecureBinaryData &seed, NetworkType netType);

            bool empty() const { return seed_.isNull(); }
            bool hasPrivateKey() const { return node_.getPrivateKey().getSize() == 32; }
            const SecureBinaryData &privateKey() const { return node_.getPrivateKey(); }
            const SecureBinaryData &seed() const { return seed_; }
            NetworkType networkType() const { return netType_; }
            void setNetworkType(NetworkType netType) { netType_ = netType; walletId_.clear(); }
            std::string getWalletId() const;

            EasyCoDec::Data toEasyCodeChecksum(size_t ckSumSize = 2) const;
            static SecureBinaryData decodeEasyCodeChecksum(const EasyCoDec::Data &, size_t ckSumSize = 2);
            static BinaryData decodeEasyCodeLineChecksum(const std::string&easyCodeHalf, size_t ckSumSize = 2, size_t keyValueSize = 16);
            static Seed fromEasyCodeChecksum(const EasyCoDec::Data &, NetworkType, size_t ckSumSize = 2);

            SecureBinaryData toXpriv(void) const;
            static Seed fromXpriv(const SecureBinaryData&, NetworkType);
            const BIP32_Node& getNode(void) const { return node_; }

         private:
            BIP32_Node node_;
            SecureBinaryData seed_;
            NetworkType       netType_ = NetworkType::Invalid;
            mutable std::string  walletId_;
         };

         enum class Type {
            Unknown,
            Bitcoin,
            ColorCoin,
            Authentication,
            Settlement
         };

         struct TXSignRequest
         {
            std::vector<std::string>   walletIds;
            std::vector<UTXO>          inputs;
            std::vector<std::shared_ptr<ScriptRecipient>>   recipients;
            struct {
               bs::Address address;
               std::string index;
               uint64_t    value = 0;
            }  change;
            uint64_t    fee = 0;
            bool        RBF = false;
            std::vector<BinaryData>       prevStates;
            bool        populateUTXOs = false;
            std::string comment;

            // Used when offline TX export is requested
            std::string offlineFilePath;

            bool isValid() const noexcept;
            BinaryData serializeState(const std::shared_ptr<ResolverFeed> &resolver = nullptr) const {
               return getSigner(resolver).serializeState();
            }
            BinaryData txId(const std::shared_ptr<ResolverFeed> &resolver=nullptr) const {
               return getSigner(resolver).getTxId();
            }
            size_t estimateTxVirtSize() const;

            using ContainsAddressCb = std::function<bool(const bs::Address &address)>;
            uint64_t amount(const ContainsAddressCb &containsAddressCb) const;
            uint64_t inputAmount(const ContainsAddressCb &containsAddressCb) const;
            uint64_t totalSpent(const ContainsAddressCb &containsAddressCb) const;
            uint64_t changeAmount(const ContainsAddressCb &containsAddressCb) const;

            uint64_t amountReceived(const ContainsAddressCb &containsAddressCb) const;
            uint64_t amountSent(const ContainsAddressCb &containsAddressCb) const;

            std::vector<UTXO> getInputs(const ContainsAddressCb &containsAddressCb) const;
            std::vector<std::shared_ptr<ScriptRecipient>> getRecipients(const ContainsAddressCb &containsAddressCb) const;

            void DebugPrint(const std::string& prefix, const std::shared_ptr<spdlog::logger>& logger, bool serializeAndPrint, const std::shared_ptr<ResolverFeed> &resolver=nullptr);

         private:
            Signer getSigner(const std::shared_ptr<ResolverFeed> &resolver = nullptr) const;
         };


         struct TXMultiSignRequest
         {
            std::map<UTXO, std::string>     inputs;     // per-wallet UTXOs
            std::vector<std::shared_ptr<ScriptRecipient>>   recipients;
            BinaryData  prevState;
            bool RBF;

            bool isValid() const noexcept;
            void addInput(const UTXO &utxo, const std::string &walletId) { inputs[utxo] = walletId; }
         };


         struct SettlementData
         {
            BinaryData  settlementId;
            BinaryData  cpPublicKey;
            bool        ownKeyFirst = true;
         };

         BinaryData computeID(const BinaryData &input);

      }  // namepsace wallet


      struct KeyPair
      {
         SecureBinaryData  privKey;
         BinaryData        pubKey;
      };


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


      class Wallet : protected wallet::MetaData   // Abstract parent for generic wallet classes
      {
      public:
         Wallet(std::shared_ptr<spdlog::logger> logger);
         virtual ~Wallet();

         virtual std::string walletId() const { return "defaultWalletID"; }
         virtual std::string name() const { return walletName_; }
         virtual std::string shortName() const { return name(); }
         virtual wallet::Type type() const { return wallet::Type::Bitcoin; }

         bool operator ==(const Wallet &w) const { return (w.walletId() == walletId()); }
         bool operator !=(const Wallet &w) const { return (w.walletId() != walletId()); }

         virtual bool containsAddress(const bs::Address &addr) = 0;
         virtual bool containsHiddenAddress(const bs::Address &) const { return false; }
         virtual BinaryData getRootId() const = 0;
         virtual NetworkType networkType(void) const = 0;

         virtual bool isWatchingOnly() const = 0;
         virtual bool hasExtOnlyAddresses() const { return false; }

         virtual std::string getAddressComment(const bs::Address& address) const;
         virtual bool setAddressComment(const bs::Address &addr, const std::string &comment);
         virtual std::string getTransactionComment(const BinaryData &txHash);
         virtual bool setTransactionComment(const BinaryData &txHash, const std::string &comment);
         virtual std::vector<std::pair<BinaryData, std::string>> getAllTxComments() const;

         virtual std::vector<bs::Address> getUsedAddressList() const { 
            return usedAddresses_; 
         }
         virtual std::vector<bs::Address> getPooledAddressList() const { return {}; }
         virtual std::vector<bs::Address> getExtAddressList() const { return usedAddresses_; }
         virtual std::vector<bs::Address> getIntAddressList() const { return usedAddresses_; }
         virtual bool isExternalAddress(const Address &) const { return true; }
         virtual unsigned getUsedAddressCount() const { return usedAddresses_.size(); }
         virtual unsigned getExtAddressCount() const { return usedAddresses_.size(); }
         virtual unsigned getIntAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getWalletAddressCount() const { return addrCount_; }

         virtual bs::Address getNewExtAddress() = 0;
         virtual bs::Address getNewIntAddress() = 0;
         virtual bs::Address getNewChangeAddress() { return getNewIntAddress(); }
         virtual std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) = 0;
         virtual std::string getAddressIndex(const bs::Address &) = 0;
         
         virtual bs::hd::Path::Elem getExtPath(void) const = 0;
         virtual bs::hd::Path::Elem getIntPath(void) const = 0;

         /***
         Used to keep track of sync wallet used address index increments on the 
         Armory wallet side
         ***/
         virtual std::pair<bs::Address, bool> synchronizeUsedAddressChain(
            const std::string&) = 0;

         /***
         Called by the sign container in reponse to sync wallet's topUpAddressPool
         Will result in public address chain extention on the relevant Armory address account
         ***/
         virtual std::vector<bs::Address> extendAddressChain(unsigned count, bool extInt) = 0;

         virtual std::shared_ptr<ResolverFeed> getResolver(void) const = 0;
         virtual std::shared_ptr<ResolverFeed> getPublicResolver(void) const = 0;
         virtual ReentrantLock lockDecryptedContainer() = 0;

         virtual BinaryData signTXRequest(const wallet::TXSignRequest &
            , bool keepDuplicatedRecipients = false);
         virtual BinaryData signPartialTXRequest(const wallet::TXSignRequest &);

         virtual SecureBinaryData getPublicKeyFor(const bs::Address &) = 0;
         virtual SecureBinaryData getPubChainedKeyFor(const bs::Address &addr) { return getPublicKeyFor(addr); }

         //shutdown db containers, typically prior to deleting the wallet file
         virtual void shutdown(void) = 0;
         virtual std::string getFilename(void) const = 0;

         //find the path for a set of prefixed scrAddr
         virtual std::map<BinaryData, bs::hd::Path> indexPath(const std::set<BinaryData>&) = 0;

         Signer getSigner(const wallet::TXSignRequest &,
            bool keepDuplicatedRecipients = false);

      protected:
         virtual std::shared_ptr<LMDBEnv> getDBEnv() = 0;
         virtual LMDB *getDB() = 0;

      protected:
         std::string       walletName_;
         std::shared_ptr<spdlog::logger>   logger_; // May need to be set manually.
         mutable std::vector<bs::Address>       usedAddresses_;
         mutable std::set<BinaryData>           addressHashes_;
         size_t            addrCount_ = 0;
      };

      using WalletMap = std::unordered_map<std::string, std::shared_ptr<Wallet>>;   // key is wallet id
      BinaryData SignMultiInputTX(const wallet::TXMultiSignRequest &, const WalletMap &);

   }  //namespace core
}  //namespace bs

#endif //BS_CORE_WALLET_H

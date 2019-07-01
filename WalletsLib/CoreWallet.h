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


#define WALLETNAME_KEY          0x00000020
#define WALLETDESCRIPTION_KEY   0x00000021

namespace spdlog {
   class logger;
}

namespace bs {
   namespace core {
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
            void write(const std::shared_ptr<LMDBEnv> env, LMDB *db);
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
            Seed(NetworkType netType) : netType_(netType) {}
            Seed(const std::string &seed, NetworkType netType);
            Seed(NetworkType netType, const SecureBinaryData &privKey, const BinaryData &chainCode = {})
               : privKey_(privKey), chainCode_(chainCode), netType_(netType) {}

            bool empty() const { return (privKey_.isNull() && seed_.isNull()); }
            bool hasPrivateKey() const { return (!privKey_.isNull()); }
            const SecureBinaryData &privateKey() const { return privKey_; }
            void setPrivateKey(const SecureBinaryData &privKey) { privKey_ = privKey; walletId_.clear(); }
            const BinaryData &chainCode() const { return chainCode_; walletId_.clear(); }
            const BinaryData &seed() const { return seed_; }
            void setSeed(const BinaryData &seed) { seed_ = seed; walletId_.clear(); }
            NetworkType networkType() const { return netType_; }
            void setNetworkType(NetworkType netType) { netType_ = netType; walletId_.clear(); }
            std::string getWalletId() const;

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
            std::string       walletId;
            //         bs::Wallet     *  wallet = nullptr;
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
            bool        populateUTXOs = false;
            std::string comment;

            bool isValid() const noexcept;
            BinaryData serializeState() const { return getSigner().serializeState(); }
            BinaryData txId() const { return getSigner().getTxId(); }
            size_t estimateTxVirtSize() const;

         private:
            Signer getSigner() const;
         };


         struct TXMultiSignRequest
         {
            std::map<UTXO, std::string>     inputs;     // per-wallet UTXOs
            std::vector<std::shared_ptr<ScriptRecipient>>   recipients;
            BinaryData  prevState;

            bool isValid() const noexcept;
            void addInput(const UTXO &utxo, const std::string &walletId) { inputs[utxo] = walletId; }
         };


//         size_t getInputScrSize(const std::shared_ptr<AddressEntry> &);
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
         Wallet(NetworkType, const std::shared_ptr<spdlog::logger> &logger = nullptr);
         virtual ~Wallet();

         virtual std::string walletId() const { return "defaultWalletID"; }
         virtual std::string name() const { return walletName_; }
         virtual std::string shortName() const { return name(); }
         virtual std::string description() const = 0;
         virtual void setDescription(const std::string &) = 0;
         virtual wallet::Type type() const { return wallet::Type::Bitcoin; }
         NetworkType networkType() const { return netType_; }

         virtual void setChainCode(const BinaryData &) {}

         bool operator ==(const Wallet &w) const { return (w.walletId() == walletId()); }
         bool operator !=(const Wallet &w) const { return (w.walletId() != walletId()); }

         virtual bool containsAddress(const bs::Address &addr) = 0;
         virtual bool containsHiddenAddress(const bs::Address &) const { return false; }
         virtual BinaryData getRootId() const = 0;

         //         virtual bool isInitialized() const { return inited_; }
         virtual bool isWatchingOnly() const { return false; }
         virtual std::vector<bs::wallet::EncryptionType> encryptionTypes() const { return {}; }
         virtual std::vector<SecureBinaryData> encryptionKeys() const { return {}; }
         virtual std::pair<unsigned int, unsigned int> encryptionRank() const { return { 0, 0 }; }
         virtual bool hasExtOnlyAddresses() const { return false; }

         virtual std::string getAddressComment(const bs::Address& address) const;
         virtual bool setAddressComment(const bs::Address &addr, const std::string &comment);
         virtual std::string getTransactionComment(const BinaryData &txHash);
         virtual bool setTransactionComment(const BinaryData &txHash, const std::string &comment);
         virtual std::vector<std::pair<BinaryData, std::string>> getAllTxComments() const;

         virtual std::vector<bs::Address> getUsedAddressList() const { return usedAddresses_; }
         virtual std::vector<bs::Address> getPooledAddressList() const { return {}; }
         virtual std::vector<bs::Address> getExtAddressList() const { return usedAddresses_; }
         virtual std::vector<bs::Address> getIntAddressList() const { return usedAddresses_; }
         virtual bool isExternalAddress(const Address &) const { return true; }
         virtual size_t getUsedAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getExtAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getIntAddressCount() const { return usedAddresses_.size(); }
         virtual size_t getWalletAddressCount() const { return addrCount_; }

         virtual bs::Address getNewExtAddress(AddressEntryType aet = AddressEntryType_Default) = 0;
         virtual bs::Address getNewIntAddress(AddressEntryType aet = AddressEntryType_Default) = 0;
         virtual bs::Address getNewChangeAddress(AddressEntryType aet = AddressEntryType_Default) { return getNewExtAddress(aet); }
         virtual bs::Address getRandomChangeAddress(AddressEntryType aet = AddressEntryType_Default);
         virtual std::shared_ptr<AddressEntry> getAddressEntryForAddr(const BinaryData &addr) = 0;
         virtual std::string getAddressIndex(const bs::Address &) = 0;
         virtual bool addressIndexExists(const std::string &index) const = 0;
         virtual bs::Address createAddressWithIndex(const std::string &index, bool persistent = true
            , AddressEntryType aet = AddressEntryType_Default) = 0;

         virtual std::shared_ptr<ResolverFeed> getResolver(const SecureBinaryData &password) = 0;
         virtual std::shared_ptr<ResolverFeed> getPublicKeyResolver() = 0;

         virtual BinaryData signTXRequest(const wallet::TXSignRequest &
            , const SecureBinaryData &password = {}
         , bool keepDuplicatedRecipients = false);
         virtual BinaryData signPartialTXRequest(const wallet::TXSignRequest &
            , const SecureBinaryData &password = {});

         //         virtual bool isSegWitInput(const UTXO& input);

         virtual SecureBinaryData getPublicKeyFor(const bs::Address &) = 0;
         virtual SecureBinaryData getPubChainedKeyFor(const bs::Address &addr) { return getPublicKeyFor(addr); }
         virtual KeyPair getKeyPairFor(const bs::Address &, const SecureBinaryData &password) = 0;

         virtual bool eraseFile() { return false; }

      protected:
         virtual std::shared_ptr<LMDBEnv> getDBEnv() = 0;
         virtual LMDB *getDB() = 0;

      private:
         bool isSegWitScript(const BinaryData &script);
         Signer getSigner(const wallet::TXSignRequest &, const SecureBinaryData &password,
            bool keepDuplicatedRecipients = false);

      protected:
         std::string       walletName_;
         NetworkType       netType_ = NetworkType::Invalid;
         std::shared_ptr<spdlog::logger>   logger_; // May need to be set manually.
         mutable std::vector<bs::Address>       usedAddresses_;
         mutable std::set<BinaryData>           addressHashes_;
         size_t            addrCount_ = 0;
      };


      using KeyMap = std::unordered_map<std::string, SecureBinaryData>; // key is wallet id
      using WalletMap = std::unordered_map<std::string, std::shared_ptr<Wallet>>;   // key is wallet id
      BinaryData SignMultiInputTX(const wallet::TXMultiSignRequest &, const KeyMap &, const WalletMap &);

   }  //namespace core
}  //namespace bs


/*bool operator ==(const bs::Wallet &, const bs::Wallet &);
bool operator !=(const bs::Wallet &, const bs::Wallet &);*/

#endif //BS_CORE_WALLET_H

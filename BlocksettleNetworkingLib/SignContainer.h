#ifndef __SIGN_CONTAINER_H__
#define __SIGN_CONTAINER_H__

#include <memory>
#include <string>

#include <QObject>
#include <QStringList>
#include <QVariant>

#include "ArmoryServersProvider.h"
#include "HDPath.h"
#include "CoreWallet.h"
#include "QWalletInfo.h"

#include "SignerUiDefs.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class SettlementWallet;
      class Wallet;

      enum class WalletFormat {
         Unknown = 0,
         HD,
         Plain,
         Settlement
      };

      struct WalletInfo
      {
         WalletFormat   format;
         std::string id;
         std::string name;
         std::string description;
         NetworkType netType;
      };

      struct HDWalletData
      {
         struct Leaf {
            std::string          id;
            bs::hd::Path::Elem   index;
         };
         struct Group {
            bs::hd::CoinType  type;
            std::vector<Leaf> leaves;
         };
         std::vector<Group>   groups;
      };

      struct AddressData
      {
         std::string index;
         bs::Address address;
         std::string comment;
      };

      struct TxCommentData
      {
         BinaryData  txHash;
         std::string comment;
      };

      struct WalletData
      {
         std::vector<bs::wallet::EncryptionType>   encryptionTypes;
         std::vector<SecureBinaryData>          encryptionKeys;
         std::pair<unsigned int, unsigned int>  encryptionRank{ 0,0 };
         NetworkType netType = NetworkType::Invalid;

         std::vector<AddressData>   addresses;
         std::vector<AddressData>   addrPool;
         std::vector<TxCommentData> txComments;
      };

      struct WatchingOnlyWallet
      {
         struct Address {
            std::string index;
            AddressEntryType  aet;
         };
         struct Leaf {
            std::string          id;
            bs::hd::Path::Elem   index;
            BinaryData           publicKey;
            BinaryData           chainCode;
            std::vector<Address> addresses;
         };
         struct Group {
            bs::hd::CoinType  type;
            std::vector<Leaf> leaves;
         };

         NetworkType netType = NetworkType::Invalid;
         std::string id;
         std::string name;
         std::string description;
         std::vector<Group>   groups;
      };

   }  //namespace sync
}

class ApplicationSettings;
class ConnectionManager;

class SignContainer : public QObject
{
   Q_OBJECT
public:
   enum class OpMode {
      Local = 1,
      Remote,
      Offline,
      // RemoteInproc - should be used for testing only, when you need to have signer and listener
      // running in same process and could not use TCP for any reason
      RemoteInproc,
      LocalInproc
   };
   enum class TXSignMode {
      Full,
      Partial
   };
   struct Limits {
      uint64_t    autoSignSpendXBT = UINT64_MAX;
      uint64_t    manualSpendXBT = UINT64_MAX;
      int         autoSignTimeS = 0;
      int         manualPassKeepInMemS = 0;

      Limits() {}
      Limits(uint64_t asXbt, uint64_t manXbt, int asTime, int manPwTime)
         : autoSignSpendXBT(asXbt), manualSpendXBT(manXbt), autoSignTimeS(asTime)
         , manualPassKeepInMemS(manPwTime) {}
   };
   using RequestId = unsigned int;
   using PasswordType = SecureBinaryData;

   SignContainer(const std::shared_ptr<spdlog::logger> &, OpMode opMode);
   ~SignContainer() noexcept = default;

   virtual bool Start() { return true; }
   virtual bool Stop() { return true; }
   virtual bool Connect() { return true; }
   virtual bool Disconnect() { return true; }

   virtual RequestId signTXRequest(const bs::core::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) = 0;
   virtual RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) = 0;
   virtual RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, bool autoSign = false, const PasswordType& password = {}) = 0;

   virtual RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) = 0;

   virtual void SendPassword(const std::string &walletId, const PasswordType &password,
      bool cancelledByUser) = 0;
   virtual RequestId CancelSignTx(const BinaryData &txId) = 0;

   virtual RequestId SetUserId(const BinaryData &) = 0;
   virtual RequestId createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}) = 0;
   virtual RequestId createHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) = 0;
   virtual RequestId DeleteHDRoot(const std::string &rootWalletId) = 0;
   virtual RequestId DeleteHDLeaf(const std::string &leafWalletId) = 0;
   virtual RequestId getDecryptedRootKey(const std::string &walletId, const SecureBinaryData &password = {}) = 0;
   virtual RequestId GetInfo(const std::string &rootWalletId) = 0;
   virtual void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) = 0;
   virtual RequestId changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
      , bs::wallet::KeyRank, const SecureBinaryData &oldPass, bool addNew, bool removeOld, bool dryRun) = 0;
   virtual void createSettlementWallet(const std::function<void(const std::shared_ptr<bs::sync::SettlementWallet> &)> &) {}
   virtual RequestId customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data = QVariantMap()) = 0;

   virtual void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) = 0;
   virtual void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) = 0;
   virtual void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) = 0;
   virtual void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) = 0;
   virtual void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) = 0;
   virtual void syncNewAddress(const std::string &walletId, const std::string &index, AddressEntryType
      , const std::function<void(const bs::Address &)> &) = 0;
   virtual void syncNewAddresses(const std::string &walletId, const std::vector<std::pair<std::string, AddressEntryType>> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool persistent = true) = 0;

   const OpMode &opMode() const { return mode_; }
   virtual bool hasUI() const { return false; }
   virtual bool isReady() const { return true; }
   virtual bool isOffline() const { return true; }
   virtual bool isWalletOffline(const std::string &) const { return true; }

signals:
   void connected();
   void disconnected();
   void authenticated();
   void connectionError(const QString &err);
   void ready();
   void Error(RequestId id, std::string error);
   void TXSigned(RequestId id, BinaryData signedTX, std::string error, bool cancelledByUser);

   void PasswordRequested(bs::hd::WalletInfo walletInfo, std::string prompt);

   void HDLeafCreated(RequestId id, const std::shared_ptr<bs::sync::hd::Leaf> &);
   void HDWalletCreated(RequestId id, std::shared_ptr<bs::sync::hd::Wallet>);
   void DecryptedRootKey(RequestId id, const SecureBinaryData &privKey, const SecureBinaryData &chainCode
      , std::string walletId);
   void QWalletInfo(unsigned int id, const bs::hd::WalletInfo &);
   void UserIdSet();
   void PasswordChanged(const std::string &walletId, bool success);
   void AutoSignStateChanged(const std::string &walletId, bool active, const std::string &error);

protected:
   std::shared_ptr<spdlog::logger> logger_;
   const OpMode mode_;
};


std::shared_ptr<SignContainer> CreateSigner(const std::shared_ptr<spdlog::logger> &
   , const std::shared_ptr<ApplicationSettings> &
   , SignContainer::OpMode
   , const QString &host
   , const std::shared_ptr<ConnectionManager> & connectionManager);

bool SignerConnectionExists(const QString &host, const QString &port);


#endif // __SIGN_CONTAINER_H__

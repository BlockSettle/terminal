#ifndef __AUTH_ADDRESS_MANAGER_H__
#define __AUTH_ADDRESS_MANAGER_H__

#include "AuthAddress.h"

#include <atomic>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>
#include <QObject>
#include <QThreadPool>
#include "CommonTypes.h"
#include "WalletEncryption.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "BSErrorCode.h"
#include "BSErrorCodeStrings.h"

#include "bs_communication.pb.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
      }
      class Wallet;
      class WalletsManager;
   }
}
class AddressVerificator;
class ApplicationSettings;
class ArmoryConnection;
class BaseCelerClient;
class ConnectionManager;
class AuthSignManager;
class RequestReplyCommand;
class ResolverFeed_AuthAddress;
class SignContainer;


class AuthAddressManager : public QObject
{
   Q_OBJECT

public:
   AuthAddressManager(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<ArmoryConnection> &
      , const ZmqBIP15XDataConnection::cbNewKey &);
   ~AuthAddressManager() noexcept;

   AuthAddressManager(const AuthAddressManager&) = delete;
   AuthAddressManager& operator = (const AuthAddressManager&) = delete;
   AuthAddressManager(AuthAddressManager&&) = delete;
   AuthAddressManager& operator = (AuthAddressManager&&) = delete;

   void init(const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AuthSignManager> &
      , const std::shared_ptr<SignContainer> &);
   void ConnectToPublicBridge(const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<BaseCelerClient> &);

   size_t GetAddressCount();
   bs::Address GetAddress(size_t index);
//   virtual BinaryData GetPublicKey(size_t index);

   AddressVerificationState GetState(const bs::Address &addr) const;
   void SetState(const bs::Address &addr, AddressVerificationState state);

   void setDefault(const bs::Address &addr);
   const bs::Address &getDefault() const { return defaultAddr_; }
   virtual size_t getDefaultIndex() const;

   virtual bool HaveAuthWallet() const;
   virtual bool HasAuthAddr() const;

   void CreateAuthWallet(const std::vector<bs::wallet::PasswordData> &pwdData = {}, bool signal = true);
   virtual bool CreateNewAuthAddress();

   virtual bool SubmitForVerification(const bs::Address &address);
   virtual bool ConfirmSubmitForVerification(const bs::Address &address, int expireTimeoutSeconds);
   virtual bool CancelSubmitForVerification(const bs::Address &address);

   virtual bool Verify(const bs::Address &address);
   virtual bool RevokeAddress(const bs::Address &address);

   virtual bool IsReady() const;

   virtual void OnDisconnectedFromCeler();

   virtual std::vector<bs::Address> GetVerifiedAddressList() const;
   size_t FromVerifiedIndex(size_t index) const;
   const std::unordered_set<std::string> &GetBSAddresses() const;

   static AddressEntryType mapFromScriptType(Blocksettle::Communication::AddressScriptType scrType);

private slots:
   void VerifyWalletAddresses();
   void onAuthWalletChanged();
   void onWalletChanged(const std::string &walletId);
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason);
   void onWalletCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &);
   void onWalletFailed(unsigned int id, std::string errMsg);

signals:
   void AddressListUpdated();
   void VerifiedAddressListUpdated();
   void NeedVerify(const QString &addr);
   void AddrStateChanged(const QString &addr, const QString &state);
   void AuthWalletChanged();
   void AuthWalletCreated(const QString &walletId);
   void ConnectionComplete();
   void Error(const QString &errorText);
   void Info(const QString &info);
   void AuthAddrSubmitError(const QString &address, const QString &error);
   void AuthConfirmSubmitError(const QString &address, const QString &error);
   void AuthAddrSubmitSuccess(const QString &address);
   void AuthAddressSubmitCancelled(const QString &address);
   void AuthVerifyTxSent();
   void AuthRevokeTxSent();

   void AuthAddressConfirmationRequired(float validationAmount);
   void SignFailed(const QString &text);

private:
   void SetAuthWallet();
   void ClearAddressList();
   bool setup();
   void OnDataReceived(const std::string& data);

   bool SubmitRequestToPB(const std::string& requestName, const std::string& data);

   bool SubmitAddressToPublicBridge(const bs::Address &);
   bool SendGetBSAddressListRequest();

   void ProcessSubmitAuthAddressResponse(const std::string& response, bool sigVerified);
   void ProcessConfirmAuthAddressSubmit(const std::string &response, bool sigVerified);
   void ProcessBSAddressListResponse(const std::string& response, bool sigVerified);

   void ProcessCancelAuthSubmitResponse(const std::string& response);

   void ProcessErrorResponse(const std::string& response) const;

   bool HaveBSAddressList() const;

   void VerifyWalletAddressesFunction();
   bool WalletAddressesLoaded();
   void AddAddress(const bs::Address &addr);

   template <typename TVal> TVal lookup(const bs::Address &key, const std::map<bs::Address, TVal> &container) const;
   BinaryData GetInitialTxHash(const bs::Address &addr) const { return lookup<BinaryData>(addr, initTxHash_); }
   void SetInitialTxHash(const bs::Address &addr, BinaryData hash) { initTxHash_[addr] = hash; }
   BinaryData GetVerifChangeTxHash(const bs::Address &addr) const { return lookup<BinaryData>(addr, verifChangeTxHash_); }
   void SetVerifChangeTxHash(const bs::Address &addr, BinaryData hash) { verifChangeTxHash_[addr] = hash; }
   bs::Address GetBSFundingAddress(const bs::Address &addr) const { return lookup<bs::Address>(addr, bsFundingAddresses_); }
   void SetBSFundingAddress(const bs::Address &addr, const bs::Address &fundingAddr) { bsFundingAddresses_[addr] = fundingAddr; }

   void SubmitToCeler(const bs::Address &);
   bool SendVerifyTransaction(const UTXO &, uint64_t amount, const bs::Address &, uint64_t remainder = 0);
   bool BroadcastTransaction(const BinaryData& transactionData);
   void SetBSAddressList(const std::unordered_set<std::string>& bsAddressList);

protected:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ArmoryConnection>      armory_;
   ZmqBIP15XDataConnection::cbNewKey      cbApproveConn_ = nullptr;
   std::shared_ptr<ApplicationSettings>   settings_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<AuthSignManager>       authSignManager_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<BaseCelerClient>           celerClient_;
   std::shared_ptr<AddressVerificator>    addressVerificator_;

   std::map<int, std::unique_ptr<RequestReplyCommand>>  activeCommands_;
   int requestId_{};

   mutable std::atomic_flag                  lockList_ = ATOMIC_FLAG_INIT;
   std::vector<bs::Address>                  addresses_;
   std::map<BinaryData, AddressVerificationState>   states_;
   using HashMap = std::map<bs::Address, BinaryData>;
   HashMap     initTxHash_, verifChangeTxHash_;
   std::map<bs::Address, bs::Address> bsFundingAddresses_;
   bs::Address                               defaultAddr_;

   std::unordered_set<std::string>           bsAddressList_;
   std::shared_ptr<bs::sync::Wallet>         authWallet_;

   std::shared_ptr<SignContainer>      signingContainer_;
   std::unordered_set<unsigned int>    signIdsVerify_;
   std::unordered_set<unsigned int>    signIdsRevoke_;
   std::pair<unsigned int, bool>       createWalletReqId_ = { 0, true };
};

#endif // __AUTH_ADDRESS_MANAGER_H__

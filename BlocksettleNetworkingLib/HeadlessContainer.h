#ifndef __HEADLESS_CONTAINER_H__
#define __HEADLESS_CONTAINER_H__

#include <string>
#include <memory>
#include <QObject>
#include <QStringList>
#include "HDNode.h"
#include "MetaData.h"
#include "SignContainer.h"

#include "headless.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   class SettlementAddressEntry;
   namespace hd {
      class Wallet;
   }
}
class ApplicationSettings;
class ConnectionManager;
class HeadlessListener;
class QProcess;
class WalletsManager;
class ZmqSecuredDataConnection;


class HeadlessContainer : public SignContainer
{
   Q_OBJECT
public:
   HeadlessContainer(const std::shared_ptr<spdlog::logger> &, OpMode);
   ~HeadlessContainer() noexcept = default;

   HeadlessContainer(const HeadlessContainer&) = delete;
   HeadlessContainer& operator = (const HeadlessContainer&) = delete;
   HeadlessContainer(HeadlessContainer&&) = delete;
   HeadlessContainer& operator = (HeadlessContainer&&) = delete;

   RequestId SignTXRequest(const bs::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}) override;
   RequestId SignPartialTXRequest(const bs::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override;
   RequestId SignPayoutTXRequest(const bs::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::shared_ptr<bs::SettlementAddressEntry> &
      , bool autoSign = false, const PasswordType& password = {}) override;

   RequestId SignMultiTXRequest(const bs::wallet::TXMultiSignRequest &) override;

   void SendPassword(const std::string &walletId, const PasswordType &password) override;

   RequestId SetUserId(const BinaryData &) override;
   RequestId SyncAddresses(const std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> &) override;
   RequestId CreateHDLeaf(const std::shared_ptr<bs::hd::Wallet> &, const bs::hd::Path &
      , const SecureBinaryData &password = {}) override;
   RequestId CreateHDWallet(const std::string &name, const std::string &desc
      , const SecureBinaryData &password, bool primary, const bs::wallet::Seed &seed) override;
   RequestId DeleteHD(const std::shared_ptr<bs::hd::Wallet> &) override;
   RequestId DeleteHD(const std::shared_ptr<bs::Wallet> &) override;
   RequestId GetDecryptedRootKey(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &password = {}) override;
   RequestId GetInfo(const std::shared_ptr<bs::hd::Wallet> &) override;
   void SetLimits(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &password, bool autoSign) override;
   RequestId ChangePassword(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &newPass
      , const SecureBinaryData &oldPass = {}, bs::wallet::EncryptionType encType = bs::wallet::EncryptionType::Password
      , const SecureBinaryData &encKey = {}) override;

   bool isReady() const override;
   bool isWalletOffline(const std::string &walletId) const override;

protected:
   RequestId Send(Blocksettle::Communication::headless::RequestPacket, bool incSeqNo = true);
   void ProcessSignTXResponse(unsigned int id, const std::string &data);
   void ProcessPasswordRequest(const std::string &data);
   void ProcessCreateHDWalletResponse(unsigned int id, const std::string &data);
   RequestId SendDeleteHDRequest(const std::string &rootWalletId, const std::string &leafId);
   void ProcessGetRootKeyResponse(unsigned int id, const std::string &data);
   void ProcessGetHDWalletInfoResponse(unsigned int id, const std::string &data);
   void ProcessSyncAddrResponse(const std::string &data);
   void ProcessChangePasswordResponse(unsigned int id, const std::string &data);
   void ProcessSetLimitsResponse(unsigned int id, const std::string &data);

protected:
   std::shared_ptr<HeadlessListener>   listener_;
   std::unordered_set<std::string>     missingWallets_;
};

bool KillHeadlessProcess();


class RemoteSigner : public HeadlessContainer
{
   Q_OBJECT
public:
   RemoteSigner(const std::shared_ptr<spdlog::logger> &, const QString &host
      , const QString &port
      , const std::shared_ptr<ConnectionManager>& connectionManager, const QString &pwHash = {}
      , OpMode opMode = OpMode::Remote);
   ~RemoteSigner() noexcept = default;

   bool Start() override;
   bool Stop() override;
   bool Connect() override;
   bool Disconnect() override;
   bool isOffline() const override;
   bool hasUI() const override;

protected slots:
   void onAuthenticated();
   void onConnected();
   void onDisconnected();
   void onConnError();
   void onPacketReceived(Blocksettle::Communication::headless::RequestPacket);

private:
   void ConnectHelper();
   void Authenticate();

protected:
   const QString        host_;
   const QString        port_;
   const QString        pwHash_;
   const std::string    connPubKey_;
   std::shared_ptr<ZmqSecuredDataConnection> connection_;
   bool  authPending_ = false;

private:
   std::shared_ptr<ConnectionManager> connectionManager_;
};

class LocalSigner : public RemoteSigner
{
   Q_OBJECT
public:
   LocalSigner(const std::shared_ptr<spdlog::logger> &, const QString &homeDir, NetworkType
      , const QString &port
      , const std::shared_ptr<ConnectionManager>& connectionManager
      , const QString &pwHash = {}, double asSpendLimit = 0);
   ~LocalSigner() noexcept = default;

   bool Start() override;
   bool Stop() override;

private:
   QStringList                args_;
   std::shared_ptr<QProcess>  headlessProcess_;
};


class HeadlessAddressSyncer : public QObject
{
   Q_OBJECT
public:
   HeadlessAddressSyncer(const std::shared_ptr<SignContainer> &, const std::shared_ptr<WalletsManager> &);

   void SyncWallet(const std::shared_ptr<bs::Wallet> &);

private slots:
   void onWalletsUpdated();
   void onSignerReady();

private:
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   bool     wasOffline_ = false;
};

#endif // __HEADLESS_CONTAINER_H__

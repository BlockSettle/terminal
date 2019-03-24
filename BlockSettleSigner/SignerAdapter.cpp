#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include "HeadlessApp.h"

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger, HeadlessAppObj *app)
   : QObject(app), logger_(logger), app_(app)
{
   app_->setReadyCallback([this](bool result) {
      if (result) {
         ready_ = true;
         emit ready();
         setCallbacks();
      }
   });
}

std::shared_ptr<bs::sync::WalletsManager> SignerAdapter::getWalletsManager()
{
   if (ready_ && !walletsMgr_) {
      walletsMgr_ = app_->getWalletsManager();
   }
   return walletsMgr_;
}

void SignerAdapter::setCallbacks()
{
   const auto &cbPeerConnected = [this](const std::string &ip) {
      emit peerConnected(QString::fromStdString(ip));
   };
   const auto &cbPeerDisconnected = [this](const std::string &ip) {
      emit peerDisconnected(QString::fromStdString(ip));
   };
   const auto &cbPwd = [this](const bs::core::wallet::TXSignRequest &txReq, const std::string &prompt) {
      if (txReq.autoSign) {
         emit autoSignRequiresPwd(txReq.walletId);
      }
      else {
         emit requestPassword(txReq, QString::fromStdString(prompt));
      }
   };
   const auto &cbTxSigned = [this](const BinaryData &tx) { emit txSigned(tx); };
   const auto &cbCancelTxSign = [this](const BinaryData &txHash) {
      emit cancelTxSign(txHash);
   };
   const auto &cbXbtSpent = [this](const int64_t value, bool autoSign) {
      emit xbtSpent(value, autoSign);
   };
   const auto &cbAutoSignActivated = [this](const std::string &walletId) {
      emit autoSignActivated(walletId);
   };
   const auto &cbAutoSignDeactivated = [this](const std::string &walletId) {
      emit autoSignDeactivated(walletId);
   };
   const auto &cbCustomDialog = [this](const QString &dialogName, const QVariant &data) {
      emit customDialogRequest(dialogName, data);
   };
   app_->setCallbacks(cbPeerConnected, cbPeerDisconnected, cbPwd, cbTxSigned, cbCancelTxSign
      , cbXbtSpent, cbAutoSignActivated, cbAutoSignDeactivated, cbCustomDialog);
}

void SignerAdapter::signTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(const BinaryData &)> &cb)
{
   app_->signTxRequest(txReq, password, cb);
}

void SignerAdapter::createWatchingOnlyWallet(const QString &walletId, const SecureBinaryData &password
   , const QString &path, const std::function<void(bool result)> &cb)
{
   app_->createWatchingOnlyWallet(walletId.toStdString(), password, path.toStdString(), cb);
}

void SignerAdapter::getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb)
{
   app_->getDecryptedRootNode(walletId, password, cb);
}

void SignerAdapter::reloadWallets(const QString &walletsDir, const std::function<void()> &cb)
{
   app_->reloadWallets(walletsDir.toStdString(), cb);
}

void SignerAdapter::setOnline(bool value)
{
   app_->setOnline(value);
}

void SignerAdapter::reconnect(const QString &address, const QString &port)
{
   app_->reconnect(address.toStdString(), port.toStdString());
   setCallbacks();
}

void SignerAdapter::setLimits(SignContainer::Limits limits)
{
   app_->setLimits(limits);
}

void SignerAdapter::passwordReceived(const std::string &walletId
   , const SecureBinaryData &password, bool cancelledByUser)
{
   app_->passwordReceived(walletId, password, cancelledByUser);
}

void SignerAdapter::addPendingAutoSignReq(const std::string &walletId)
{

}

void SignerAdapter::deactivateAutoSign()
{

}

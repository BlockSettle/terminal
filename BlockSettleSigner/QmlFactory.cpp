#include "QmlFactory.h"
#include <QApplication>
#include <QClipboard>
#include <spdlog/spdlog.h>
#include "AuthProxy.h"
#include "Wallets/SyncWalletsManager.h"

using namespace bs::hd;

// todo
// check authObject->signWallet results, return null object, emit error signal

QmlFactory::QmlFactory(const std::shared_ptr<ApplicationSettings> &settings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent)
   , settings_(settings)
   , connectionManager_(connectionManager)
   , logger_(logger)
{
}

void QmlFactory::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   walletsMgr_ = walletsMgr;
}

WalletInfo *QmlFactory::createWalletInfo() {
   auto wi = new bs::hd::WalletInfo();
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

WalletInfo *QmlFactory::createWalletInfo(const QString &walletId)
{
   if (!walletsMgr_) {
      return nullptr;
   }
   // ? move logic to WalletsManager ?
   bs::hd::WalletInfo *wi = nullptr;

   const auto &wallet = walletsMgr_->getWalletById(walletId.toStdString());
   if (wallet) {
      const auto rootWallet = walletsMgr_->getHDRootForLeaf(wallet->walletId());
      wi = new bs::hd::WalletInfo(wallet, rootWallet);
   }
   else {
      const auto &hdWallet = walletsMgr_->getHDWalletById(walletId.toStdString());
      if (!hdWallet) {
         // wallet not found
         wi = new bs::hd::WalletInfo();
      }
      else {
         wi = new bs::hd::WalletInfo(hdWallet);
      }
   }

   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

WalletInfo *QmlFactory::createWalletInfoFromDigitalBackup(const QString &filename) {
   auto wi = new bs::hd::WalletInfo(bs::hd::WalletInfo::fromDigitalBackup(filename));
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

AuthSignWalletObject *QmlFactory::createAutheIDSignObject(AutheIDClient::RequestType requestType
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] signing {}", walletInfo->walletId().toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, settings_, connectionManager_, this);
   authObject->connectToServer();
   authObject->signWallet(requestType, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createActivateEidObject(const QString &userId
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] activate wallet {} for {}", walletInfo->walletId().toStdString(), userId.toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, settings_, connectionManager_, this);
   walletInfo->setEncKeys(QStringList() << (userId + QStringLiteral("::")));
   authObject->connectToServer();
   authObject->signWallet(AutheIDClient::ActivateWallet, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createRemoveEidObject(int index
                                                        , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] remove device for {}, device index: {}", walletInfo->walletId().toStdString(), index);
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, settings_, connectionManager_, this);
   authObject->connectToServer();
   authObject->removeDevice(index, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}

void QmlFactory::setClipboard(const QString &text)
{
   QApplication::clipboard()->setText(text);
}

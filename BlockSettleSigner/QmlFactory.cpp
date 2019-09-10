#include "QmlFactory.h"
#include <QApplication>
#include <QStyle>
#include <QClipboard>
#include <QQuickWindow>
#include <QKeyEvent>
#include <spdlog/spdlog.h>
#include "AuthProxy.h"
#include "Wallets/SyncWalletsManager.h"
#include "SignerAdapter.h"

using namespace bs::hd;

// todo
// check authObject->signWallet results, return null object, emit error signal

QmlFactory::QmlFactory(const std::shared_ptr<ApplicationSettings> &settings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<spdlog::logger> &logger
   , QObject *parent)
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

WalletInfo *QmlFactory::createWalletInfo() const{
   auto wi = new bs::hd::WalletInfo();
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

WalletInfo *QmlFactory::createWalletInfo(const QString &walletId) const
{
   if (!walletsMgr_) {
      logger_->error("[{}] wallets manager is missing", __func__);
      return nullptr;
   }
   // ? move logic to WalletsManager ?
   bs::hd::WalletInfo *wi = nullptr;

   const auto &wallet = walletsMgr_->getWalletById(walletId.toStdString());
   if (wallet) {
      wi = new bs::hd::WalletInfo(walletsMgr_, wallet);
   }
   else {
      const auto &hdWallet = walletsMgr_->getHDWalletById(walletId.toStdString());
      if (!hdWallet) {
         logger_->warn("[{}] wallet with id {} not found", __func__, walletId.toStdString());
         wi = new bs::hd::WalletInfo();
      }
      else {
         wi = new bs::hd::WalletInfo(walletsMgr_, hdWallet);
      }
   }

   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

bs::hd::WalletInfo *QmlFactory::createWalletInfo(int index) const
{
   const auto &wallet = walletsMgr_->getHDWallet(index);
   auto wi = new bs::hd::WalletInfo(walletsMgr_, wallet);
   QQmlEngine::setObjectOwnership(wi, QQmlEngine::JavaScriptOwnership);
   return wi;
}

WalletInfo *QmlFactory::createWalletInfoFromDigitalBackup(const QString &filename) const {
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

void QmlFactory::setClipboard(const QString &text) const
{
   QApplication::clipboard()->setText(text);
}

QString QmlFactory::getClipboard() const
{
   return QApplication::clipboard()->text();
}

QRect QmlFactory::frameSize(QObject *window) const
{
   auto win = qobject_cast<QQuickWindow *>(window);
   if (win) {
      return win->frameGeometry();
   }
   return QRect();
}

int QmlFactory::titleBarHeight()
{
   return QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
}

void QmlFactory::installEventFilterToObj(QObject *object)
{
   if (!object) {
      return;
   }

   object->installEventFilter(this);
}

void QmlFactory::applyWindowFix(QQuickWindow *mw)
{
#ifdef Q_OS_WIN
   SetClassLongPtr(HWND(mw->winId()), GCLP_HBRBACKGROUND, LONG_PTR(GetStockObject(NULL_BRUSH)));
#endif
}

bool QmlFactory::eventFilter(QObject *object, QEvent *event)
{
   // Do not return true to allow propagate close event (it's needed for tx dialog close signal detection)
   if (event->type() == QEvent::Close) {
      emit closeEventReceived();
      return true;
   }

   return false;
}

QString QmlFactory::headlessPubKey() const
{
   return headlessPubKey_;
}

void QmlFactory::setHeadlessPubKey(const QString &headlessPubKey)
{
   if (headlessPubKey != headlessPubKey_) {
      headlessPubKey_ = headlessPubKey;
      emit headlessPubKeyChanged();
   }
}

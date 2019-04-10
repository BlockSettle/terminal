#include "AutheIDClient.h"
#include "AuthProxy.h"
#include "ConnectionManager.h"
#include "CoreWalletsManager.h"
#include "EasyEncValidator.h"
#include "OfflineProcessor.h"
#include "PasswordConfirmValidator.h"
#include "PdfBackupQmlPrinter.h"
#include "QMLApp.h"
#include "QmlFactory.h"
#include "QMLStatusUpdater.h"
#include "QmlWalletsViewModel.h"
#include "QPasswordData.h"
#include "QSeed.h"
#include "QWalletInfo.h"
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "SignerVersion.h"
#include "SignerUiDefs.h"
#include "TXInfo.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "WalletsProxy.h"

#include <functional>

#include <QtQml>
#include <QQmlContext>
#include <QGuiApplication>
#include <QSplashScreen>

#include <spdlog/spdlog.h>

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

Q_DECLARE_METATYPE(bs::core::wallet::TXSignRequest)
Q_DECLARE_METATYPE(bs::wallet::TXInfo)
Q_DECLARE_METATYPE(bs::hd::WalletInfo)

QMLAppObj::QMLAppObj(SignerAdapter *adapter, const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignerSettings> &params, QSplashScreen *spl, QQmlContext *ctxt)
   : QObject(nullptr), adapter_(adapter), logger_(logger), settings_(params)
   , splashScreen_(spl), ctxt_(ctxt), notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Signer"), this))
#endif // BS_USE_DBUS
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   auto settings = std::make_shared<ApplicationSettings>();
   auto connectionManager = std::make_shared<ConnectionManager>(logger);

   registerQtTypes();

   connect(adapter_, &SignerAdapter::ready, this, &QMLAppObj::onReady);
   connect(adapter_, &SignerAdapter::requestPassword, this, &QMLAppObj::onPasswordRequested);
   connect(adapter_, &SignerAdapter::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   connect(adapter_, &SignerAdapter::cancelTxSign, this, &QMLAppObj::onCancelSignTx);

   connect(adapter_, &SignerAdapter::customDialogRequest, this, [this](const QString &dialogName, const QVariantMap &data){
      QMetaObject::invokeMethod(rootObj_, "customDialogRequest"
                                , Q_ARG(QVariant, dialogName), Q_ARG(QVariant, data));
   });

   walletsModel_ = new QmlWalletsViewModel(ctxt_->engine());
   ctxt_->setContextProperty(QStringLiteral("walletsModel"), walletsModel_);

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(settings_, adapter_, logger_);
   connect(statusUpdater_.get(), &QMLStatusUpdater::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   ctxt_->setContextProperty(QStringLiteral("signerStatus"), statusUpdater_.get());

   ctxt_->setContextProperty(QStringLiteral("qmlAppObj"), this);

   ctxt_->setContextProperty(QStringLiteral("signerSettings"), settings_.get());

   settingsConnections();

   qmlFactory_ = std::make_shared<QmlFactory>(settings, connectionManager, logger_);
   ctxt_->setContextProperty(QStringLiteral("qmlFactory"), qmlFactory_.get());

   offlineProc_ = std::make_shared<OfflineProcessor>(logger_, adapter_);
   connect(offlineProc_.get(), &OfflineProcessor::requestPassword, this, &QMLAppObj::onOfflinePassword);
   ctxt_->setContextProperty(QStringLiteral("offlineProc"), offlineProc_.get());

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, adapter_);
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());
   connect(walletsProxy_.get(), &WalletsProxy::walletsChanged, [this] {
      if (walletsProxy_->walletsLoaded()) {
         if (splashScreen_) {
            splashScreen_->close();
            splashScreen_ = nullptr;
         }
      }
   });

   trayIcon_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/images/bs_logo.png")), this);
   connect(trayIcon_, &QSystemTrayIcon::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
   connect(trayIcon_, &QSystemTrayIcon::activated, this, &QMLAppObj::onSysTrayActivated);

#ifdef BS_USE_DBUS
   if (dbus_->isValid()) {
      notifMode_ = Freedesktop;

      QObject::disconnect(trayIcon_, &QSystemTrayIcon::messageClicked,
         this, &QMLAppObj::onSysTrayMsgClicked);
      connect(dbus_, &DBusNotification::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
   }
#endif // BS_USE_DBUS
}

void QMLAppObj::onReady()
{
   logger_->debug("[{}]", __func__);
   walletsMgr_ = adapter_->getWalletsManager();
   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &QMLAppObj::onWalletsSynced);

   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Syncing wallet {} of {}", cur, total);
   };
   walletsMgr_->syncWallets(cbProgress);
}

void QMLAppObj::onWalletsSynced()
{
   logger_->debug("[{}]", __func__);
   if (splashScreen_) {
      splashScreen_->close();
      splashScreen_ = nullptr;
   }
   walletsModel_->setWalletsManager(walletsMgr_);
   qmlFactory_->setWalletsManager(walletsMgr_);
}

void QMLAppObj::settingsConnections()
{
   connect(settings_.get(), &SignerSettings::offlineChanged, this, &QMLAppObj::onOfflineChanged);
   connect(settings_.get(), &SignerSettings::walletsDirChanged, this, &QMLAppObj::onWalletsDirChanged);
   connect(settings_.get(), &SignerSettings::listenSocketChanged, this, &QMLAppObj::onListenSocketChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLAppObj::onLimitsChanged);
}

void QMLAppObj::Start()
{
   trayIcon_->show();
}

void QMLAppObj::registerQtTypes()
{
   qRegisterMetaType<QJSValueList>("QJSValueList");

   qRegisterMetaType<bs::core::wallet::TXSignRequest>();
   qRegisterMetaType<AutheIDClient::RequestType>("AutheIDClient::RequestType");
   qRegisterMetaType<bs::wallet::EncryptionType>("EncryptionType");
   qRegisterMetaType<bs::wallet::QSeed>("QSeed");

   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.WalletsViewModel", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<OfflineProcessor>("com.blocksettle.OfflineProc", 1, 0,
      "OfflineProcess", QStringLiteral("Cannot create a OfflineProc instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterUncreatableType<AutheIDClient>("com.blocksettle.AutheIDClient", 1, 0,
      "AutheIDClient", QStringLiteral("Cannot create a AutheIDClient instance"));
   qmlRegisterUncreatableType<AuthSignWalletObject>("com.blocksettle.AuthSignWalletObject", 1, 0, "AuthSignWalletObject",
      QStringLiteral("Cannot create a AuthSignWalletObject instance"));

   qmlRegisterType<bs::wallet::TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");
   qmlRegisterType<QmlPdfBackup>("com.blocksettle.QmlPdfBackup", 1, 0, "QmlPdfBackup");
   qmlRegisterType<EasyEncValidator>("com.blocksettle.EasyEncValidator", 1, 0, "EasyEncValidator");
   qmlRegisterType<PasswordConfirmValidator>("com.blocksettle.PasswordConfirmValidator", 1, 0, "PasswordConfirmValidator");

   qmlRegisterType<bs::hd::WalletInfo>("com.blocksettle.WalletInfo", 1, 0, "WalletInfo");
   qmlRegisterType<bs::wallet::QSeed>("com.blocksettle.QSeed", 1, 0, "QSeed");
   qmlRegisterType<bs::wallet::QPasswordData>("com.blocksettle.QPasswordData", 1, 0, "QPasswordData");

   qmlRegisterUncreatableType<QmlFactory>("com.blocksettle.QmlFactory", 1, 0,
      "QmlFactory", QStringLiteral("Cannot create a QmlFactory instance"));

   qmlRegisterUncreatableMetaObject(
     bs::wallet::staticMetaObject, // static meta object
     "com.blocksettle.NsWallet.namespace",                // import statement (can be any string)
     1, 0,                          // major and minor version of the import
     "NsWallet",                 // name in QML (does not have to match C++ name)
     QStringLiteral("Error: namespace.bs.NsWallet: only enums")            // error in case someone tries to create a MyNamespace object
   );
}

void QMLAppObj::onOfflineChanged()
{
   adapter_->setOnline(!settings_->offline());
}

void QMLAppObj::onWalletsDirChanged()
{
   adapter_->reloadWallets(settings_->getWalletsDir(), [this] {
      walletsMgr_->reset();
      walletsMgr_->syncWallets();
   });
}

void QMLAppObj::onListenSocketChanged()
{
   if (settings_->offline()) {
      return;
   }
   logger_->info("Restarting listening socket");
   adapter_->reconnect(settings_->listenAddress(), settings_->port());
}

void QMLAppObj::onLimitsChanged()
{
   adapter_->setLimits(settings_->limits());
}

void QMLAppObj::SetRootObject(QObject *obj)
{
   rootObj_ = obj;
   connect(walletsModel_, &QmlWalletsViewModel::modelReset, [this] {
      const auto walletsView = rootObj_->findChild<QObject *>(QLatin1String("walletsView"));
      if (walletsView) {
         QMetaObject::invokeMethod(walletsView, "expandAll");
      }
   });
   connect(rootObj_, SIGNAL(passwordEntered(QString, bs::wallet::QPasswordData *, bool)),
      this, SLOT(onPasswordAccepted(QString, bs::wallet::QPasswordData *, bool)));
}

void QMLAppObj::onPasswordAccepted(const QString &walletId
                                   , bs::wallet::QPasswordData *passwordData
                                   , bool cancelledByUser)
{
   //SecureBinaryData decodedPwd = passwordData->password;
   logger_->debug("Password for wallet {} was accepted", walletId.toStdString());
   adapter_->passwordReceived(walletId.toStdString(), passwordData->password, cancelledByUser);
   if (offlinePasswordRequests_.find(walletId.toStdString()) != offlinePasswordRequests_.end()) {
      offlineProc_->passwordEntered(walletId.toStdString(), passwordData->password);
      offlinePasswordRequests_.erase(walletId.toStdString());
   }
}

void QMLAppObj::onOfflinePassword(const bs::core::wallet::TXSignRequest &txReq)
{
   offlinePasswordRequests_.insert(txReq.walletId);
   requestPassword(txReq, {}, false);
}

void QMLAppObj::onPasswordRequested(const bs::core::wallet::TXSignRequest &txReq, const QString &prompt)
{
   requestPassword(txReq, prompt);
}

void QMLAppObj::onAutoSignPwdRequested(const std::string &walletId)
{
   bs::core::wallet::TXSignRequest txReq;
   QString walletName;
   txReq.walletId = walletId;
   if (txReq.walletId.empty()) {
      const auto wallet = walletsMgr_->getPrimaryWallet();
      if (wallet) {
         txReq.walletId = wallet->walletId();
         walletName = QString::fromStdString(wallet->name());
      }
   }
   requestPassword(txReq, walletName.isEmpty() ? tr("Activate Auto-Signing") :
      tr("Activate Auto-Signing for %1").arg(walletName));
}

void QMLAppObj::requestPassword(const bs::core::wallet::TXSignRequest &txReq, const QString &prompt, bool alert)
{
   bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(txReq);
   QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

   bs::hd::WalletInfo *walletInfo = qmlFactory_.get()->createWalletInfo(txReq.walletId);
   if (!walletInfo->walletId().isEmpty()) {
      if (alert && trayIcon_) {
         QString notifPrompt = prompt;
         if (!txReq.walletId.empty()) {
            notifPrompt = tr("Enter password for %1").arg(walletInfo->name());
         }

         if (notifMode_ == QSystemTray) {
            trayIcon_->showMessage(tr("Password request"), notifPrompt, QSystemTrayIcon::Warning, 30000);
         }
   #ifdef BS_USE_DBUS
         else {
            dbus_->notifyDBus(QSystemTrayIcon::Warning,
               tr("Password request"), notifPrompt,
               QIcon(), 30000);
         }
   #endif // BS_USE_DBUS
      }

      QMetaObject::invokeMethod(rootObj_, "createTxSignDialog"
                                , Q_ARG(QVariant, prompt)
                                , Q_ARG(QVariant, QVariant::fromValue(txInfo))
                                , Q_ARG(QVariant, QVariant::fromValue(walletInfo)));
   }
   else {
      logger_->error("Wallet {} not found", txReq.walletId);
      emit offlineProc_->signFailure();
   }
}

void QMLAppObj::onSysTrayMsgClicked()
{
   logger_->debug("Systray message clicked");
   QMetaObject::invokeMethod(rootObj_, "raiseWindow");
   QGuiApplication::processEvents();
   QMetaObject::invokeMethod(rootObj_, "raiseWindow");
}

void QMLAppObj::onSysTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
   if (reason == QSystemTrayIcon::Trigger) {
      QMetaObject::invokeMethod(rootObj_, "raiseWindow");
      QGuiApplication::processEvents();
      QMetaObject::invokeMethod(rootObj_, "raiseWindow");
   }
}

void QMLAppObj::onCancelSignTx(const BinaryData &txId)
{
   emit cancelSignTx(QString::fromStdString(txId.toBinStr()));
}

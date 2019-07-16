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
#include "SignContainer.h"
#include "TXInfo.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "WalletsProxy.h"

#include <functional>

#include <QtQml>
#include <QQmlContext>
#include <QGuiApplication>
#include <QSplashScreen>
#include <QQuickWindow>

#include <spdlog/spdlog.h>

#include "bs_signer.pb.h"

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

Q_DECLARE_METATYPE(bs::core::wallet::TXSignRequest)
Q_DECLARE_METATYPE(bs::wallet::TXInfo)
Q_DECLARE_METATYPE(bs::sync::PasswordDialogData)
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
   connect(adapter_, &SignerAdapter::connectionError, this, &QMLAppObj::onConnectionError);
   connect(adapter_, &SignerAdapter::headlessBindUpdated, this, &QMLAppObj::onHeadlessBindUpdated);
   connect(adapter_, &SignerAdapter::cancelTxSign, this, &QMLAppObj::onCancelSignTx);
   connect(adapter_, &SignerAdapter::customDialogRequest, this, &QMLAppObj::onCustomDialogRequest);
   connect(adapter_, &SignerAdapter::terminalHandshakeFailed, this, &QMLAppObj::onTerminalHandshakeFailed);

   walletsModel_ = new QmlWalletsViewModel(ctxt_->engine());

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(settings_, adapter_, logger_);

   qmlFactory_ = std::make_shared<QmlFactory>(settings, connectionManager, logger_);
   adapter_->setQmlFactory(qmlFactory_);

   qmlFactory_->setHeadlessPubKey(adapter_->headlessPubKey());
   connect(adapter_, &SignerAdapter::headlessPubKeyChanged, qmlFactory_.get(), &QmlFactory::setHeadlessPubKey);

   connect(qmlFactory_.get(), &QmlFactory::showTrayNotify, this, &QMLAppObj::showTrayNotify);

   connect(qmlFactory_.get(), &QmlFactory::closeEventReceived, this, [this](){
      hideQmlWindow();
   });

   offlineProc_ = std::make_shared<OfflineProcessor>(logger_, adapter_);
   connect(offlineProc_.get(), &OfflineProcessor::requestPassword, this, &QMLAppObj::onOfflinePassword);

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, adapter_);
   connect(walletsProxy_.get(), &WalletsProxy::walletsChanged, [this] {
      if (walletsProxy_->walletsLoaded()) {
         if (splashScreen_) {
            splashScreen_->deleteLater();
            splashScreen_ = nullptr;
         }
      }
   });

   if (params->runMode() != bs::signer::ui::RunMode::lightgui) {
      trayIconOptional_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/images/bs_logo.png")), this);
      connect(trayIconOptional_, &QSystemTrayIcon::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
      connect(trayIconOptional_, &QSystemTrayIcon::activated, this, &QMLAppObj::onSysTrayActivated);

#ifdef BS_USE_DBUS
      if (dbus_->isValid()) {
         notifMode_ = Freedesktop;

         QObject::disconnect(trayIconOptional_, &QSystemTrayIcon::messageClicked,
            this, &QMLAppObj::onSysTrayMsgClicked);
         connect(dbus_, &DBusNotification::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
      }
#endif // BS_USE_DBUS
   }

   if (adapter) {
      settingsConnections();
   }

   connect(params.get(), &SignerSettings::trustedTerminalsChanged, this, [this] {
      // Show error one more time if needed
      lastFailedTerminals_.clear();
   });

   connect(params.get(), &SignerSettings::offlineChanged, this, [this] {
      // Show error one more time if needed
      lastFailedTerminals_.clear();
   });

   ctxt_->setContextProperty(QStringLiteral("walletsModel"), walletsModel_);
   ctxt_->setContextProperty(QStringLiteral("signerStatus"), statusUpdater_.get());
   ctxt_->setContextProperty(QStringLiteral("qmlAppObj"), this);
   ctxt_->setContextProperty(QStringLiteral("signerSettings"), settings_.get());
   ctxt_->setContextProperty(QStringLiteral("qmlFactory"), qmlFactory_.get());
   ctxt_->setContextProperty(QStringLiteral("offlineProc"), offlineProc_.get());
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());
}

void QMLAppObj::onReady()
{
   logger_->debug("[{}]", __func__);
   walletsMgr_ = adapter_->getWalletsManager();
   qmlFactory_->setWalletsManager(walletsMgr_);

   connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &QMLAppObj::onWalletsSynced);

   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Syncing wallet {} of {}", cur, total);
   };
   walletsMgr_->syncWallets(cbProgress);
}

void QMLAppObj::onConnectionError()
{
   QMetaObject::invokeMethod(rootObj_, "showError"
      , Q_ARG(QVariant, tr("Error connecting to headless signer process")));
}

void QMLAppObj::onHeadlessBindUpdated(bool success)
{
   if (!success) {
      QMetaObject::invokeMethod(rootObj_, "showError"
         , Q_ARG(QVariant, tr("Server start failed. Please check listen address and port")));
   }

   statusUpdater_->setSocketOk(success);
}

void QMLAppObj::onWalletsSynced()
{
   logger_->debug("[{}]", __func__);
   if (splashScreen_) {
      splashScreen_->deleteLater();
      splashScreen_ = nullptr;
   }
   walletsModel_->setWalletsManager(walletsMgr_);
}

void QMLAppObj::settingsConnections()
{
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::changed, this, &QMLAppObj::onSettingChanged);
}

void QMLAppObj::Start()
{
   if (trayIconOptional_) {
      trayIconOptional_->show();
   }
}

void QMLAppObj::registerQtTypes()
{
   qRegisterMetaType<QJSValueList>("QJSValueList");

   qRegisterMetaType<bs::core::wallet::TXSignRequest>();
   qRegisterMetaType<AutheIDClient::RequestType>("AutheIDClient::RequestType");
   qRegisterMetaType<bs::wallet::EncryptionType>("EncryptionType");
   qRegisterMetaType<bs::wallet::QSeed>("QSeed");
   qRegisterMetaType<AuthSignWalletObject>("AuthSignWalletObject");

   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.WalletsViewModel", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<OfflineProcessor>("com.blocksettle.OfflineProc", 1, 0,
      "OfflineProcess", QStringLiteral("Cannot create a OfflineProc instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterUncreatableType<AutheIDClient>("com.blocksettle.AutheIDClient", 1, 0,
      "AutheIDClient", QStringLiteral("Cannot create a AutheIDClient instance"));
   qmlRegisterUncreatableType<QmlFactory>("com.blocksettle.QmlFactory", 1, 0,
      "QmlFactory", QStringLiteral("Cannot create a QmlFactory instance"));

   qmlRegisterType<AuthSignWalletObject>("com.blocksettle.AuthSignWalletObject", 1, 0, "AuthSignWalletObject");
   qmlRegisterType<bs::wallet::TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");
   qmlRegisterType<bs::sync::PasswordDialogData>("com.blocksettle.PasswordDialogData", 1, 0, "PasswordDialogData");
   qmlRegisterType<QmlPdfBackup>("com.blocksettle.QmlPdfBackup", 1, 0, "QmlPdfBackup");
   qmlRegisterType<EasyEncValidator>("com.blocksettle.EasyEncValidator", 1, 0, "EasyEncValidator");
   qmlRegisterType<PasswordConfirmValidator>("com.blocksettle.PasswordConfirmValidator", 1, 0, "PasswordConfirmValidator");

   qmlRegisterType<bs::hd::WalletInfo>("com.blocksettle.WalletInfo", 1, 0, "WalletInfo");
   qmlRegisterType<bs::wallet::QSeed>("com.blocksettle.QSeed", 1, 0, "QSeed");
   qmlRegisterType<bs::wallet::QPasswordData>("com.blocksettle.QPasswordData", 1, 0, "QPasswordData");
}

void QMLAppObj::onLimitsChanged()
{
   adapter_->setLimits(settings_->limits());
}

void QMLAppObj::onSettingChanged(int)
{
   adapter_->syncSettings(settings_->get());
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
//   connect(rootObj_, SIGNAL(passwordEntered(QString, bs::wallet::QPasswordData *, bool)),
//           this, SLOT(onPasswordAccepted(QString, bs::wallet::QPasswordData *, bool)));
}

void QMLAppObj::raiseQmlWindow()
{
   QMetaObject::invokeMethod(rootObj_, "raiseWindow");
   auto window = qobject_cast<QQuickWindow*>(rootObj_);
   if (window) {
      window->setWindowState(Qt::WindowMinimized);
   }
   QGuiApplication::processEvents();
   if (window) {
      window->setWindowState(Qt::WindowActive);
   }
   QMetaObject::invokeMethod(rootObj_, "raiseWindow");
}

void QMLAppObj::hideQmlWindow()
{
   QMetaObject::invokeMethod(rootObj_, "hideWindow");
}

QString QMLAppObj::getUrlPath(const QUrl &url)
{
   QString path = url.path();
#ifdef Q_OS_WIN
      if (path.startsWith(QLatin1Char('/'))) {
         path.remove(0, 1);
      }
#endif
   return path;
}

void QMLAppObj::onPasswordAccepted(const QString &walletId
                                   , bs::wallet::QPasswordData *passwordData
                                   , bool cancelledByUser)
{
   logger_->debug("Password for wallet {} was accepted", walletId.toStdString());
   adapter_->passwordReceived(walletId.toStdString()
      , cancelledByUser ? bs::error::ErrorCode::TxCanceled : bs::error::ErrorCode::NoError
      , passwordData->password);
   if (offlinePasswordRequests_.find(walletId.toStdString()) != offlinePasswordRequests_.end()) {
      offlineProc_->passwordEntered(walletId.toStdString(), passwordData->password);
      offlinePasswordRequests_.erase(walletId.toStdString());
   }
}

void QMLAppObj::onOfflinePassword(const bs::core::wallet::TXSignRequest &txReq)
{
   offlinePasswordRequests_.insert(txReq.walletId);
   //requestPasswordForSigningTx(txReq, {}, false);
}

//void QMLAppObj::requestPasswordForSigningTx(const bs::core::wallet::TXSignRequest &txReq, const QString &prompt, bool alert)
//{
//   bs::wallet::TXInfo *txInfo = new bs::wallet::TXInfo(txReq);
//   QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

//   bs::hd::WalletInfo *walletInfo = qmlFactory_.get()->createWalletInfo(txReq.walletId);
//   if (!walletInfo->walletId().isEmpty()) {
//      raiseQmlWindow();
//      QMetaObject::invokeMethod(rootObj_, "createTxSignDialog"
//                                , Q_ARG(QVariant, prompt)
//                                , Q_ARG(QVariant, QVariant::fromValue(txInfo))
//                                , Q_ARG(QVariant, QVariant::fromValue(walletInfo)));
//   }
//   else {
//      logger_->error("Wallet {} not found", txReq.walletId);
//      emit offlineProc_->signFailure();
//   }
//}

void QMLAppObj::onSysTrayMsgClicked()
{
   logger_->debug("Systray message clicked");
   raiseQmlWindow();
}

void QMLAppObj::onSysTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
   if (reason == QSystemTrayIcon::Trigger) {
      raiseQmlWindow();
   }
}

void QMLAppObj::onCancelSignTx(const BinaryData &txId)
{
   emit cancelSignTx(QString::fromStdString(txId.toBinStr()));
}

void QMLAppObj::onCustomDialogRequest(const QString &dialogName, const QVariantMap &data)
{
   QMetaEnum metaSignerDialogType = QMetaEnum::fromType<bs::signer::ui::DialogType>();

   bool isDialogCorrect = false;
   for (int i = 0; i < metaSignerDialogType.keyCount(); ++i) {
      if (bs::signer::ui::getSignerDialogPath(static_cast<bs::signer::ui::DialogType>(i)) == dialogName) {
         isDialogCorrect = true;
         break;
      }
   }

   if (!isDialogCorrect) {
      logger_->error("[{}] unknown signer dialog {}", __func__, dialogName.toStdString());
      throw(std::logic_error("Unknown signer dialog"));
   }
   QMetaObject::invokeMethod(rootObj_, "customDialogRequest"
                             , Q_ARG(QVariant, dialogName), Q_ARG(QVariant, data));
}

void QMLAppObj::onTerminalHandshakeFailed(const std::string &peerAddress)
{
   // Show error only once (because terminal will try reconnect)
   if (lastFailedTerminals_.find(peerAddress) != lastFailedTerminals_.end()) {
      return;
   }

   lastFailedTerminals_.insert(peerAddress);

   QMetaObject::invokeMethod(rootObj_, "terminalHandshakeFailed"
                             , Q_ARG(QVariant, QString::fromStdString(peerAddress)));
}

void QMLAppObj::showTrayNotify(const QString &title, const QString &msg)
{
   if (trayIconOptional_) {
      if (notifMode_ == QSystemTray) {
         trayIconOptional_->showMessage(title, msg, QSystemTrayIcon::Warning, 30000);
      }
#ifdef BS_USE_DBUS
      else {
         dbus_->notifyDBus(QSystemTrayIcon::Warning,
            title, msg,
            QIcon(), 30000);
      }
#endif // BS_USE_DBUS
   }
}

/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "CoreWalletsManager.h"
#include "EasyEncValidator.h"
#include "Bip39EntryValidator.h"
#include "PasswordConfirmValidator.h"
#include "PdfBackupQmlPrinter.h"
#include "QMLApp.h"
#include "QmlFactory.h"
#include "QMLStatusUpdater.h"
#include "QmlWalletsViewModel.h"
#include "Wallets/QPasswordData.h"
#include "Wallets/QSeed.h"
#include "Wallets/QWalletInfo.h"
#include "SignerAdapter.h"
#include "Settings/SignerSettings.h"
#include "SignerVersion.h"
#include "Wallets/SignerUiDefs.h"
#include "Wallets/SignContainer.h"
#include "TXInfo.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "WalletsProxy.h"
#include "hwdevicemanager.h"
#include "hwdevicemodel.h"

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

   if (!adapter_) {
      return;
   }

   connect(adapter_, &SignerAdapter::ready, this, &QMLAppObj::onReady);
   connect(adapter_, &SignerAdapter::connectionError, this, &QMLAppObj::onConnectionError);
   connect(adapter_, &SignerAdapter::headlessBindUpdated, this, &QMLAppObj::onHeadlessBindUpdated);
   connect(adapter_, &SignerAdapter::cancelTxSign, this, &QMLAppObj::onCancelSignTx);
   connect(adapter_, &SignerAdapter::customDialogRequest, this, &QMLAppObj::onCustomDialogRequest);
   connect(adapter_, &SignerAdapter::terminalHandshakeFailed, this, &QMLAppObj::onTerminalHandshakeFailed);
   connect(adapter_, &SignerAdapter::signerPubKeyUpdated, this, &QMLAppObj::onSignerPubKeyUpdated);

   walletsModel_ = new QmlWalletsViewModel(ctxt_->engine());

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(settings_, adapter_, logger_);

   qmlFactory_ = std::make_shared<QmlFactory>(settings, connectionManager, logger_);
   adapter_->setQmlFactory(qmlFactory_);

   hwDeviceManager_ = new HwDeviceManager(connectionManager,
      adapter_->getWalletsManager(), params->testNet(), this);

   qmlFactory_->setHeadlessPubKey(adapter_->headlessPubKey());
   connect(adapter_, &SignerAdapter::headlessPubKeyChanged, qmlFactory_.get(), &QmlFactory::setHeadlessPubKey);

   connect(qmlFactory_.get(), &QmlFactory::showTrayNotify, this, &QMLAppObj::showTrayNotify);

   connect(qmlFactory_.get(), &QmlFactory::closeEventReceived, this, [this](){
      hideQmlWindow();
   });

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, adapter_);
   connect(walletsProxy_.get(), &WalletsProxy::walletsChanged, [this] {
      if (walletsProxy_->walletsLoaded()) {
         if (splashScreen_) {
            splashScreen_->deleteLater();
            splashScreen_ = nullptr;
         }
      }
   });

   trayIconOptional_ = new QSystemTrayIcon(QStringLiteral(":/images/terminal.ico"), this);
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
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());
   ctxt_->setContextProperty(QStringLiteral("hwDeviceManager"), hwDeviceManager_);
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

void QMLAppObj::onHeadlessBindUpdated(bs::signer::BindStatus status)
{
   if (status == bs::signer::BindStatus::Failed) {
      QMetaObject::invokeMethod(rootObj_, "showError"
         , Q_ARG(QVariant, tr("Server start failed. Please check listen address and port")));
   }

   // bs::signer::BindStatus::Inactive is OK status too
   statusUpdater_->setSocketOk(status != bs::signer::BindStatus::Failed);
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
   emit qmlAppStarted();
}

void QMLAppObj::registerQtTypes()
{
   qRegisterMetaType<QJSValueList>("QJSValueList");

   qRegisterMetaType<bs::core::wallet::TXSignRequest>();
   qRegisterMetaType<bs::wallet::EncryptionType>("EncryptionType");
   qRegisterMetaType<bs::wallet::QSeed>("QSeed");

   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.WalletsViewModel", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterUncreatableType<QmlFactory>("com.blocksettle.QmlFactory", 1, 0,
      "QmlFactory", QStringLiteral("Cannot create a QmlFactory instance"));
   qmlRegisterUncreatableType<HwDeviceManager>("com.blocksettle.HwDeviceManager", 1, 0,
      "HwDeviceManager", QStringLiteral("Cannot create a HwDeviceManager instance"));

   qmlRegisterType<bs::wallet::TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");
   qmlRegisterType<bs::sync::PasswordDialogData>("com.blocksettle.PasswordDialogData", 1, 0, "PasswordDialogData");
   qmlRegisterType<QmlPdfBackup>("com.blocksettle.QmlPdfBackup", 1, 0, "QmlPdfBackup");
   qmlRegisterType<Bip39EntryValidator>("com.blocksettle.Bip39EntryValidator", 1, 0, "Bip39EntryValidator");
   qmlRegisterType<EasyEncValidator>("com.blocksettle.EasyEncValidator", 1, 0, "EasyEncValidator");
   qmlRegisterType<PasswordConfirmValidator>("com.blocksettle.PasswordConfirmValidator", 1, 0, "PasswordConfirmValidator");

   qmlRegisterType<bs::hd::WalletInfo>("com.blocksettle.WalletInfo", 1, 0, "WalletInfo");
   qmlRegisterType<bs::wallet::QSeed>("com.blocksettle.QSeed", 1, 0, "QSeed");
   qmlRegisterType<bs::wallet::QPasswordData>("com.blocksettle.QPasswordData", 1, 0, "QPasswordData");
   qmlRegisterType<ControlPasswordStatus>("com.blocksettle.ControlPasswordStatus", 1, 0, "ControlPasswordStatus");
   qmlRegisterType<HwDeviceModel>("com.blocksettle.HwDeviceModel", 1, 0, "HwDeviceModel");

   // Exposing metadata to js files
   QJSValue scriptControlEnum = ctxt_->engine()->newQMetaObject(&ControlPasswordStatus::staticMetaObject);
   ctxt_->engine()->globalObject().setProperty(QLatin1String("ControlPasswordStatus"), scriptControlEnum);
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
   auto window = qobject_cast<QQuickWindow*>(rootObj_);
   connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
      adapter_->sendWindowStatus(visible);
   });
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

QString QMLAppObj::getUrlPathWithoutExtention(const QUrl &url)
{
   QString filePath = getUrlPath(url);
   QFileInfo fileInfo(filePath);

   return QDir(fileInfo.path()).absoluteFilePath(fileInfo.baseName());
}

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
   QMetaEnum metaSignerDialogType = QMetaEnum::fromType<bs::signer::ui::GeneralDialogType>();

   bool isDialogCorrect = false;
   for (int i = 0; i < metaSignerDialogType.keyCount(); ++i) {
      if (bs::signer::ui::getGeneralDialogName(static_cast<bs::signer::ui::GeneralDialogType>(i)) == dialogName) {
         isDialogCorrect = true;
         break;
      }
   }

   if (!isDialogCorrect) {
      logger_->error("[{}] unknown signer dialog {}", __func__, dialogName.toStdString());
      throw(std::logic_error("Unknown signer dialog"));
   }
   QMetaObject::invokeMethod(rootObj_, QmlBridge::getQmlMethodName(QmlBridge::CustomDialogRequest)
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

void QMLAppObj::onSignerPubKeyUpdated(const BinaryData &pubKey)
{
   qmlFactory_->setHeadlessPubKey(QString::fromStdString(pubKey.toHexStr()));
}


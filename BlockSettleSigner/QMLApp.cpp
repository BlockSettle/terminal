#include <functional>
#include <QtQml>
#include <QQmlContext>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include "SignerVersion.h"
#include "ConnectionManager.h"
#include "AuthProxy.h"
#include "AutheIDClient.h"
#include "QMLApp.h"
#include "QMLStatusUpdater.h"
#include "HDWallet.h"
#include "HeadlessContainerListener.h"
#include "OfflineProcessor.h"
#include "SignerSettings.h"
#include "TXInfo.h"
#include "WalletsManager.h"
#include "WalletsProxy.h"
#include "WalletsViewModel.h"
#include "ZmqSecuredServerConnection.h"
#include "EasyEncValidator.h"
#include "PasswordConfirmValidator.h"
#include "PdfBackupQmlPrinter.h"
#include "QmlFactory.h"
#include "QWalletInfo.h"

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

Q_DECLARE_METATYPE(bs::wallet::TXSignRequest)
Q_DECLARE_METATYPE(TXInfo)

QMLAppObj::QMLAppObj(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<SignerSettings> &params
   , QQmlContext *ctxt)
   : QObject(nullptr), logger_(logger), settings_(params), ctxt_(ctxt)
   , notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Signer"), this))
#endif // BS_USE_DBUS
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   qRegisterMetaType<bs::wallet::TXSignRequest>();
   qRegisterMetaType<AutheIDClient::RequestType>("AutheIDClient::RequestType");
   qRegisterMetaType<bs::wallet::QEncryptionType>("QEncryptionType");
   qRegisterMetaType<bs::wallet::QSeed>("QSeed");

   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.WalletsViewModel", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<OfflineProcessor>("com.blocksettle.OfflineProc", 1, 0,
      "OfflineProcess", QStringLiteral("Cannot create a OfflineProc instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterUncreatableType<AutheIDClient>("com.blocksettle.AutheIDClient", 1, 0,
      "AutheIDClient", QStringLiteral("Cannot create a AutheIDClient instance"));

   qmlRegisterType<AuthSignWalletObject>("com.blocksettle.AuthSignWalletObject", 1, 0, "AuthSignWalletObject");
   qmlRegisterType<TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");
   qmlRegisterType<QmlPdfBackup>("com.blocksettle.QmlPdfBackup", 1, 0, "QmlPdfBackup");
   qmlRegisterType<EasyEncValidator>("com.blocksettle.EasyEncValidator", 1, 0, "EasyEncValidator");
   qmlRegisterType<PasswordConfirmValidator>("com.blocksettle.PasswordConfirmValidator", 1, 0, "PasswordConfirmValidator");

   qmlRegisterType<bs::hd::WalletInfo>("com.blocksettle.WalletInfo", 1, 0, "WalletInfo");
   qmlRegisterType<bs::wallet::QSeed>("com.blocksettle.QSeed", 1, 0, "QSeed");
   qmlRegisterType<bs::wallet::QPasswordData>("com.blocksettle.QPasswordData", 1, 0, "QPasswordData");

   qmlRegisterUncreatableType<QmlFactory>("com.blocksettle.QmlFactory", 1, 0,
      "QmlFactory", QStringLiteral("Cannot create a QmlFactory instance"));



//   qmlRegisterUncreatableMetaObject(
//     bs::staticMetaObject, // static meta object
//     "com.blocksettle.NsBs.namespace",                // import statement (can be any string)
//     1, 0,                          // major and minor version of the import
//     "NsBs",                 // name in QML (does not have to match C++ name)
//     QStringLiteral("Error: com.blocksettle.NsBs: only enums")            // error in case someone tries to create a MyNamespace object
//   );

   qmlRegisterUncreatableMetaObject(
     bs::wallet::staticMetaObject, // static meta object
     "com.blocksettle.NsWallet.namespace",                // import statement (can be any string)
     1, 0,                          // major and minor version of the import
     "NsWallet",                 // name in QML (does not have to match C++ name)
     QStringLiteral("Error: namespace.bs.NsWallet: only enums")            // error in case someone tries to create a MyNamespace object
   );

   walletsMgr_ = std::make_shared<WalletsManager>(logger_);

   walletsModel_ = new QmlWalletsViewModel(walletsMgr_, ctxt_->engine());
   ctxt_->setContextProperty(QStringLiteral("walletsModel"), walletsModel_);

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(settings_);
   connect(statusUpdater_.get(), &QMLStatusUpdater::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   ctxt_->setContextProperty(QStringLiteral("signerStatus"), statusUpdater_.get());

   ctxt_->setContextProperty(QStringLiteral("qmlAppObj"), this);

   ctxt_->setContextProperty(QStringLiteral("signerSettings"), settings_.get());

   settingsConnections();

   qmlFactory_ = std::make_shared<QmlFactory>(walletsMgr_, logger_);
   ctxt_->setContextProperty(QStringLiteral("qmlFactory"), qmlFactory_.get());


   offlineProc_ = std::make_shared<OfflineProcessor>(logger_, walletsMgr_);
   connect(offlineProc_.get(), &OfflineProcessor::requestPassword, this, &QMLAppObj::onOfflinePassword);
   ctxt_->setContextProperty(QStringLiteral("offlineProc"), offlineProc_.get());

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, walletsMgr_, settings_);
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());
   connect(walletsProxy_.get(), &WalletsProxy::walletsChanged, [this] {
      if (walletsProxy_->walletsLoaded()) {
         emit loadingComplete();
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

void QMLAppObj::settingsConnections()
{
   connect(settings_.get(), &SignerSettings::offlineChanged, this, &QMLAppObj::onOfflineChanged);
   connect(settings_.get(), &SignerSettings::walletsDirChanged, this, &QMLAppObj::onWalletsDirChanged);
   connect(settings_.get(), &SignerSettings::listenSocketChanged, this, &QMLAppObj::onListenSocketChanged);
   connect(settings_.get(), &SignerSettings::passwordChanged, this, &QMLAppObj::onListenSocketChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLAppObj::onLimitsChanged);
}

void QMLAppObj::walletsLoad()
{
   logger_->debug("Loading wallets from dir <{}>", settings_->getWalletsDir().toStdString());
   walletsMgr_->LoadWallets(settings_->netType(), settings_->getWalletsDir());
   if (walletsMgr_->GetWalletsCount() > 0) {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->GetWalletsCount());

      if (!walletsMgr_->GetSettlementWallet()) {
         if (!walletsMgr_->CreateSettlementWallet(QString())) {
            logger_->error("Failed to create Settlement wallet");
         }
      }
   }
   else {
      logger_->warn("No wallets loaded");
   }

   walletsModel_->LoadWallets();
}

void QMLAppObj::Start()
{
   trayIcon_->show();
   walletsLoad();

   try {
      if (!settings_->offline()) {
         OnlineProcessing();
      }
   }
   catch (const std::exception &e) {
      if (notifMode_ == QSystemTray) {
         trayIcon_->showMessage(tr("BlockSettle Signer"), tr("Error: %1").arg(QLatin1String(e.what()))
            , QSystemTrayIcon::Critical, 10000);
      }
#ifdef BS_USE_DBUS
      else {
         dbus_->notifyDBus(QSystemTrayIcon::Critical,
            tr("BlockSettle Signer"), tr("Error: %1").arg(QLatin1String(e.what())),
            QIcon(), 10000);
      }
#endif // BS_USE_DBUS
      throw std::runtime_error("failed to open connection");
   }
}

void QMLAppObj::disconnect()
{
   listener_->disconnect();
   listener_ = nullptr;
   connection_ = nullptr;
   statusUpdater_->clearConnections();
}

void QMLAppObj::onOfflineChanged()
{
   if (settings_->offline()) {
      logger_->info("Going offline");
      disconnect();
   }
   else {
      OnlineProcessing();
   }
}

void QMLAppObj::onWalletsDirChanged()
{
   walletsMgr_->Reset();
   walletsLoad();
}

void QMLAppObj::onListenSocketChanged()
{
   if (settings_->offline()) {
      return;
   }
   logger_->info("Restarting listening socket");
   disconnect();
   OnlineProcessing();
}

void QMLAppObj::onLimitsChanged()
{
   if (listener_) {
      listener_->SetLimits(settings_->limits());
   }
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
   connect(rootObj_, SIGNAL(passwordEntered(QString, QString, bool)),
      this, SLOT(onPasswordAccepted(QString, QString, bool)));
}

void QMLAppObj::onPasswordAccepted(const QString &walletId, const QString &password,
   bool cancelledByUser)
{
   SecureBinaryData decodedPwd = BinaryData::CreateFromHex(password.toStdString());
   logger_->debug("Password for wallet {} was accepted ({})", walletId.toStdString(), password.size());
   if (listener_) {
      listener_->passwordReceived(walletId.toStdString(), decodedPwd, cancelledByUser);
   }
   if (offlinePasswordRequests_.find(walletId.toStdString()) != offlinePasswordRequests_.end()) {
      offlineProc_->passwordEntered(walletId.toStdString(), decodedPwd);
      offlinePasswordRequests_.erase(walletId.toStdString());
   }
}

void QMLAppObj::onOfflinePassword(const bs::wallet::TXSignRequest &txReq)
{
   offlinePasswordRequests_.insert(txReq.walletId);
   requestPassword(txReq, {}, false);
}

void QMLAppObj::onPasswordRequested(const bs::wallet::TXSignRequest &txReq, const QString &prompt)
{
   requestPassword(txReq, prompt);
}

void QMLAppObj::onAutoSignPwdRequested(const std::string &walletId)
{
   bs::wallet::TXSignRequest txReq;
   QString walletName;
   txReq.walletId = walletId;
   if (txReq.walletId.empty()) {
      const auto &wallet = walletsMgr_->GetPrimaryWallet();
      if (wallet) {
         txReq.walletId = wallet->getWalletId();
         walletName = QString::fromStdString(wallet->getName());
      }
   }
   requestPassword(txReq, walletName.isEmpty() ? tr("Activate Auto-Signing") :
      tr("Activate Auto-Signing for %1").arg(walletName));
}

void QMLAppObj::requestPassword(const bs::wallet::TXSignRequest &txReq, const QString &prompt, bool alert)
{
   auto txInfo = new TXInfo(walletsMgr_, txReq);
   QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

   if (alert && trayIcon_) {
      QString notifPrompt = prompt;
      if (!txReq.walletId.empty()) {
         notifPrompt = tr("Enter password for %1").arg(txInfo->wallet()->name());
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

   QMetaObject::invokeMethod(rootObj_, "createPasswordDialog", Q_ARG(QVariant, prompt)
      , Q_ARG(QVariant, QVariant::fromValue(txInfo)));
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

void QMLAppObj::OnlineProcessing()
{
   logger_->debug("Going online with socket {}:{}, network {}", settings_->listenAddress().toStdString()
      , settings_->port().toStdString(), (settings_->testNet() ? "testnet" : "mainnet"));

   const ConnectionManager connMgr(logger_);
   connection_ = connMgr.CreateSecuredServerConnection();
   if (!connection_->SetKeyPair(settings_->publicKey().toStdString(), settings_->privateKey().toStdString())) {
      logger_->error("Failed to establish secure connection");
      throw std::runtime_error("secure connection problem");
   }

   listener_ = std::make_shared<HeadlessContainerListener>(connection_, logger_, walletsMgr_
      , settings_->getWalletsDir().toStdString(), settings_->netType(), settings_->pwHash().toStdString(), true);
   listener_->SetLimits(settings_->limits());
   statusUpdater_->SetListener(listener_);
   connect(listener_.get(), &HeadlessContainerListener::passwordRequired, this, &QMLAppObj::onPasswordRequested);
   connect(listener_.get(), &HeadlessContainerListener::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   connect(listener_.get(), &HeadlessContainerListener::cancelSignTx,
      this, &QMLAppObj::onCancelSignTx);

   if (!connection_->BindConnection(settings_->listenAddress().toStdString(), settings_->port().toStdString(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}", settings_->listenAddress().toStdString(), settings_->port().toStdString());
      statusUpdater_->setSocketOk(false);
      return;
   }
   statusUpdater_->setSocketOk(true);
}

#include <functional>
#include <QtQml>
#include <QQmlContext>
#include <spdlog/spdlog.h>
#include "SignerVersion.h"
#include "ConnectionManager.h"
#include "FrejaProxy.h"
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

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

Q_DECLARE_METATYPE(bs::wallet::TXSignRequest)
Q_DECLARE_METATYPE(TXInfo)


QMLAppObj::QMLAppObj(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<SignerSettings> &params
   , QQmlContext *ctxt)
   : QObject(nullptr), logger_(logger), params_(params), ctxt_(ctxt)
   , notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Signer"), this))
#endif // BS_USE_DBUS
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   qRegisterMetaType<bs::wallet::TXSignRequest>();
   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.Wallets", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<OfflineProcessor>("com.blocksettle.OfflineProc", 1, 0,
      "OfflineProcess", QStringLiteral("Cannot create a OfflineProc instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterUncreatableType<FrejaProxy>("com.blocksettle.FrejaProxy", 1, 0,
      "FrejaProxy", QStringLiteral("Cannot create a FrejaProxy instance"));
   qmlRegisterType<FrejaSignWalletObject>("com.blocksettle.FrejaSignWalletObject", 1, 0, "FrejaSignWalletObject");
   qmlRegisterType<TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");
   qmlRegisterType<WalletInfo>("com.blocksettle.WalletInfo", 1, 0, "WalletInfo");
   qmlRegisterType<WalletSeed>("com.blocksettle.WalletSeed", 1, 0, "WalletSeed");
   qmlRegisterType<EasyEncValidator>("com.blocksettle.EasyEncValidator", 1, 0, "EasyEncValidator");
   qmlRegisterType<PasswordConfirmValidator>("com.blocksettle.PasswordConfirmValidator", 1, 0, "PasswordConfirmValidator");

   walletsMgr_ = std::make_shared<WalletsManager>(logger_);

   walletsModel_ = new QmlWalletsViewModel(walletsMgr_, ctxt_->engine());
   ctxt_->setContextProperty(QStringLiteral("walletsModel"), walletsModel_);

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(params_);
   connect(statusUpdater_.get(), &QMLStatusUpdater::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   ctxt_->setContextProperty(QStringLiteral("signerStatus"), statusUpdater_.get());

   ctxt_->setContextProperty(QStringLiteral("signerParams"), params_.get());
   settingsConnections();

   offlineProc_ = std::make_shared<OfflineProcessor>(logger_, walletsMgr_);
   connect(offlineProc_.get(), &OfflineProcessor::requestPassword, this, &QMLAppObj::onOfflinePassword);
   ctxt_->setContextProperty(QStringLiteral("offlineProc"), offlineProc_.get());

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, walletsMgr_, params_);
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());
   connect(walletsProxy_.get(), &WalletsProxy::walletsChanged, [this] {
      if (walletsProxy_->walletsLoaded()) {
         emit loadingComplete();
      }
   });

   frejaProxy_ = std::make_shared<FrejaProxy>(logger_);
   ctxt_->setContextProperty(QStringLiteral("freja"), frejaProxy_.get());

   trayIcon_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/images/bs_logo.png")), this);
   connect(trayIcon_, &QSystemTrayIcon::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
   connect(trayIcon_, &QSystemTrayIcon::activated, this, &QMLAppObj::onSysTrayActivated);

#ifdef BS_USE_DBUS
   if (dbus_->isValid()) {
      notifMode_ = Freedesktop;

      connect(dbus_, &DBusNotification::messageClicked, this, &QMLAppObj::onSysTrayMsgClicked);
   }
#endif // BS_USE_DBUS
}

void QMLAppObj::settingsConnections()
{
   connect(params_.get(), &SignerSettings::offlineChanged, this, &QMLAppObj::onOfflineChanged);
   connect(params_.get(), &SignerSettings::walletsDirChanged, this, &QMLAppObj::onWalletsDirChanged);
   connect(params_.get(), &SignerSettings::listenSocketChanged, this, &QMLAppObj::onListenSocketChanged);
   connect(params_.get(), &SignerSettings::passwordChanged, this, &QMLAppObj::onListenSocketChanged);
   connect(params_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLAppObj::onLimitsChanged);
   connect(params_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(params_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLAppObj::onLimitsChanged);
}

void QMLAppObj::walletsLoad()
{
   logger_->debug("Loading wallets from dir <{}>", params_->getWalletsDir().toStdString());
   walletsMgr_->LoadWallets(params_->netType(), params_->getWalletsDir());
   if (walletsMgr_->GetWalletsCount() > 0) {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->GetWalletsCount());

      if (!walletsMgr_->GetSettlementWallet()) {
         if (!walletsMgr_->CreateSettlementWallet(params_->netType(), params_->getWalletsDir())) {
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
      if (!params_->offline()) {
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
   if (params_->offline()) {
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
   if (params_->offline()) {
      return;
   }
   logger_->info("Restarting listening socket");
   disconnect();
   OnlineProcessing();
}

void QMLAppObj::onLimitsChanged()
{
   if (listener_) {
      listener_->SetLimits(params_->limits());
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
   connect(rootObj_, SIGNAL(passwordEntered(QString, QString)), this, SLOT(onPasswordAccepted(QString, QString)));
}

void QMLAppObj::onPasswordAccepted(const QString &walletId, const QString &password)
{
   SecureBinaryData decodedPwd = BinaryData::CreateFromHex(password.toStdString());
   logger_->debug("Password for wallet {} was accepted ({})", walletId.toStdString(), password.size());
   if (listener_) {
      listener_->passwordReceived(walletId.toStdString(), decodedPwd);
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
   requestPassword(txReq, walletName.isEmpty() ? tr("Auto-Sign") : tr("Auto-Sign for %1").arg(walletName));
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
}

void QMLAppObj::onSysTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
   if (reason == QSystemTrayIcon::DoubleClick) {
      QMetaObject::invokeMethod(rootObj_, "raiseWindow");
   }
}

void QMLAppObj::OnlineProcessing()
{
   logger_->debug("Going online with socket {}:{}, network {}", params_->listenAddress().toStdString()
      , params_->port().toStdString(), (params_->testNet() ? "testnet" : "mainnet"));

   const ConnectionManager connMgr(logger_);
   connection_ = connMgr.CreateSecuredServerConnection();
   if (!connection_->SetKeyPair(params_->publicKey().toStdString(), params_->privateKey().toStdString())) {
      logger_->error("Failed to establish secure connection");
      throw std::runtime_error("secure connection problem");
   }

   listener_ = std::make_shared<HeadlessContainerListener>(connection_, logger_, walletsMgr_
      , params_->getWalletsDir().toStdString(), params_->pwHash().toStdString(), true);
   listener_->SetLimits(params_->limits());
   statusUpdater_->SetListener(listener_);
   connect(listener_.get(), &HeadlessContainerListener::passwordRequired, this, &QMLAppObj::onPasswordRequested);
   connect(listener_.get(), &HeadlessContainerListener::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);

   if (!connection_->BindConnection(params_->listenAddress().toStdString(), params_->port().toStdString(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}", params_->listenAddress().toStdString(), params_->port().toStdString());
      statusUpdater_->setSocketOk(false);
      return;
   }
   statusUpdater_->setSocketOk(true);
}

#include <functional>
#include <QtQml>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <spdlog/spdlog.h>
#include "SignerVersion.h"
#include "ConnectionManager.h"
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

Q_DECLARE_METATYPE(bs::wallet::TXSignRequest)
Q_DECLARE_METATYPE(TXInfo)


QMLAppObj::QMLAppObj(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<SignerSettings> &params
   , QQmlContext *ctxt)
   : QObject(nullptr), logger_(logger), params_(params), ctxt_(ctxt)
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   qRegisterMetaType<bs::wallet::TXSignRequest>();
   qmlRegisterUncreatableType<QmlWalletsViewModel>("com.blocksettle.Wallets", 1, 0,
      "WalletsModel", QStringLiteral("Cannot create a WalletsViewModel instance"));
   qmlRegisterUncreatableType<OfflineProcessor>("com.blocksettle.OfflineProc", 1, 0,
      "OfflineProcess", QStringLiteral("Cannot create a OfflineProc instance"));
   qmlRegisterUncreatableType<WalletsProxy>("com.blocksettle.WalletsProxy", 1, 0,
      "WalletsProxy", QStringLiteral("Cannot create a WalletesProxy instance"));
   qmlRegisterType<TXInfo>("com.blocksettle.TXInfo", 1, 0, "TXInfo");

   walletsMgr_ = std::make_shared<WalletsManager>(logger_, params_->netType(), params_->getWalletsDir().toStdString());

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

   walletsProxy_ = std::make_shared<WalletsProxy>(logger_, walletsMgr_);
   ctxt_->setContextProperty(QStringLiteral("walletsProxy"), walletsProxy_.get());

   trayIcon_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/images/bs_logo.png")), this);
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
   walletsMgr_->LoadWallets(nullptr);
   if (walletsMgr_->GetWalletsCount() > 0) {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->GetWalletsCount());

      if (!walletsMgr_->GetSettlementWallet()) {
         if (!walletsMgr_->CreateSettlementWallet()) {
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
      trayIcon_->showMessage(tr("BlockSettle Signer"), tr("Error: %1").arg(QLatin1String(e.what()))
         , QSystemTrayIcon::Critical, 10000);
      throw std::runtime_error("failed to open connection");
   }
}

void QMLAppObj::onOfflineChanged()
{
   if (params_->offline()) {
      logger_->info("Going offline");
      listener_.reset();
      connection_.reset();
   }
   else {
      OnlineProcessing();
   }
}

void QMLAppObj::onWalletsDirChanged()
{
   walletsMgr_->Reset(params_->netType(), params_->getWalletsDir().toStdString());
   walletsLoad();
}

void QMLAppObj::onListenSocketChanged()
{
   if (params_->offline()) {
      return;
   }
   logger_->info("Restarting listening socket");
   listener_.reset();
   connection_.reset();
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
   logger_->debug("Password for wallet {} was accepted", walletId.toStdString());
   if (listener_) {
      listener_->passwordReceived(walletId.toStdString(), password.toStdString());
   }
   if (offlinePasswordRequests_.find(walletId.toStdString()) != offlinePasswordRequests_.end()) {
      offlineProc_->passwordEntered(walletId.toStdString(), password.toStdString());
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
   txReq.walletId = walletId;
   if (txReq.walletId.empty()) {
      const auto &wallet = walletsMgr_->GetPrimaryWallet();
      if (wallet) {
         txReq.walletId = wallet->getWalletId();
      }
   }
   requestPassword(txReq, tr("Auto-Sign for %1").arg(QString::fromStdString(walletId)));
}

void QMLAppObj::requestPassword(const bs::wallet::TXSignRequest &txReq, const QString &prompt, bool alert)
{
   auto txInfo = new TXInfo(walletsMgr_, txReq);
   QQmlEngine::setObjectOwnership(txInfo, QQmlEngine::JavaScriptOwnership);

   if (alert && trayIcon_) {
      QString notifPrompt = prompt;
      if (!txReq.walletId.empty()) {
         notifPrompt = tr("Enter password for %1").arg(txInfo->sendingWallet());
      }
      trayIcon_->showMessage(tr("Password request"), notifPrompt, QSystemTrayIcon::Warning, 30000);
   }

   QMetaObject::invokeMethod(rootObj_, "createPasswordDialog", Q_ARG(QVariant, prompt)
      , Q_ARG(QVariant, QVariant::fromValue(txInfo)));
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
      , params_->pwHash().toStdString(), true);
   listener_->SetLimits(params_->limits());
   statusUpdater_->SetListener(listener_);
   connect(listener_.get(), &HeadlessContainerListener::passwordRequired, this, &QMLAppObj::onPasswordRequested);
   connect(listener_.get(), &HeadlessContainerListener::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);

   if (!connection_->BindConnection(params_->listenAddress().toStdString(), params_->port().toStdString(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}", params_->listenAddress().toStdString(), params_->port().toStdString());
//      throw std::runtime_error("failed to bind listening socket");
      statusUpdater_->setSocketOk(false);
      return;
   }
   statusUpdater_->setSocketOk(true);
}

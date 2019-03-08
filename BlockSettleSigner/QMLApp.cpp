#include <functional>
#include <QtQml>
#include <QQmlContext>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include "SignerVersion.h"
#include "ConnectionManager.h"
#include "CoreHDWallet.h"
#include "AuthProxy.h"
#include "AutheIDClient.h"
#include "CoreWalletsManager.h"
#include "QMLApp.h"
#include "QMLStatusUpdater.h"
#include "QmlWalletsViewModel.h"
#include "HeadlessContainerListener.h"
#include "OfflineProcessor.h"
#include "SignerSettings.h"
#include "TXInfo.h"
#include "WalletsProxy.h"
#include "ZmqSecuredServerConnection.h"
#include "EasyEncValidator.h"
#include "PasswordConfirmValidator.h"
#include "PdfBackupQmlPrinter.h"
#include "QmlFactory.h"
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include "ZMQHelperFunctions.h"

#ifdef BS_USE_DBUS
#include "DBusNotification.h"
#endif // BS_USE_DBUS

Q_DECLARE_METATYPE(bs::core::wallet::TXSignRequest)
Q_DECLARE_METATYPE(bs::wallet::TXInfo)
Q_DECLARE_METATYPE(bs::hd::WalletInfo)

QMLAppObj::QMLAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignerSettings> &params, QQmlContext *ctxt)
   : QObject(nullptr), logger_(logger), settings_(params), ctxt_(ctxt)
   , notifMode_(QSystemTray)
#ifdef BS_USE_DBUS
   , dbus_(new DBusNotification(tr("BlockSettle Signer"), this))
#endif // BS_USE_DBUS
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   auto settings = std::make_shared<ApplicationSettings>();
   auto connectionManager = std::make_shared<ConnectionManager>(logger);

   registerQtTypes();

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger_);

   walletsModel_ = new QmlWalletsViewModel(walletsMgr_, ctxt_->engine());
   ctxt_->setContextProperty(QStringLiteral("walletsModel"), walletsModel_);

   statusUpdater_ = std::make_shared<QMLStatusUpdater>(settings_);
   connect(statusUpdater_.get(), &QMLStatusUpdater::autoSignRequiresPwd, this, &QMLAppObj::onAutoSignPwdRequested);
   ctxt_->setContextProperty(QStringLiteral("signerStatus"), statusUpdater_.get());

   ctxt_->setContextProperty(QStringLiteral("qmlAppObj"), this);

   ctxt_->setContextProperty(QStringLiteral("signerSettings"), settings_.get());

   settingsConnections();

   qmlFactory_ = std::make_shared<QmlFactory>(settings, connectionManager, walletsMgr_, logger_);
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

   connect(settings_.get(), &SignerSettings::zmqPubKeyFileChanged, [this](){
      initZmqKeys();
   });
   connect(settings_.get(), &SignerSettings::zmqPrvKeyFileChanged, [this](){
      initZmqKeys();
   });

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
   connect(settings_.get(), &SignerSettings::limitAutoSignTimeChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitAutoSignXbtChanged, this, &QMLAppObj::onLimitsChanged);
   connect(settings_.get(), &SignerSettings::limitManualXbtChanged, this, &QMLAppObj::onLimitsChanged);
}

void QMLAppObj::walletsLoad()
{
   logger_->debug("Loading wallets from dir <{}>", settings_->getWalletsDir().toStdString());
   walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir().toStdString());
   if (!walletsMgr_->empty()) {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());

      if (!settings_->watchingOnly() && !walletsMgr_->getSettlementWallet()) {
         if (!walletsMgr_->createSettlementWallet(settings_->netType(), settings_->getWalletsDir().toStdString())) {
            logger_->error("Failed to create Settlement wallet");
         }
      }
   }
   else {
      logger_->warn("No wallets loaded");
   }

   walletsModel_->loadWallets();
}

void QMLAppObj::Start()
{
   initZmqKeys();

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
   if (listener_)
      listener_->disconnect();

   listener_ = nullptr;
   connection_ = nullptr;

   if (statusUpdater_)
      statusUpdater_->clearConnections();
}

void QMLAppObj::initZmqKeys()
{
   // Get the ZMQ server public key.
   SecureBinaryData tempPubKey;
   SecureBinaryData tempPrvKey;

   bool isZmqPubKeyOk = bs::network::readZmqKeyFile(settings_->zmqPubKeyFile()
                                                    , zmqPubKey_
                                                    , true
                                                    , logger_);
   bool isZmqPrvKeyOk = bs::network::readZmqKeyFile(settings_->zmqPrvKeyFile()
                                                    , zmqPrvKey_
                                                    , true
                                                    , logger_);
   QString errorString;
   if (!isZmqPubKeyOk)  {
      errorString.append(QStringLiteral("Failed to read ZMQ server public key\n"));
   }
   if (!isZmqPrvKeyOk)  {
      errorString.append(QStringLiteral("Failed to read ZMQ server private key"));
   }

   QString detailsString;
   if (!isZmqPubKeyOk)  {
      detailsString.append(QStringLiteral("Public key: ") + settings_->zmqPubKeyFile() + QStringLiteral("\n\n"));
   }
   if (!isZmqPrvKeyOk)  {
      detailsString.append(QStringLiteral("Private key: ") + settings_->zmqPrvKeyFile());
   }

   if (!isZmqPubKeyOk || !isZmqPrvKeyOk) {
      QMetaObject::invokeMethod(this, [this, errorString, detailsString](){
         QMetaObject::invokeMethod(rootObj_, "messageBoxCritical"
                                   , Q_ARG(QVariant, QStringLiteral("Error"))
                                   , Q_ARG(QVariant, QVariant::fromValue(errorString))
                                   , Q_ARG(QVariant, QVariant::fromValue(detailsString)));
      },
      Qt::QueuedConnection);
   }

   // reset connection
   disconnect();
   onOfflineChanged();
}

void QMLAppObj::registerQtTypes()
{
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
   walletsMgr_->reset();
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
   connect(rootObj_, SIGNAL(passwordEntered(QString, bs::wallet::QPasswordData *, bool)),
      this, SLOT(onPasswordAccepted(QString, bs::wallet::QPasswordData *, bool)));
}

void QMLAppObj::onPasswordAccepted(const QString &walletId
                                   , bs::wallet::QPasswordData *passwordData
                                   , bool cancelledByUser)
{
   //SecureBinaryData decodedPwd = passwordData->password;
   logger_->debug("Password for wallet {} was accepted", walletId.toStdString());
   if (listener_) {
      listener_->passwordReceived(walletId.toStdString(), passwordData->password, cancelledByUser);
   }
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

void QMLAppObj::OnlineProcessing()
{
   logger_->debug("Going online with socket {}:{}, network {}"
      , settings_->listenAddress().toStdString()
      , settings_->port().toStdString()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   const ConnectionManager connMgr(logger_);
   connection_ = connMgr.CreateSecuredServerConnection();
   if (!connection_->SetKeyPair(zmqPubKey_, zmqPrvKey_)) {
      logger_->error("Failed to establish secure connection");
      throw std::runtime_error("secure connection problem");
   }

   listener_ = std::make_shared<HeadlessContainerListener>(connection_, logger_
      , walletsMgr_, settings_->getWalletsDir().toStdString()
      , settings_->netType(), true);
   listener_->SetLimits(settings_->limits());
   statusUpdater_->SetListener(listener_);
   connect(listener_.get(), &HeadlessContainerListener::passwordRequired, this
      , &QMLAppObj::onPasswordRequested);
   connect(listener_.get(), &HeadlessContainerListener::autoSignRequiresPwd
      , this, &QMLAppObj::onAutoSignPwdRequested);
   connect(listener_.get(), &HeadlessContainerListener::cancelSignTx, this
      , &QMLAppObj::onCancelSignTx);

   if (!connection_->BindConnection(settings_->listenAddress().toStdString()
      , settings_->port().toStdString(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}"
         , settings_->listenAddress().toStdString()
         , settings_->port().toStdString());
      statusUpdater_->setSocketOk(false);
      return;
   }
   statusUpdater_->setSocketOk(true);
}

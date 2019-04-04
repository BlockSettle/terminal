#ifdef WIN32
   #include <windows.h>
#else
   #include <termios.h>
   #include <unistd.h>
#endif // WIN32

#include <functional>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

#include <spdlog/spdlog.h>

#include "CelerStreamServerConnection.h"
#include "ConnectionManager.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "HeadlessSettings.h"
#include "SignerAdapterListener.h"
#include "SignerVersion.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZMQ_BIP15X_ServerConnection.h"


HeadlessAppObj::HeadlessAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<HeadlessSettings> &params)
   : logger_(logger), settings_(params)
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);

   const auto zmqContext = std::make_shared<ZmqContext>(logger_);
   const auto &cbTrustedClients = [this] {
      return settings_->trustedInterfaces();
   };
   const auto adapterConn = std::make_shared<ZmqBIP15XServerConnection>(logger_
      , zmqContext, cbTrustedClients);
   adapterLsn_ = std::make_shared<SignerAdapterListener>(this, adapterConn, logger_, walletsMgr_);

   if (!adapterConn->BindConnection("127.0.0.1", settings_->interfacePort()
      , adapterLsn_.get())) {
      logger_->error("Failed to bind adapter connection");
      throw std::runtime_error("failed to bind adapter socket");
   }

   const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
   QFile pubKeyFile(dir + QLatin1String("/headless.pub"));
   if (!pubKeyFile.open(QIODevice::WriteOnly)) {
      throw std::runtime_error("Failed to open public key file "
         + pubKeyFile.fileName().toStdString() + " for writing");
   }
   logger_->debug("[{}] creating pubkey file in {}", __func__, pubKeyFile.fileName().toStdString());
   pubKeyFile.write(adapterConn->getOwnPubKey().toHexStr().c_str());

   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);
}

HeadlessAppObj::~HeadlessAppObj()
{
   stopInterface();
}

void HeadlessAppObj::start()
{
   startInterface();

   logger_->debug("[{}] loading wallets from dir <{}>", __func__
      , settings_->getWalletsDir());
   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Loading wallet {} of {}", cur, total);
      adapterLsn_->onReady(cur, total);
   };
   walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir()
      , settings_->watchingOnly(), cbProgress);
   if (!settings_->watchingOnly() && !walletsMgr_->getSettlementWallet()) {
      if (!walletsMgr_->createSettlementWallet(settings_->netType(), settings_->getWalletsDir())) {
         logger_->error("Failed to create Settlement wallet");
      }
   }

   if (walletsMgr_->empty()) {
      logger_->warn("No wallets loaded");
   }
   else {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());
   }

   onlineProcessing();
   if (cbReady_) {
      cbReady_(true);
   }
}

void HeadlessAppObj::startInterface()
{
   QStringList args;
   switch (settings_->runMode()) {
   case bs::signer::ui::RunMode::headless:
      logger_->debug("[{}] no interface in headless mode", __func__);
      return;
   case bs::signer::ui::RunMode::cli:
      logger_->warn("[{}] cli run mode is not supported yet"
         , __func__);
      return;
   case bs::signer::ui::RunMode::lightgui:
      logger_->debug("[{}] starting lightgui", __func__);
      args << QLatin1String("--guimode") << QLatin1String("lightgui");
      break;
   case bs::signer::ui::RunMode::fullgui:
      logger_->debug("[{}] starting fullgui", __func__);
      args << QLatin1String("--guimode") << QLatin1String("fullgui");
      break;
   default:
      break;
   }


#ifdef Q_OS_MACOS
   QString guiPath = QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QLatin1String("Blocksettle Signer GUI.app/Contents/MacOS/Blocksettle Signer GUI"));
#else
   QString guiPath = QCoreApplication::applicationDirPath();

   guiPath += QLatin1String("/bs_signer_gui");

#ifdef Q_OS_WIN
   guiPath += QLatin1String(".exe");
#endif //Q_OS_WIN
#endif //Q_OS_MACOS

   if (!QFile::exists(guiPath)) {
      logger_->error("[{}] {} doesn't exist"
         , __func__, guiPath.toStdString());
      return;
   }
   logger_->debug("[{}] process path: {} {}", __func__
      , guiPath.toStdString(), args.join(QLatin1Char(' ')).toStdString());

   guiProcess_ = std::make_shared<QProcess>();
#ifndef NDEBUG
   guiProcess_->setProcessChannelMode(QProcess::MergedChannels);
   connect(guiProcess_.get(), &QProcess::readyReadStandardOutput, this, [this](){
      qDebug().noquote() << guiProcess_->readAllStandardOutput();
   });
#endif
   guiProcess_->start(guiPath, args);
}

void HeadlessAppObj::stopInterface()
{
   if (guiProcess_) {
      guiProcess_->kill();
   }
}

void HeadlessAppObj::onlineProcessing()
{
   if (connection_) {
      logger_->debug("[{}] already online", __func__);
      return;
   }
   logger_->debug("Using command socket {}:{}, network {}"
      , settings_->listenAddress(), settings_->listenPort()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   const ConnectionManager connMgr(logger_, settings_->trustedTerminals());
   connection_ = connMgr.CreateZMQBIP15XServerConnection();

   if (!listener_) {
      listener_ = std::make_shared<HeadlessContainerListener>(connection_, logger_
         , walletsMgr_, settings_->getWalletsDir(), settings_->netType());
   }
   listener_->SetLimits(settings_->limits());
   if (!connection_->BindConnection(settings_->listenAddress()
      , settings_->listenPort(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}"
         , settings_->listenAddress(), settings_->listenPort());
      throw std::runtime_error("failed to bind listening socket");
   }
}

#if 0
void HeadlessAppObj::OfflineProcessing()
{
   const auto cbCLI = [this](const std::shared_ptr<bs::sync::Wallet> &wallet) -> SecureBinaryData {
      std::cout << "Enter password for wallet " << wallet->name();
      if (!wallet->description().empty()) {
         std::cout << " (" << wallet->description() << ")";
      }
      (std::cout << ": ").flush();
      setConsoleEcho(false);
      std::string password;
      std::getline(std::cin, password);
      std::cout << std::endl;
      setConsoleEcho(true);
      return password;
   };

   if (!offlineProc_) {
      auto adapter = new SignerAdapter(logger_, this);
      offlineProc_ = std::make_shared<OfflineProcessor>(logger_, adapter, cbCLI);
   }
   if (!settings_->requestFiles().empty()) {
      offlineProc_->ProcessFiles(settings_->requestFiles());
      emit finished();
   }
}

void HeadlessAppObj::setConsoleEcho(bool enable) const
{
#ifdef WIN32
   HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
   DWORD mode;
   GetConsoleMode(hStdin, &mode);

   if (!enable) {
      mode &= ~ENABLE_ECHO_INPUT;
   }
   else {
      mode |= ENABLE_ECHO_INPUT;
   }
   SetConsoleMode(hStdin, mode);
#else
   struct termios tty;
   tcgetattr(STDIN_FILENO, &tty);
   if (!enable) {
      tty.c_lflag &= ~ECHO;
   }
   else {
      tty.c_lflag |= ECHO;
   }
   tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}
#endif   //0

void HeadlessAppObj::reloadWallets(const std::string &walletsDir, const std::function<void()> &cb)
{
   walletsMgr_->reset();
   walletsMgr_->loadWallets(settings_->netType(), walletsDir
      , settings_->watchingOnly(), [](int, int) {});
//   settings_->setWalletsDir(walletsDir);
   cb();
}

void HeadlessAppObj::setOnline(bool value)
{
   if (value && connection_ && listener_) {
      return;
   }
   logger_->info("[{}] changing online state to {}", __func__, value);
   if (value) {
      onlineProcessing();
   }
   else {
      connection_.reset();
      listener_.reset();
   }
}

void HeadlessAppObj::reconnect(const std::string &listenAddr, const std::string &port)
{
   setOnline(false);
//   settings_->setListenAddress(QString::fromStdString(listenAddr));  // won't be any QString trans-
//   settings_->setPort(QString::fromStdString(port));  // formations once SignerSettings are split, too
   setOnline(true);
}

void HeadlessAppObj::setLimits(SignContainer::Limits limits)
{
   if (listener_) {
      listener_->SetLimits(limits);
   }
}

void HeadlessAppObj::passwordReceived(const std::string &walletId
   , const SecureBinaryData &password, bool cancelledByUser)
{
   if (listener_) {
      listener_->passwordReceived(walletId, password, cancelledByUser);
   }
}

void HeadlessAppObj::setCallbacks(const std::function<void(const std::string &)> &cbPeerConn
   , const std::function<void(const std::string &)> &cbPeerDisconn
   , const std::function<void(const bs::core::wallet::TXSignRequest &, const std::string &)> &cbPwd
   , const std::function<void(const BinaryData &)> &cbTxSigned
   , const std::function<void(const BinaryData &)> &cbCancelTxSign
   , const std::function<void(int64_t, bool)> &cbXbtSpent
   , const std::function<void(const std::string &)> &cbAsAct
   , const std::function<void(const std::string &)> &cbAsDeact
   , const std::function<void(const std::string &, const std::string &)> &cbCustomDialog)
{
   if (listener_) {
      listener_->setCallbacks(cbPeerConn, cbPeerDisconn, cbPwd, cbTxSigned
          , cbCancelTxSign, cbXbtSpent, cbAsAct, cbAsDeact, cbCustomDialog);
   }
   else {
      logger_->error("[{}] attempting to set callbacks on uninited listener", __func__);
   }
}

void HeadlessAppObj::close()
{
   QMetaObject::invokeMethod(this, [this] { emit finished(); });
}

void HeadlessAppObj::deactivateAutoSign()
{
   if (listener_) {
      listener_->deactivateAutoSign();
   }
}

void HeadlessAppObj::addPendingAutoSignReq(const std::string &walletId)
{
   if (listener_) {
      listener_->addPendingAutoSignReq(walletId);
   }
}

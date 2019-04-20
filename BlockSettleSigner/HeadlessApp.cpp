#ifdef WIN32
   #include <windows.h>
#else
   #include <unistd.h>
#endif // WIN32

#include <fstream>
#include <functional>
#include <spdlog/spdlog.h>

#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "HeadlessSettings.h"
#include "SignerAdapterListener.h"
#include "SignerVersion.h"
#include "SystemFileUtils.h"
#include "ZmqContext.h"
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
   adapterLsn_ = std::make_shared<SignerAdapterListener>(this, adapterConn, logger_, walletsMgr_, params);

   if (!adapterConn->BindConnection("127.0.0.1", settings_->interfacePort()
      , adapterLsn_.get())) {
      logger_->error("Failed to bind adapter connection");
      throw std::runtime_error("failed to bind adapter socket");
   }

   const std::string pubKeyFileName = SystemFilePaths::appDataLocation() + "/headless.pub";
   std::ofstream out(pubKeyFileName);
   if (!out.good()) {
      throw std::runtime_error("failed to write interface connection pubkey file " + pubKeyFileName);
   }
   out << adapterConn->getOwnPubKey().toHexStr();
   out.flush();

   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);
}

void HeadlessAppObj::start()
{
   startInterface();

   logger_->debug("[{}] loading wallets from dir <{}>", __func__
      , settings_->getWalletsDir());
   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Loaded wallet {} of {}", cur, total);
      adapterLsn_->onReady(cur, total);
   };
   walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir()
      , cbProgress);

   if (walletsMgr_->empty()) {
      logger_->warn("No wallets loaded");
   }
   else {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());
   }

   ready_ = true;
   onlineProcessing();
   if (cbReady_) {
      cbReady_(true);
   }
}

void HeadlessAppObj::startInterface()
{
   std::vector<std::string> args;
   switch (settings_->runMode()) {
   case bs::signer::RunMode::headless:
      logger_->debug("[{}] no interface in headless mode", __func__);
      return;
   case bs::signer::RunMode::cli:
      logger_->warn("[{}] cli run mode is not supported yet"
         , __func__);
      return;
   case bs::signer::RunMode::lightgui:
      logger_->debug("[{}] starting lightgui", __func__);
      args.push_back("--guimode");
      args.push_back("lightgui");
      break;
   case bs::signer::RunMode::fullgui:
      logger_->debug("[{}] starting fullgui", __func__);
      args.push_back("--guimode");
      args.push_back("fullgui");
      break;
   default:
      break;
   }

   if (settings_->testNet()) {
      args.push_back("--testnet");
   }

   std::string guiPath = SystemFilePaths::applicationDir() + "/bs_signer_gui";
#ifdef WIN32
   guiPath += ".exe";
#endif

   if (!SystemFileUtils::fileExist(guiPath)) {
      logger_->error("[{}] {} doesn't exist"
         , __func__, guiPath);
      return;
   }
   logger_->debug("[{}] process path: {}", __func__, guiPath);

/*
#ifndef NDEBUG
   guiProcess_->setProcessChannelMode(QProcess::MergedChannels);
   connect(guiProcess_.get(), &QProcess::readyReadStandardOutput, this, [this](){
      qDebug().noquote() << guiProcess_->readAllStandardOutput();
   });
#endif
*/
   if (!guiProcess_.run(guiPath, args)) {
      logger_->error("Failed to run {}", guiPath);
   }
}

void HeadlessAppObj::onlineProcessing()
{
   if (!ready_) {
      return;
   }
   if (connection_) {
      logger_->debug("[{}] already online", __func__);
      return;
   }
   logger_->debug("Using command socket {}:{}, network {}"
      , settings_->listenAddress(), settings_->listenPort()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   // Set up the connection with the terminal.
   const auto zmqContext = std::make_shared<ZmqContext>(logger_);
   const BinaryData bdID = CryptoPRNG::generateRandom(8);
   std::vector<std::string> trustedTerms;
   if (settings_->getTermIDKeyStr().empty()) {
      trustedTerms = settings_->trustedTerminals();
   }
   else {
      BinaryData termIDKey;
      if (!(settings_->getTermIDKeyBin(termIDKey))) {
         logger_->error("[{}] Signer unable to get the local terminal BIP 150 "
            "ID key", __func__);
      }
      if (!(CryptoECDSA().VerifyPublicKeyValid(termIDKey))) {
         logger_->error("[{}] Signer unable to add the terminal BIP 150 ID key "
            "({})", __func__, termIDKey.toHexStr());
      }
      std::string trustedTermStr = "127.0.0.1:" + settings_->getTermIDKeyStr();
      trustedTerms.push_back(trustedTermStr);
   }
   connection_ = std::make_shared<ZmqBIP15XServerConnection>(logger_, zmqContext
      , trustedTerms, READ_UINT64_LE(bdID.getPtr()), false);

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
      , [](int, int) {});
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

void HeadlessAppObj::setLimits(bs::signer::Limits limits)
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
   exit(0);
}

void HeadlessAppObj::walletsListUpdated()
{
   if (listener_) {
      listener_->walletsListUpdated();
   }
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

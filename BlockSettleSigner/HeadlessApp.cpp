#ifdef WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include <functional>
#include <spdlog/spdlog.h>
#include "SignerVersion.h"
#include "ConnectionManager.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "InprocSigner.h"
#include "OfflineProcessor.h"
#include "SignerAdapter.h"
#include "SignerSettings.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqSecuredServerConnection.h"
#include "ZMQHelperFunctions.h"

HeadlessAppObj::HeadlessAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignerSettings> &params)
   : logger_(logger), settings_(params)
{
   // Get the ZMQ server public key.
   if (!bs::network::readZmqKeyFile(params->zmqPubKeyFile(), zmqPubKey_
      , true, logger)) {
      throw std::runtime_error("failed to read ZMQ server public key");
   }

   // Get the ZMQ server private key.
   if (!bs::network::readZmqKeyFile(params->zmqPrvKeyFile(), zmqPrvKey_
      , false, logger)) {
      throw std::runtime_error("failed to read ZMQ server private key");
   }

   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);
}

void HeadlessAppObj::Start()
{
   logger_->debug("[{}] loading {} wallets from dir <{}>", __func__
      , settings_->watchingOnly() ? "watching-only" : "full"
      , settings_->getWalletsDir().toStdString());
   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Loading wallet {} of {}", cur, total);
   };
   walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir().toStdString()
      , settings_->watchingOnly(), cbProgress);
   if (!settings_->watchingOnly() && !walletsMgr_->getSettlementWallet()) {
      if (!walletsMgr_->createSettlementWallet(settings_->netType(), settings_->getWalletsDir().toStdString())) {
         logger_->error("Failed to create Settlement wallet");
      }
   }

   if (walletsMgr_->empty()) {
      logger_->warn("No wallets loaded");
   }
   else {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());
   }

   if (settings_->offline()) {
      if (settings_->watchingOnly()) {
         logger_->critical("[{}] offline mode doesn't support watching-only wallets", __func__);
         emit finished();
         if (cbReady_) {
            cbReady_(false);
         }
         return;
      }
      OfflineProcessing();
   }
   else {
      OnlineProcessing();
   }
   if (cbReady_) {
      cbReady_(true);
   }
   emit started();
}

void HeadlessAppObj::OnlineProcessing()
{
   logger_->debug("Using command socket {}:{}, network {}"
      , settings_->listenAddress().toStdString()
      , settings_->port().toStdString()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   const ConnectionManager connMgr(logger_, settings_->trustedTerminals());
   connection_ = connMgr.CreateZMQBIP15XServerConnection();
/*   if (!connection_->SetKeyPair(zmqPubKey_, zmqPrvKey_)) {
      logger_->error("Failed to establish secure connection");
      throw std::runtime_error("secure connection problem");
   }*/

   if (!listener_) {
      listener_ = std::make_shared<HeadlessContainerListener>(connection_, logger_
         , walletsMgr_, settings_->getWalletsDir().toStdString()
         , settings_->netType());
   }
   listener_->SetLimits(settings_->limits());
   if (!connection_->BindConnection(settings_->listenAddress().toStdString()
      , settings_->port().toStdString(), listener_.get())) {
      logger_->error("Failed to bind to {}:{}"
         , settings_->listenAddress().toStdString()
         , settings_->port().toStdString());
      throw std::runtime_error("failed to bind listening socket");
   }
}

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

std::shared_ptr<bs::sync::WalletsManager> HeadlessAppObj::getWalletsManager() const
{
   auto inprocSigner = std::make_shared<InprocSigner>(walletsMgr_, logger_
      , settings_->getWalletsDir().toStdString(), settings_->netType());
   auto syncMgr = std::make_shared<bs::sync::WalletsManager>(logger_, nullptr, nullptr);
   inprocSigner->Start();
   syncMgr->setSignContainer(inprocSigner);
   return syncMgr;
}

void HeadlessAppObj::reloadWallets(const std::string &walletsDir, const std::function<void()> &cb)
{
   walletsMgr_->reset();
   walletsMgr_->loadWallets(settings_->netType(), walletsDir
      , settings_->watchingOnly(), [](int, int) {});
   settings_->setWalletsDir(QString::fromStdString(walletsDir));
   cb();
}

void HeadlessAppObj::setOnline(bool value)
{
   if (value && connection_ && listener_) {
      return;
   }
   logger_->info("[{}] changing online state to {}", __func__, value);
   if (value) {
      OnlineProcessing();
   }
   else {
      connection_.reset();
      listener_.reset();
   }
}

void HeadlessAppObj::reconnect(const std::string &listenAddr, const std::string &port)
{
   setOnline(false);
   settings_->setListenAddress(QString::fromStdString(listenAddr));  // won't be any QString trans-
   settings_->setPort(QString::fromStdString(port));  // formations once SignerSettings are split, too
   setOnline(true);
}

void HeadlessAppObj::signTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(const BinaryData &)> &cb)
{
   const auto wallet = walletsMgr_->getWalletById(txReq.walletId);
   if (!wallet) {
      cb({});
      return;
   }
   const auto signedTX = wallet->signTXRequest(txReq, password);
   cb(signedTX);
}

void HeadlessAppObj::createWatchingOnlyWallet(const std::string &walletId, const SecureBinaryData &password
   , std::string path, const std::function<void(bool)> &cb)
{
   const auto hdWallet = walletsMgr_->getHDWalletById(walletId);
   if (!hdWallet) {
      cb(false);
      return;
   }
   const auto woWallet = hdWallet->createWatchingOnly(password);
   if (!woWallet) {
      logger_->error("[{}] failed to create watching-only wallet for id {}", __func__, walletId);
      cb(false);
      return;
   }
#if !defined (Q_OS_WIN)
   if (path.find('/') != 0) {
      path = "/" + path;
   }
#endif
   try {
      woWallet->saveToDir(path);
      cb(true);
   } catch (const std::exception &e) {
      logger_->error("[WalletsProxy] failed to save watching-only wallet to {}: {}", path, e.what());
      cb(false);
   }
}

void HeadlessAppObj::getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb)
{
   const auto hdWallet = walletsMgr_->getHDWalletById(walletId);
   if (!hdWallet) {
      cb(SecureBinaryData{}, SecureBinaryData("wallet not found"));
      return;
   }
   const auto decrypted = hdWallet->getRootNode(password);
   if (!decrypted) {
      cb(SecureBinaryData{}, SecureBinaryData("failed to decrypt root node"));
      return;
   }
   cb(decrypted->privateKey(), decrypted->chainCode());
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

void HeadlessAppObj::setCallbacks(
   const std::function<void(const std::string &)> &cbPeerConn
   , const std::function<void(const std::string &)> &cbPeerDisconn
   , const std::function<void(const bs::core::wallet::TXSignRequest &, const std::string &)> &cbPwd
   , const std::function<void(const BinaryData &)> &cbTxSigned
   , const std::function<void(const BinaryData &)> &cbCancelTxSign
   , const std::function<void(int64_t, bool)> &cbXbtSpent
   , const std::function<void(const std::string &)> &cbAsAct
   , const std::function<void(const std::string &)> &cbAsDeact)
{
   if (listener_) {
      listener_->setCallbacks(cbPeerConn, cbPeerDisconn, cbPwd, cbTxSigned
         , cbCancelTxSign, cbXbtSpent, cbAsAct, cbAsDeact);
   }
   else {
      logger_->error("[{}] attempting to set callbacks on uninited listener", __func__);
   }
}

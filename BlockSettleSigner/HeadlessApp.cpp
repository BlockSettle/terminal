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
#include "HeadlessApp.h"
#include "HeadlessContainer.h"
#include "OfflineProcessor.h"
#include "SignerSettings.h"
#include "WalletsManager.h"
#include "ZmqSecuredDataConnection.h"
#include "ZMQHelperFunctions.h"

HeadlessAppObj::HeadlessAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignerSettings> &params)
   : logger_(logger), settings_(params)
{
   walletsMgr_ = std::make_shared<WalletsManager>(logger);
   logger_->info("[{}] BlockSettle Signer {} started", __func__
      , SIGNER_VERSION_STRING);
}

void HeadlessAppObj::Start()
{
   logger_->debug("Loading wallets from dir <{}>"
      , settings_->getWalletsDir().toStdString());
   walletsMgr_->LoadWallets(settings_->netType(), settings_->getWalletsDir());
   if (!walletsMgr_->GetSettlementWallet()) {
      if (!walletsMgr_->CreateSettlementWallet(QString())) {
         logger_->error("Failed to create Settlement wallet");
      }
   }

   if (!walletsMgr_->GetWalletsCount()) {
      logger_->warn("No wallets loaded");
   }
   else {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->GetWalletsCount());
   }

   if (settings_->offline()) {
      OfflineProcessing();
   }
   else {
      OnlineProcessing();
   }
}

void HeadlessAppObj::OnlineProcessing()
{
   logger_->debug("[{}] Using listening socket {}:{}, network {}", __func__
      , settings_->listenAddress().toStdString()
      , settings_->port().toStdString()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   SecureBinaryData termZMQPubKey;
   if (!bs::network::readZmqKeyFile(settings_->localTermZMQPubKeyFile()
      , termZMQPubKey, true, logger_)) {
      logger_->error("[{}] Failed to read terminal ZMQ public key"
         , __func__);
      throw std::runtime_error("Failed to read terminal ZMQ public key");
   }

   const ConnectionManager connMgr(logger_);
   connection_->SetServerPublicKey(termZMQPubKey);
   if (!connection_->SetServerPublicKey(termZMQPubKey)) {
      logger_->error("[{}] Failed to set ZMQ server public key", __func__);
      throw std::runtime_error("Failed to set ZMQ server public key");
      connection_ = nullptr;
      return;
   }

/*   if (opMode() == OpMode::RemoteInproc) {
      connection_->SetZMQTransport(ZMQTransport::InprocTransport);
   }*/

   {
      /////// TO DO: Connect everything. There are some architecture questions
      /////// to unwind first.
//      std::lock_guard<std::mutex> lock(mutex_);
      listener_ = std::make_shared<HeadlessListener>(logger_, connection_
         , settings_->netType());
/*      connect(listener_.get(), &HeadlessListener::connected, this, &RemoteSigner::onConnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::authenticated, this, &RemoteSigner::onAuthenticated, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::authFailed, [this] { authPending_ = false; });
      connect(listener_.get(), &HeadlessListener::disconnected, this, &RemoteSigner::onDisconnected, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::error, this, &RemoteSigner::onConnError, Qt::QueuedConnection);
      connect(listener_.get(), &HeadlessListener::PacketReceived, this, &RemoteSigner::onPacketReceived, Qt::QueuedConnection);*/
   }
}

void HeadlessAppObj::OfflineProcessing()
{
   const auto cbCLI = [this](const std::shared_ptr<bs::Wallet> &wallet) -> SecureBinaryData {
      std::cout << "Enter password for wallet " << wallet->GetWalletName();
      if (!wallet->GetWalletDescription().empty()) {
         std::cout << " (" << wallet->GetWalletDescription() << ")";
      }
      (std::cout << ": ").flush();
      setConsoleEcho(false);
      std::string password;
      std::getline(std::cin, password);
      std::cout << std::endl;
      setConsoleEcho(true);
      return password;
   };

   offlineProc_ = std::make_shared<OfflineProcessor>(logger_, walletsMgr_, cbCLI);
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

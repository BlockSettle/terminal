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
#include "HeadlessContainerListener.h"
#include "OfflineProcessor.h"
#include "SignerSettings.h"
#include "WalletsManager.h"
#include "ZmqSecuredServerConnection.h"
#include "ZMQHelperFunctions.h"

HeadlessAppObj::HeadlessAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignerSettings> &params)
   : logger_(logger), settings_(params)
{
   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   // Get the ZMQ server public key.
   if(!bs::network::readZMQKeyFile(params->headlessPubKeyFile(), zmqPubKey_
      , true, logger)) {
      return;
   }

   // Get the ZMQ server private key.
   if(!bs::network::readZMQKeyFile(params->headlessPrvKeyFile(), zmqPrvKey_
      , false, logger)) {
      return;
   }

   walletsMgr_ = std::make_shared<WalletsManager>(logger);
}

void HeadlessAppObj::Start()
{
   logger_->debug("Loading wallets from dir <{}>", settings_->getWalletsDir().toStdString());
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
   logger_->debug("Using command socket {}:{}, network {}"
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
      , settings_->netType(), settings_->pwHash().toStdString());
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

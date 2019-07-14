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
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "HeadlessSettings.h"
#include "SignerAdapterListener.h"
#include "SignerVersion.h"
#include "SystemFileUtils.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_ServerConnection.h"

using namespace bs::error;

HeadlessAppObj::HeadlessAppObj(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<HeadlessSettings> &params, const std::shared_ptr<DispatchQueue> &queue)
   : logger_(logger)
   , settings_(params)
   , queue_(queue)
{
   walletsMgr_ = std::make_shared<bs::core::WalletsManager>(logger);

   // Only the SignerListener's cookie will be trusted. Supply an empty set of
   // non-cookie trusted clients.
   const auto &cbTrustedClientsSL = [] {
      return ZmqBIP15XPeers();
   };

   const bool readClientCookie = true;
   const bool makeServerCookie = false;
   const std::string absCookiePath = SystemFilePaths::appDataLocation() + "/adapterClientID";

   const auto zmqContext = std::make_shared<ZmqContext>(logger_);
   guiConnection_ = std::make_unique<ZmqBIP15XServerConnection>(logger_
      , zmqContext, cbTrustedClientsSL, "", ""
      , makeServerCookie, readClientCookie, absCookiePath);
   guiListener_ = std::make_unique<SignerAdapterListener>(this, guiConnection_.get()
      , logger_, walletsMgr_, queue_, params);

   guiConnection_->setLocalHeartbeatInterval();

   if (!guiConnection_->BindConnection("127.0.0.1", settings_->interfacePort()
      , guiListener_.get())) {
      logger_->error("Failed to bind adapter connection");
      throw std::runtime_error("failed to bind adapter socket");
   }

   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   terminalListener_ = std::make_unique<HeadlessContainerListener>(logger_
      , walletsMgr_, queue_, settings_->getWalletsDir(), settings_->netType());
   terminalListener_->setCallbacks(guiListener_->callbacks());
}

HeadlessAppObj::~HeadlessAppObj() noexcept = default;

void HeadlessAppObj::start()
{
   startInterface();

   logger_->debug("[{}] loading wallets from dir <{}>", __func__
      , settings_->getWalletsDir());
   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Loaded wallet {} of {}", cur, total);
   };
   walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir()
      , cbProgress);

   if (walletsMgr_->empty()) {
      logger_->warn("No wallets loaded");
   }
   else {
      logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());
   }

   if (!settings_->offline()) {
      startTerminalsProcessing();
   } else {
      SPDLOG_LOGGER_INFO(logger_, "do not start listening for terminal connections (offline mode selected)");
   }
}

void HeadlessAppObj::stop()
{
   guiConnection_.reset();
   guiListener_->resetConnection();

   // Send message that server has stopped
   terminalListener_->disconnect();
   terminalConnection_.reset();
   terminalListener_->resetConnection(nullptr);
}

void HeadlessAppObj::startInterface()
{
   std::vector<std::string> args;
   BinaryData serverIDKey(BIP151PUBKEYSIZE);
   switch (settings_->runMode()) {
   case bs::signer::RunMode::headless:
      logger_->debug("[{}] no interface in headless mode", __func__);
      return;
   case bs::signer::RunMode::cli:
      logger_->warn("[{}] cli run mode is not supported yet"
         , __func__);
      return;
   case bs::signer::RunMode::lightgui:
      serverIDKey = guiListener_->getServerConn()->getOwnPubKey();
      logger_->debug("[{}] starting lightgui", __func__);
      args.push_back("--guimode");
      args.push_back("lightgui");
      args.push_back("--server_id_key");
      args.push_back(serverIDKey.toHexStr());
      break;
   case bs::signer::RunMode::fullgui:
      serverIDKey = guiListener_->getServerConn()->getOwnPubKey();
      logger_->debug("[{}] starting fullgui", __func__);
      args.push_back("--guimode");
      args.push_back("fullgui");
      args.push_back("--server_id_key");
      args.push_back(serverIDKey.toHexStr());
      break;
   }

   if (settings_->testNet()) {
      args.push_back("--testnet");
   }

#ifdef __APPLE__
   std::string guiPath = SystemFilePaths::applicationDirIfKnown()
      + "/Blocksettle Signer Gui.app/Contents/MacOS/Blocksettle Signer GUI";
#else

   std::string guiPath = SystemFilePaths::applicationDirIfKnown();

   if (guiPath.empty()) {
      guiPath = "bs_signer_gui";
   } else {
      guiPath = guiPath + "/bs_signer_gui";

#ifdef WIN32
      guiPath += ".exe";
#endif

      if (!SystemFileUtils::fileExist(guiPath)) {
         logger_->error("[{}] {} doesn't exist"
            , __func__, guiPath);
         return;
      }
   }
#endif

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

void HeadlessAppObj::startTerminalsProcessing()
{
   if (terminalConnection_) {
      logger_->debug("[{}] already online", __func__);
      return;
   }

   SPDLOG_LOGGER_INFO(logger_, "start listening for terminal connections");

   logger_->debug("Using command socket {}:{}, network {}"
      , settings_->listenAddress(), settings_->listenPort()
      , (settings_->testNet() ? "testnet" : "mainnet"));

   // Set up the connection with the terminal.
   const auto zmqContext = std::make_shared<ZmqContext>(logger_);
   std::vector<std::string> trustedTerms;
   std::string absTermCookiePath =
      SystemFilePaths::appDataLocation() + "/signerServerID";
   bool makeServerCookie = true;
   std::string ourKeyFileDir = "";
   std::string ourKeyFileName = "";

   if (settings_->getTermIDKeyStr().empty()) {
      makeServerCookie = false;
      absTermCookiePath = "";
      ourKeyFileDir = SystemFilePaths::appDataLocation();
      ourKeyFileName = "remote_signer.peers";
   }

   // The whitelisted key set depends on whether or not the signer is meant to
   // be local (signer invoked with --terminal_id_key) or remote (use a set of
   // trusted terminals).
   auto getClientIDKeys = [this]() -> ZmqBIP15XPeers {
      ZmqBIP15XPeers retKeys;

      if (settings_->getTermIDKeyStr().empty()) {
         // We're using a user-defined, whitelisted key set. Make sure the keys are
         // valid before accepting the entries.
         for (const std::string& i : settings_->trustedTerminals()) {
            const auto colonIndex = i.find(':');
            if (colonIndex == std::string::npos) {
               logger_->error("[{}] Trusted client list key entry {} is malformed."
                  , __func__, i);
               continue;
            }

            SecureBinaryData inKey;
            std::string name = i.substr(0, colonIndex);
            std::string hexValue = i.substr(colonIndex + 1);
            try {
               inKey = READHEX(hexValue);

               if (inKey.isNull()) {
                  throw std::runtime_error(fmt::format("trusted client list key entry {} has no key", i));
               }

               retKeys.push_back(ZmqBIP15XPeer(name, inKey));
            } catch (const std::exception &e) {
               logger_->error("invalid trusted terminal key found: {}: {}", hexValue, e.what());
               continue;
            }
         }
      }
      else {
         const std::string termIDKeyStr = settings_->getTermIDKeyStr();
         if (termIDKeyStr.empty()) {
            logger_->error("[{}] Local connection requested but no key is available"
               , __func__);
            return retKeys;
         }

         // We're using a cookie. Only the one key in the cookie will be trusted.
         try {
            BinaryData termIDKey = READHEX(termIDKeyStr);
            retKeys.push_back(ZmqBIP15XPeer("127.0.0.1", termIDKey));
         } catch (const std::exception &e) {
            logger_->error("[{}] Local connection requested but key is invalid: {}: {}"
               , __func__, termIDKeyStr, e.what());
            return retKeys;
         }
      }

      return retKeys;
   };

   // This would stop old server if any
   terminalConnection_ = std::make_unique<ZmqBIP15XServerConnection>(logger_, zmqContext
      , getClientIDKeys, ourKeyFileDir, ourKeyFileName, makeServerCookie, false
      , absTermCookiePath);

   terminalConnection_->setLocalHeartbeatInterval();
   if (!settings_->listenFrom().empty()) {
      terminalConnection_->setListenFrom({settings_->listenFrom()});
   }
   terminalListener_->SetLimits(settings_->limits());

   terminalListener_->resetConnection(terminalConnection_.get());

   bool result = terminalConnection_->BindConnection(settings_->listenAddress()
      , settings_->listenPort(), terminalListener_.get());

   if (!result) {
      logger_->error("Failed to bind to {}:{}"
         , settings_->listenAddress(), settings_->listenPort());

      // Abort only if lightgui used, fullgui should just show error message instead
      if (settings_->runMode() == bs::signer::RunMode::lightgui) {
         throw std::runtime_error("failed to bind listening socket");
      }
   }

   signerBindStatus_ = result ? bs::signer::BindStatus::Succeed : bs::signer::BindStatus::Failed;
}

void HeadlessAppObj::stopTerminalsProcessing()
{
   if (!terminalListener_) {
      return;
   }

   SPDLOG_LOGGER_INFO(logger_, "stop listening for terminal connections");

   // Send message that server has stopped
   terminalListener_->disconnect();
   terminalConnection_.reset();
   terminalListener_->resetConnection(nullptr);

   signerBindStatus_ = bs::signer::BindStatus::Inactive;
}

ZmqBIP15XServerConnection *HeadlessAppObj::connection() const
{
   return terminalConnection_.get();
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

void HeadlessAppObj::setLimits(bs::signer::Limits limits)
{
   terminalListener_->SetLimits(limits);
}

void HeadlessAppObj::passwordReceived(const std::string &walletId, ErrorCode result
   , const SecureBinaryData &password)
{
   if (terminalListener_) {
      terminalListener_->passwordReceived(walletId, result, password);
   }
}

void HeadlessAppObj::close()
{
   queue_->quit();
}

void HeadlessAppObj::walletsListUpdated()
{
   terminalListener_->walletsListUpdated();
}

bs::error::ErrorCode HeadlessAppObj::activateAutoSign(const std::string &walletId, bool activate, const SecureBinaryData &password)
{
   if (terminalListener_) {
      if (activate) {
         return terminalListener_->activateAutoSign(walletId, password);
      }
      else {
         return terminalListener_->deactivateAutoSign(walletId);
      }
   }
   return bs::error::ErrorCode::InternalError;
}

void HeadlessAppObj::updateSettings(const Blocksettle::Communication::signer::Settings &settings)
{
   const bool prevOffline = settings_->offline();
   const std::string prevListenAddress = settings_->listenAddress();
   const std::string prevListenPort = settings_->listenPort();
   const auto prevTrustedTerminals = settings_->trustedTerminals();
   const std::string prevListenFrom = settings_->listenFrom();

   settings_->update(settings);

   const bool needReconnect = prevOffline != settings_->offline()
         || prevListenAddress != settings_->listenAddress()
         || prevListenFrom != settings_->listenFrom()
         || prevListenPort != settings_->listenPort();

   const auto trustedTerminals = settings_->trustedTerminals();
   if (terminalConnection_ && (trustedTerminals != prevTrustedTerminals)) {
      ZmqBIP15XPeers updatedKeys;
      for (const auto &line : trustedTerminals) {
         const auto colonIndex = line.find(':');
         if (colonIndex == std::string::npos) {
            logger_->error("[{}] Trusted client list key entry ({}) is malformed"
               , __func__, line);
            continue;
         }

         try {
            std::string name = line.substr(0, colonIndex);
            const SecureBinaryData inKey = READHEX(line.substr(colonIndex + 1));
            if (inKey.isNull()) {
               throw std::invalid_argument("no or malformed key data");
            }

            updatedKeys.push_back(ZmqBIP15XPeer(name, inKey));
         }
         catch (const std::exception &e) {
            logger_->error("[{}] Trusted client list key entry ({}) has invalid key: {}"
               , __func__, line, e.what());
         }
      }

      logger_->info("[{}] Updating {} trusted keys", __func__, updatedKeys.size());
      terminalConnection_->updatePeerKeys(updatedKeys);
   }

   if (needReconnect) {
      // Stop old connection at any case (if possible)
      if (terminalConnection_) {
         stopTerminalsProcessing();
      }

      // Start new connection only if we are not offline
      if (!settings_->offline()) {
         startTerminalsProcessing();
      }

      guiListener_->sendStatusUpdate();
   }
}

/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

#include "bs_signer.pb.h"

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

   settings_->setServerIdKey(guiListener_->getServerConn()->getOwnPubKey());

   guiConnection_->setLocalHeartbeatInterval();

   int port = 0;
   bool success = false;
   int count = 0;
   while (count < 3 && !success) {
      port = 10000 + rand() % 50000;
      success = guiConnection_->BindConnection("127.0.0.1", std::to_string(port)
         , guiListener_.get());
      count += 1;
   }

   if (!success) {
      logger_->error("Failed to bind adapter connection");
      throw std::runtime_error("failed to bind adapter socket");
   }

   settings_->setInterfacePort(port);

   logger_->info("BS Signer {} started", SIGNER_VERSION_STRING);

   terminalListener_ = std::make_unique<HeadlessContainerListener>(logger_
      , walletsMgr_, queue_, settings_->getWalletsDir(), settings_->netType());
   terminalListener_->setCallbacks(guiListener_->callbacks());
}

HeadlessAppObj::~HeadlessAppObj() noexcept = default;

void HeadlessAppObj::start()
{
   logger_->debug("[{}] loading wallets from dir <{}>", __func__
      , settings_->getWalletsDir());

   if (!settings_->offline()) {
      startTerminalsProcessing();
   }
   else {
      SPDLOG_LOGGER_INFO(logger_, "do not start listening for terminal connections (offline mode selected)");
   }

   reloadWallets();
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

#if 0    //kept for reference
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
   case bs::signer::RunMode::litegui:
      logger_->debug("[{}] starting litegui", __func__);
      args.push_back("--guimode");
      args.push_back("litegui");
      break;
   case bs::signer::RunMode::fullgui:
      logger_->debug("[{}] starting fullgui", __func__);
      args.push_back("--guimode");
      args.push_back("fullgui");
      break;
   }

   BinaryData serverIDKey(BIP151PUBKEYSIZE);
   serverIDKey = guiListener_->getServerConn()->getOwnPubKey();
   args.push_back("--server_id_key");
   args.push_back(serverIDKey.toHexStr());

   assert(interfacePort_ != 0);
   args.push_back("--port");
   args.push_back(std::to_string(interfacePort_));

   if (settings_->testNet()) {
      args.push_back("--testnet");
   }

#ifdef __APPLE__
   std::string guiPath = SystemFilePaths::applicationDirIfKnown()
      + "/BlockSettle Signer Gui.app/Contents/MacOS/BlockSettle Signer GUI";
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
#endif   //0

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
      ourKeyFileDir = getOwnKeyFileDir();
      ourKeyFileName = getOwnKeyFileName();
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
   if (!settings_->acceptFrom().empty()) {
      terminalConnection_->setListenFrom({settings_->acceptFrom()});
   }
   terminalListener_->SetLimits(settings_->limits());

   terminalListener_->resetConnection(terminalConnection_.get());

   bool result = terminalConnection_->BindConnection(settings_->listenAddress()
      , std::to_string(settings_->listenPort()), terminalListener_.get());

   if (!result) {
      logger_->error("Failed to bind to {}:{}"
         , settings_->listenAddress(), settings_->listenPort());

      // Abort only if litegui used, fullgui should just show error message instead
      if (settings_->runMode() == bs::signer::RunMode::litegui) {
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

SecureBinaryData HeadlessAppObj::controlPassword() const
{
   return controlPassword_;
}

void HeadlessAppObj::setControlPassword(const SecureBinaryData &controlPassword)
{
   controlPassword_ = controlPassword;
   if (requestedNewControlPassword_) {
      requestedNewControlPassword_ = false;
      guiListener_->sendControlPasswordStatusUpdate(signer::ControlPasswordStatus::Accepted);
      terminalListener_->sendControlPasswordStatusUpdate(headless::ControlPasswordStatus::Accepted);
   }
}

bs::error::ErrorCode HeadlessAppObj::changeControlPassword(const SecureBinaryData &controlPasswordOld, const SecureBinaryData &controlPasswordNew)
{
   if (controlPasswordOld != controlPassword()) {
      return bs::error::ErrorCode::InvalidPassword;
   }
   try {
      walletsMgr_->changeControlPassword(controlPasswordOld, controlPasswordNew);
   } catch (...) {
      return bs::error::ErrorCode::InvalidPassword;
   }
   return bs::error::ErrorCode::NoError;
}

ZmqBIP15XServerConnection *HeadlessAppObj::connection() const
{
   return terminalConnection_.get();
}

BinaryData HeadlessAppObj::signerPubKey() const
{
   if (terminalConnection_) {
      return terminalConnection_->getOwnPubKey();
   }

   return ZmqBIP15XServerConnection::getOwnPubKey(getOwnKeyFileDir(), getOwnKeyFileName());
}

std::string HeadlessAppObj::getOwnKeyFileDir()
{
   return SystemFilePaths::appDataLocation();
}

std::string HeadlessAppObj::getOwnKeyFileName()
{
   return "remote_signer.peers";
}

void HeadlessAppObj::reloadWallets(const std::function<void()> &cb)
{
   walletsMgr_->reset();

   const auto &cbProgress = [this](int cur, int total) {
      logger_->debug("Loaded wallet {} of {}", cur, total);
   };

   bool ok = walletsMgr_->loadWallets(settings_->netType(), settings_->getWalletsDir()
      , controlPassword(), cbProgress);
//   settings_->setWalletsDir(walletsDir);

   if (cb) {
      cb();
   }

   if (ok) {
      if (walletsMgr_->empty()) {
         logger_->warn("No wallets loaded");
         if (controlPassword().getSize() == 0) {
            guiListener_->sendControlPasswordStatusUpdate(signer::ControlPasswordStatus::RequestedNew);
            terminalListener_->sendControlPasswordStatusUpdate(headless::ControlPasswordStatus::RequestedNew);
            requestedNewControlPassword_ = true;
         }
      }
      else {
         logger_->debug("Loaded {} wallet[s]", walletsMgr_->getHDWalletsCount());
         guiListener_->sendControlPasswordStatusUpdate(signer::ControlPasswordStatus::Accepted);
         terminalListener_->sendControlPasswordStatusUpdate(headless::ControlPasswordStatus::Accepted);
      }
   }
   else {
      // wallets not loaded if control password wrong
      // send message to gui to request it
      logger_->warn("Control password required to decrypt wallets. Sending message to GUI");
      guiListener_->sendControlPasswordStatusUpdate(signer::ControlPasswordStatus::Rejected);
      terminalListener_->sendControlPasswordStatusUpdate(headless::ControlPasswordStatus::Rejected);
   }
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
   const auto prevListenPort = settings_->listenPort();
   const auto prevTrustedTerminals = settings_->trustedTerminals();
   const std::string prevListenFrom = settings_->acceptFrom();

   settings_->update(settings);

   const bool needReconnect = prevOffline != settings_->offline()
         || prevListenAddress != settings_->listenAddress()
         || prevListenFrom != settings_->acceptFrom()
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

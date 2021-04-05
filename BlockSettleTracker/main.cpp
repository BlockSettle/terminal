/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <btc/ecc.h>
#include <cxxopts.hpp>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "ArmoryConnection.h"
#include "Bip15xServerConnection.h"
#include "BsTrackerVersion.h"
#include "ColoredCoinServer.h"
#include "TransportBIP15xServer.h"
#include "WsServerConnection.h"

int main(int argc, char** argv) {
   auto logger = spdlog::stdout_color_mt("stdout logger");

   bool help{};
   std::string logfile;

   std::string listenAddress;
   uint16_t listenPort{};
   bool testnet{};

   std::string ownKeyPath;
   std::string ownKeyName;

   std::string armoryHost;
   uint16_t armoryPort{};
   std::string armoryKey;
   BinaryData armoryKeyParsed;

   cxxopts::Options options("blocksettle_tracker", "Caching tracker server for ArmoryDB");
   options.add_options()
      ("h,help", "Print help"
         , cxxopts::value<bool>(help))
      ("logfile", "Output log file (rotated daily). Default is stdout"
         , cxxopts::value<std::string>(logfile))
      ("listen_addr", "IP address to listen on"
         , cxxopts::value<std::string>(listenAddress)->default_value("0.0.0.0"))
      ("listen_port", "IP port to listen on"
         , cxxopts::value<uint16_t>(listenPort))
      ("own_key_path", "Path to own key file"
         , cxxopts::value<std::string>(ownKeyPath))
      ("own_key_name", "Own key file name"
         , cxxopts::value<std::string>(ownKeyName)->default_value("tracker.peers"))
      ("armory_host", "ArmoryDB host"
         , cxxopts::value<std::string>(armoryHost))
      ("armory_port", "ArmoryDB port"
         , cxxopts::value<uint16_t>(armoryPort))
      ("armory_key", "ArmoryDB key. Set to '-' to ignore"
         , cxxopts::value<std::string>(armoryKey))
      ("testnet", "Set bitcoin network type to testnet (default mainnet)."
         , cxxopts::value<bool>(testnet))
   ;

   try {
      const auto result = options.parse(argc, argv);

      if (armoryKey != "-") {
         armoryKeyParsed = BinaryData::CreateFromHex(armoryKey);
      }
   }
   catch(const std::exception& e) {
      SPDLOG_LOGGER_CRITICAL(logger, "parsing args failed: {}", e.what());
      exit(EXIT_FAILURE);
   }

   if (help) {
      std::cout << options.help() << std::endl;
      exit(EXIT_SUCCESS);
   }

   if (ownKeyPath.empty()) {
      SPDLOG_LOGGER_CRITICAL(logger, "please set own key path");
      exit(EXIT_FAILURE);
   }

   if (!logfile.empty()) {
      auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      consoleSink->set_level(spdlog::level::critical);

      auto fileSink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logfile, 0, 0);
      fileSink->set_level(spdlog::level::debug);

      logger = std::make_shared<spdlog::logger>("default", std::initializer_list<spdlog::sink_ptr>{consoleSink, fileSink});
   }

   logger->set_pattern("[%D %H:%M:%S.%e] [%l](%t) %s:%#:%!: %v");
   logger->set_level(spdlog::level::debug);

   logger->debug("===============================================================================");
   logger->info("Starting tracker {}", BS_TRACKER_VERSION);
   logger->flush_on(spdlog::level::debug);

   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4);
   NetworkConfig::selectNetwork(testnet ? NETWORK_MODE_TESTNET : NETWORK_MODE_MAINNET);

   auto ownKey = bs::network::TransportBIP15xServer::getOwnPubKey_FromKeyFile(ownKeyPath, ownKeyName);
   if (ownKey.empty()) {
      SPDLOG_LOGGER_CRITICAL(logger, "can't read own key");
      exit(EXIT_FAILURE);
   }
   SPDLOG_LOGGER_INFO(logger, "own key: {}", ownKey.toHexStr());

   auto armory = std::make_shared<ArmoryConnection>(logger);

   auto armoryKeyCb = [logger, armoryKey, armoryKeyParsed](const BinaryData &key, const std::string &name) -> bool {
      SPDLOG_LOGGER_INFO(logger, "got new armory public key: {}", key.toHexStr());
      bool validKey = armoryKey == "-" || armoryKeyParsed == key;
      if (!validKey) {
         SPDLOG_LOGGER_CRITICAL(logger, "please submit valid armory key");
         exit(EXIT_FAILURE);
      }
      return validKey;
   };

   // Use ownKeyPath as the data dir
   armory->setupConnection(testnet ? NetworkType::TestNet : NetworkType::MainNet, armoryHost, std::to_string(armoryPort), ownKeyPath, true, armoryKeyCb);
   auto now = std::chrono::steady_clock::now();
   while (std::chrono::steady_clock::now() - now < std::chrono::seconds(60) && armory->state() != ArmoryState::Connected) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
   }

   if (armory->state() != ArmoryState::Connected) {
      SPDLOG_LOGGER_CRITICAL(logger, "can't connect to armory, quit now");
      exit(EXIT_FAILURE);
   }

   bool result = armory->goOnline();
   if (!result) {
      SPDLOG_LOGGER_CRITICAL(logger, "ArmoryConnection::goOnline call failed");
      exit(EXIT_FAILURE);
   }

   auto cbTrustedClients = []() -> bs::network::BIP15xPeers{
      return {};
   };
   auto wsServer = std::make_unique<WsServerConnection>(logger, WsServerConnectionParams{});
   const auto ephemeralPeersServer = false; // Required for persistent server's public key
   const auto &transport = std::make_shared<bs::network::TransportBIP15xServer>(logger
      , cbTrustedClients, ephemeralPeersServer, bs::network::BIP15xAuthMode::OneWay, ownKeyPath, ownKeyName);
   auto bipServer = std::make_shared<Bip15xServerConnection>(logger, std::move(wsServer), transport);

   auto ccServer = std::make_unique<CcTrackerServer>(logger, armory, bipServer);

   result = bipServer->BindConnection(listenAddress, std::to_string(listenPort), ccServer.get());
   if (!result) {
      SPDLOG_LOGGER_CRITICAL(logger, "starting server failed");
      exit(EXIT_FAILURE);
   }

   while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      if (armory->state() != ArmoryState::Ready && armory->state() != ArmoryState::Connected) {
         SPDLOG_LOGGER_CRITICAL(logger, "connection to armory closed unexpectedly");
         exit(EXIT_FAILURE);
      }
   }
}

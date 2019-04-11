#include <spdlog/spdlog.h>
#include <fstream>
#include "BtcDefinitions.h"
#include "BlockDataManagerConfig.h"
#include "BtcUtils.h"
#include "cxxopts.hpp"
#include "HeadlessSettings.h"
#include "INIReader.h"
#include "SystemFileUtils.h"


HeadlessSettings::HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
{
   logFile_ = SystemFilePaths::appDataLocation() + "/bs_signer.log";
}

bool HeadlessSettings::loadSettings(int argc, char **argv)
{
   const std::string iniFileName = SystemFilePaths::configDataLocation() + "/signer.ini";
   INIReader iniReader(iniFileName);
   if (iniReader.ParseError() != 0) {
      return false;
   }

   watchOnly_ = iniReader.GetBoolean("General", "WatchinOnly", false);
   testNet_ = iniReader.GetBoolean("General", "TestNet", false);
   walletsDir_ = iniReader.Get("General", "WalletsDir", walletsDir_);
   logFile_ = iniReader.Get("General", "LogFileName", logFile_);
   listenAddress_ = iniReader.Get("General", "ListenAddress", listenAddress_);
   listenPort_ = iniReader.Get("General", "ListenPort", listenPort_);
   autoSignSpendLimit_ = iniReader.GetReal("Limits", "AutoSign\\XBT", 0);

   auto termKey = iniReader.Get("General", "TrustedTerminals", "");
   if (!termKey.empty()) {
      if ((*(termKey.begin()) == '"') && (*(termKey.rbegin()) == '"')) {
         termKey = termKey.substr(1, termKey.length() - 2);
      }
      trustedTerminals_.push_back(termKey);
   }

   cxxopts::Options options("BlockSettle Signer", "Headless Signer process");
   std::string guiMode;
   options.add_options()
      ("h,help", "Print help")
      ("a,listen", "IP address to listen on", cxxopts::value<std::string>(listenAddress_)->default_value(listenAddress_))
      ("p,port", "Listen port for terminal connections", cxxopts::value<std::string>(listenPort_)->default_value(listenPort_))
      ("l,log", "Log file name", cxxopts::value<std::string>(logFile_)->default_value(logFile_))
      ("d,dirwallets", "Directory where wallets reside", cxxopts::value<std::string>(walletsDir_))
      ("testnet", "Set bitcoin network type to testnet", cxxopts::value<bool>()->default_value("false"))
      ("mainnet", "Set bitcoin network type to mainnet", cxxopts::value<bool>()->default_value("true"))
      ("watchonly", "Try to load only watching-only wallets", cxxopts::value<bool>(watchOnly_))
      ("auto_sign_spend_limit", "Spend limit expressed in XBT for auto-sign operations"
         , cxxopts::value<double>(autoSignSpendLimit_))
      ("g,guimode", "GUI run mode", cxxopts::value<std::string>(guiMode)->default_value("fullgui"))
      ;

   const auto result = options.parse(argc, argv);

   if (result.count("help")) {
      std::cout << options.help({ "" }) << std::endl;
      exit(0);
   }

   if (result.count("mainnet")) {
      testNet_ = false;
   }
   else if (result.count("testnet")) {
      testNet_ = true;
   }

   if (guiMode == "lightgui") {
      runMode_ = bs::signer::RunMode::lightgui;
   }
   else if (guiMode == "fullgui") {
      runMode_ = bs::signer::RunMode::fullgui;
   }
   else if (guiMode == "headless") {
      runMode_ = bs::signer::RunMode::headless;
   }

   NetworkConfig config;
   if (testNet()) {
      config.selectNetwork(NETWORK_MODE_TESTNET);
   } else {
      config.selectNetwork(NETWORK_MODE_MAINNET);
   }
   return true;
}

NetworkType HeadlessSettings::netType() const
{
   if (testNet()) {
      return NetworkType::TestNet;
   }
   return NetworkType::MainNet;
}

std::string HeadlessSettings::getWalletsDir() const
{
   if (!walletsDir_.empty()) {
      return walletsDir_;
   }
   const auto commonRoot = SystemFilePaths::appDataLocation();
   const std::string testnetSubdir = "testnet3";

   std::string result = testNet() ? commonRoot + "/" + testnetSubdir : commonRoot;
   result += "/signer";
   return result;
}

bs::signer::Limits HeadlessSettings::limits() const
{
   bs::signer::Limits result;
   result.autoSignSpendXBT = autoSignSpendLimit_ * BTCNumericTypes::BalanceDivider;
   return result;
}

std::vector<std::string> HeadlessSettings::trustedInterfaces() const
{
   std::vector<std::string> result;
   const auto pubKeyFileName = SystemFilePaths::appDataLocation() + "/interface.pub";
   if (SystemFileUtils::fileExist(pubKeyFileName)) {
      std::stringstream ss;
      ss << "local:";
      std::ifstream src(pubKeyFileName);
      ss << src.rdbuf();
      result.push_back(ss.str());
   }
   return result;
}

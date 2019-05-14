#include <spdlog/spdlog.h>
#include <fstream>

#include "BIP150_151.h"
#include "BtcDefinitions.h"
#include "BlockDataManagerConfig.h"
#include "BtcUtils.h"
#include "cxxopts.hpp"
#include "HeadlessSettings.h"
#include "SystemFileUtils.h"
#include "google/protobuf/util/json_util.h"
#include "bs_signer.pb.h"

namespace {

   std::string getDefaultWalletsDir(bool testNet)
   {
      const auto commonRoot = SystemFilePaths::appDataLocation();
      const std::string testnetSubdir = "testnet3";

      std::string result = testNet ? commonRoot + "/" + testnetSubdir : commonRoot;
      result += "/signer";
      return result;
   }

} // namespace
HeadlessSettings::HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , d_(new Settings)
{
   logFile_ = SystemFilePaths::appDataLocation() + "/bs_signer.log";
}

HeadlessSettings::~HeadlessSettings() noexcept = default;

bool HeadlessSettings::loadSettings(int argc, char **argv)
{
   const std::string jsonFileName = SystemFilePaths::appDataLocation() + "/signer.json";
   logger_->debug("Loading settings from {}", jsonFileName);

   if (SystemFileUtils::fileExist(jsonFileName)) {
      bool result = loadSettings(d_.get(), jsonFileName);
      if (!result) {
         logger_->error("Parsing settings file failed: {}", jsonFileName);
         return false;
      }
   }

   std::string listenAddress;
   int listenPort;
   double autoSignSpendLimit;
   std::string walletsDir;

   cxxopts::Options options("BlockSettle Signer", "Headless Signer process");
   std::string guiMode;
   options.add_options()
      ("h,help", "Print help")
      ("a,listen", "IP address to listen on"
         , cxxopts::value<std::string>(listenAddress))
      ("p,port", "Listen port for terminal connections"
         , cxxopts::value<int>(listenPort))
      ("d,dirwallets", "Directory where wallets reside"
         , cxxopts::value<std::string>(walletsDir))
      ("terminal_id_key", "Set terminal BIP 150 ID key"
         , cxxopts::value<std::string>(termIDKeyStr_))
      ("testnet", "Set bitcoin network type to testnet"
         , cxxopts::value<bool>()->default_value("false"))
      ("mainnet", "Set bitcoin network type to mainnet"
         , cxxopts::value<bool>()->default_value("true"))
      ("auto_sign_spend_limit", "Spend limit expressed in XBT for auto-sign operations"
         , cxxopts::value<double>(autoSignSpendLimit))
      ("g,guimode", "GUI run mode"
         , cxxopts::value<std::string>(guiMode)->default_value("fullgui"))
      ;

   try {
      const auto result = options.parse(argc, argv);

      if (result.count("help")) {
         std::cout << options.help({ "" }) << std::endl;
         exit(0);
      }

      if (result.count("mainnet")) {
         d_->set_test_net(false);
      }
      else if (result.count("testnet")) {
         d_->set_test_net(true);
      }

      if (result.count("listen")) {
         d_->set_listen_address(listenAddress);
      }

      if (result.count("port")) {
         d_->set_listen_port(listenPort);
      }

      if (result.count("auto_sign_spend_limit")) {
         d_->set_limit_auto_sign_xbt(uint64_t(autoSignSpendLimit * BTCNumericTypes::BalanceDivider));
      }

      if (result.count("dirwallets")) {
         walletsDir_ = walletsDir;
      } else {
         walletsDir_ = getDefaultWalletsDir(d_->test_net());
      }
   }
   catch(const std::exception& e) {
      // The logger should still be outputting to stdout at this point. Still,
      // in case this changes, output help directly to stdout.
      std::cout << options.help({ "" }) << std::endl;
      logger_->warn("[{}] Signer option error: {}", __func__, e.what());
      logger_->warn("[{}] Signer will now exit.", __func__);
      exit(0);
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

   if (testNet()) {
      NetworkConfig::selectNetwork(NETWORK_MODE_TESTNET);
   } else {
      NetworkConfig::selectNetwork(NETWORK_MODE_MAINNET);
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

bool HeadlessSettings::testNet() const
{
   return d_->test_net();
}

// Get the terminal (client) BIP 150 ID key. Intended only for when the key is
// passed in via a CL arg.
//
// INPUT:  N/A
// OUTPUT: A buffer containing the binary key. (BinaryData)
// RETURN: True is success, false if failure.
bool HeadlessSettings::getTermIDKeyBin(BinaryData& keyBuf)
{
   try {
      keyBuf.resize(BIP151PUBKEYSIZE);
      if (termIDKeyStr_.empty()) {
         logger_->error("[{}] No terminal BIP 150 ID key is available.", __func__);
         return false;
      }

      // Make sure the key is a valid public key.
      keyBuf = READHEX(termIDKeyStr_);
      if (keyBuf.getSize() != BIP151PUBKEYSIZE) {
         logger_->error("[{}] Terminal BIP 150 ID key is not {} bytes).", __func__
            , BIP151PUBKEYSIZE);
         return false;
      }
      if (!(CryptoECDSA().VerifyPublicKeyValid(keyBuf))) {
         logger_->error("[{}] Terminal BIP 150 ID key ({}) is not a valid "
            "secp256k1 compressed public key.", __func__, termIDKeyStr_);
         return false;
      }

      return true;
   } catch (const std::exception &e) {
      logger_->error("[{}] Terminal BIP 150 ID key ({}) is not valid: {}"
         , __func__, e.what());
      return false;
   }
}

std::vector<std::string> HeadlessSettings::trustedTerminals() const
{
   std::vector<std::string> result;
   for (const auto& item : d_->trusted_terminals()) {
      result.push_back(item.id() + ":" + item.key());
   }
   return result;
}

std::string HeadlessSettings::listenAddress() const
{
   if (d_->listen_address().empty()) {
      return "0.0.0.0";
   }
   return d_->listen_address();
}

std::string HeadlessSettings::listenPort() const
{
   if (d_->listen_port() == 0) {
      return "23456";
   }
   return std::to_string(d_->listen_port());
}

bs::signer::Limits HeadlessSettings::limits() const
{
   bs::signer::Limits result;
   result.autoSignSpendXBT = d_->limit_auto_sign_xbt();
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

bool HeadlessSettings::loadSettings(HeadlessSettings::Settings *settings, const std::string &fileName)
{
   std::ifstream s(fileName);
   std::stringstream buffer;
   buffer << s.rdbuf();

   auto status = google::protobuf::util::JsonStringToMessage(buffer.str(), settings);
   return status.ok();
}

bool HeadlessSettings::saveSettings(const HeadlessSettings::Settings &settings, const std::string &fileName)
{
   std::string out;
   google::protobuf::util::JsonOptions options;
   options.add_whitespace = true;
   google::protobuf::util::MessageToJsonString(settings, &out, options);

   std::ofstream s(fileName);
   s << out;
   s.close();
   return s.good();
}

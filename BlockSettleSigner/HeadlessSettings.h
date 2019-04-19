#ifndef __HEADLESS_SETTINGS_H__
#define __HEADLESS_SETTINGS_H__

#include <memory>
#include "BtcDefinitions.h"
#include "SignerDefs.h"

namespace spdlog {
   class logger;
}

class HeadlessSettings
{
public:
   HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger);
   ~HeadlessSettings() noexcept = default;

   bool loadSettings(int argc, char **argv);

   NetworkType netType() const;
   bool testNet() const { return testNet_; }
   bs::signer::Limits limits() const;
   std::string getWalletsDir() const;
   std::string listenAddress() const { return listenAddress_; }
   std::string listenPort() const { return listenPort_; }
   std::string interfacePort() const { return interfacePort_; }
   std::string logFile() const { return logFile_; }
   std::vector<std::string> trustedTerminals() const { return trustedTerminals_; }
   std::vector<std::string> trustedInterfaces() const;

   bs::signer::RunMode runMode() const { return runMode_; }

private:
   // since INIReader doesn't support stringlist settings, iniStringToStringList do that
   // valstr is a string taken from INI file, and contains comma separated values.
   std::vector<std::string> iniStringToStringList(std::string valstr) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;

   bool     testNet_ = false;
   double   autoSignSpendLimit_ = 0;
   std::string logFile_;
   std::string walletsDir_;
   std::string listenAddress_ = "0.0.0.0";
   std::string listenPort_ = "23456";
   std::string interfacePort_ = "23457";
   std::vector<std::string>   trustedTerminals_;
   bs::signer::RunMode runMode_;
};


#endif // __HEADLESS_SETTINGS_H__

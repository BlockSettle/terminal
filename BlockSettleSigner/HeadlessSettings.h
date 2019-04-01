#ifndef __HEADLESS_SETTINGS_H__
#define __HEADLESS_SETTINGS_H__

#include "BtcDefinitions.h"
#include "SettingsParser.h"
#include "SignContainer.h"
#include "SignerUiDefs.h"

namespace spdlog {
   class logger;
}

class HeadlessSettings
{
   Q_GADGET
public:
   HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger);
   ~HeadlessSettings() noexcept = default;

   bool loadSettings(const QStringList &);

   NetworkType netType() const;
   bool testNet() const { return testNet_; }
   SignContainer::Limits limits() const;
   std::string getWalletsDir() const;
   bool watchingOnly() const { return watchOnly_; }
   std::string listenAddress() const { return listenAddress_; }
   std::string listenPort() const { return listenPort_; }
   std::string interfacePort() const { return interfacePort_; }
   std::string logFile() const { return logFile_; }
   QStringList trustedTerminals() const { return trustedTerminals_; }
   QStringList trustedInterfaces() const;

   SignerUiDefs::SignerRunMode runMode() const { return runMode_; }

private:
   std::shared_ptr<spdlog::logger>  logger_;

   bool     testNet_ = false;
   bool     watchOnly_ = false;
   double   autoSignSpendLimit_ = 0;
   std::string logFile_;
   std::string walletsDir_;
   std::string listenAddress_ = "0.0.0.0";
   std::string listenPort_ = "23456";
   std::string interfacePort_ = "23457";
   QStringList trustedTerminals_;
   SignerUiDefs::SignerRunMode runMode_;
};


#endif // __HEADLESS_SETTINGS_H__

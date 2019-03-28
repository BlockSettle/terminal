#ifndef __HEADLESS_SETTINGS_H__
#define __HEADLESS_SETTINGS_H__

#include "BtcDefinitions.h"
#include "SettingsParser.h"
#include "SignContainer.h"

namespace spdlog {
   class logger;
}

class HeadlessSettings
{
public:
   HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger);
   ~HeadlessSettings() noexcept = default;

   bool loadSettings(const QStringList &);

   NetworkType netType() const;
   bool testNet() const { return testNet_; }
   SignContainer::Limits limits() const;
   std::string getWalletsDir() const;
   QString zmqPubKeyFile() const { return zmqPubFile_; }
   QString zmqPrvKeyFile() const { return zmqPrvFile_; }
   bool watchingOnly() const { return watchOnly_; }
   std::string listenAddress() const { return listenAddress_; }
   std::string listenPort() const { return listenPort_; }
   std::string logFile() const { return logFile_; }
   QStringList trustedTerminals() const { return trustedTerminals_; }

   enum class RunMode {
      headless,
      QmlGui,
      LightGui,
      CLI
   };
   RunMode runMode() const;

private:
   std::shared_ptr<spdlog::logger>  logger_;

   bool     testNet_ = false;
   bool     watchOnly_ = false;
   bool     headless_ = false;
   double   autoSignSpendLimit_ = 0;
   std::string logFile_;
   std::string walletsDir_;
   std::string listenAddress_ = "0.0.0.0";
   std::string listenPort_ = "23456";
   QString     zmqPubFile_;
   QString     zmqPrvFile_;
   QStringList trustedTerminals_;
};


#endif // __HEADLESS_SETTINGS_H__

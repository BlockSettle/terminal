#ifndef __HEADLESS_SETTINGS_H__
#define __HEADLESS_SETTINGS_H__

#include <memory>
#include "BtcDefinitions.h"
#include "SignerDefs.h"
#include <SettableField.h>

namespace spdlog {
   class logger;
}

namespace Blocksettle { namespace Communication { namespace signer {
   class Settings;
} } }

class HeadlessSettings
{
public:
   using Settings = Blocksettle::Communication::signer::Settings;

   HeadlessSettings(const std::shared_ptr<spdlog::logger> &logger);
   ~HeadlessSettings() noexcept;

   bool loadSettings(int argc, char **argv);

   NetworkType netType() const;
   bool testNet() const;
   bs::signer::Limits limits() const;
   std::string getWalletsDir() const { return walletsDir_; }
   std::string listenAddress() const;
   std::string acceptFrom() const;
   std::string listenPort() const;
   std::string interfacePort() const { return "23457"; }
   std::string getTermIDKeyStr() const { return termIDKeyStr_; }
   bool getTermIDKeyBin(BinaryData& keyBuf);
   std::string logFile() const { return logFile_; }
   std::vector<std::string> trustedTerminals() const;
   std::vector<std::string> trustedInterfaces() const;
   bool twoWaySignerAuth() const;
   bool offline() const;

   bs::signer::RunMode runMode() const { return runMode_; }

   void update(const Settings&);

   static bool loadSettings(Settings *settings, const std::string &fileName);
   static bool saveSettings(const Settings &settings, const std::string &fileName);
private:
   std::shared_ptr<spdlog::logger>  logger_;

   std::string logFile_;
   std::string termIDKeyStr_;
   bs::signer::RunMode runMode_;
   std::string walletsDir_;
   std::unique_ptr<Settings> d_;

   SettableField<bool> overrideTestNet_;
   SettableField<std::string> overrideListenAddress_;
   SettableField<std::string> overrideAcceptFrom_;
   SettableField<int> overrideListenPort_;
   SettableField<uint64_t> overrideAutoSignXbt_;
};


#endif // __HEADLESS_SETTINGS_H__

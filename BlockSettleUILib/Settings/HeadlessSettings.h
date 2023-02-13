/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __HEADLESS_SETTINGS_H__
#define __HEADLESS_SETTINGS_H__

#include <memory>
#include "BtcDefinitions.h"
#include "Wallets/SignerDefs.h"
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
   int listenPort() const;
   std::string getTermIDKeyStr() const { return termIDKeyStr_; }
   bool getTermIDKeyBin(BinaryData& keyBuf);
   std::string logFile() const { return logFile_; }
   std::vector<std::string> trustedTerminals() const;
   std::vector<std::string> trustedInterfaces() const;
   bool twoWaySignerAuth() const;
   bool offline() const;

   BinaryData serverIdKey() const { return serverIdKey_; }
   void setServerIdKey(const BinaryData &key) { serverIdKey_ = key; }

   int interfacePort() const { return interfacePort_; }
   void setInterfacePort(int port) { interfacePort_ = port; }

   void update(const Settings&);

   static bool loadSettings(Settings *settings, const std::string &fileName);
   static bool saveSettings(const Settings &settings, const std::string &fileName);

private:
   std::shared_ptr<spdlog::logger>  logger_;

   std::string logFile_;
   std::string termIDKeyStr_;
   std::string walletsDir_;
   std::unique_ptr<Settings> d_;
   BinaryData  serverIdKey_;
   int interfacePort_{};

   SettableField<bool> overrideTestNet_;
   SettableField<std::string> overrideListenAddress_;
   SettableField<std::string> overrideAcceptFrom_;
   SettableField<int> overrideListenPort_;
   SettableField<uint64_t> overrideAutoSignXbt_;
};


#endif // __HEADLESS_SETTINGS_H__

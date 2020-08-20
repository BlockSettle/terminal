/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SETTINGS_ADAPTER_H
#define SETTINGS_ADAPTER_H

#include <QObject>
#include <QStringList>
#include "Message/Adapter.h"
#include "TerminalMessage.h"

namespace spdlog {
   class logger;
}
namespace bs {
   class LogManager;
   class TradeSettings;
}
namespace BlockSettle {
   namespace Terminal {
      class SettingsMessage_ArmoryServerSet;
      class SettingsMessage_SettingsRequest;
      class SettingsMessage_SettingsResponse;
      class SettingsMessage_SignerSetKey;
   }
}
class ApplicationSettings;
class ArmoryServersProvider;
class CCFileManager;
class SignersProvider;


class SettingsAdapter : public bs::message::Adapter
{
public:
   SettingsAdapter(const std::shared_ptr<ApplicationSettings> &
      , const QStringList &appArgs);
   ~SettingsAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Settings"; }

   std::shared_ptr<bs::LogManager> logManager() const { return logMgr_; }

private:
   bool processGetRequest(const bs::message::Envelope &
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest &);
   bool processPutRequest(const BlockSettle::Terminal::SettingsMessage_SettingsResponse &);
   bool processArmoryServer(const BlockSettle::Terminal::SettingsMessage_ArmoryServerSet &);
   bool processSignerSettings(const bs::message::Envelope &);
   bool processSignerSetKey(const BlockSettle::Terminal::SettingsMessage_SignerSetKey &);
   bool processSignerReset();
   bool processRemoteSettings(uint64_t msgId);

private:
   std::shared_ptr<bs::message::User>  user_, userBC_;
   std::shared_ptr<bs::LogManager>     logMgr_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::shared_ptr<CCFileManager>         ccFileManager_;
   std::shared_ptr<bs::TradeSettings>     tradeSettings_;

   std::map<uint64_t, bs::message::Envelope> remoteSetReqs_;
};


#endif	// SETTINGS_ADAPTER_H

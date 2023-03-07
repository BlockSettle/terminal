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
#include <QVariant>
#include "Message/Adapter.h"
#include "TerminalMessage.h"

namespace spdlog {
   class logger;
}
namespace bs {
   class LogManager;
   struct TradeSettings;
}
namespace BlockSettle {
   namespace Terminal {
      class SettingsMessage_ArmoryServer;
      class SettingsMessage_ArmoryServerUpdate;
      class SettingsMessage_SettingsRequest;
      class SettingsMessage_SettingsResponse;
      class SettingsMessage_SignerServer;
      class SettingsMessage_SignerSetKey;
      class SettingRequest;
      class SettingResponse;
      enum SettingType : int;
   }
}
class ApplicationSettings;
class ArmoryServersProvider;
class BootstrapDataManager;
class CCFileManager;
class SignersProvider;

namespace bs {
   namespace message {
      void setFromQVariant(const QVariant &, BlockSettle::Terminal::SettingRequest *
         , BlockSettle::Terminal::SettingResponse *);
      QVariant fromResponse(const BlockSettle::Terminal::SettingResponse &);
   }
}

class SettingsAdapter : public bs::message::Adapter
{
public:
   SettingsAdapter(const std::shared_ptr<ApplicationSettings> &
      , const QStringList &appArgs);
   ~SettingsAdapter() override = default;

   bs::message::ProcessingResult process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "Settings"; }

   std::shared_ptr<bs::LogManager> logManager() const { return logMgr_; }
   std::string guiMode() const;

private:
   bs::message::ProcessingResult processGetRequest(const bs::message::Envelope &
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest &);
   bs::message::ProcessingResult processPutRequest(const BlockSettle::Terminal::SettingsMessage_SettingsResponse &);
   bs::message::ProcessingResult processArmoryServer(const BlockSettle::Terminal::SettingsMessage_ArmoryServer &);
   bs::message::ProcessingResult processSetArmoryServer(const bs::message::Envelope&, int index);
   bs::message::ProcessingResult processGetArmoryServers(const bs::message::Envelope&);
   bs::message::ProcessingResult processAddArmoryServer(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_ArmoryServer&);
   bs::message::ProcessingResult processDelArmoryServer(const bs::message::Envelope&, int index);
   bs::message::ProcessingResult processUpdArmoryServer(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_ArmoryServerUpdate&);
   bs::message::ProcessingResult processSignerSettings(const bs::message::Envelope&);
   bs::message::ProcessingResult processRemoteSettings(uint64_t msgId);
   bs::message::ProcessingResult processGetState(const bs::message::Envelope&);
   bs::message::ProcessingResult processReset(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest&);
   bs::message::ProcessingResult processResetToState(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bs::message::ProcessingResult processBootstrap(const bs::message::Envelope&, const std::string&);
   bs::message::ProcessingResult processApiPrivKey(const bs::message::Envelope&);
   bs::message::ProcessingResult processApiClientsList(const bs::message::Envelope&);

private:
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<bs::LogManager>     logMgr_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<BootstrapDataManager>  bootstrapDataManager_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   //std::shared_ptr<CCFileManager>         ccFileManager_;
   std::shared_ptr<bs::TradeSettings>     tradeSettings_;

   std::map<uint64_t, bs::message::Envelope> remoteSetReqs_;
};


#endif	// SETTINGS_ADAPTER_H

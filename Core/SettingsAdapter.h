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

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "Settings"; }

   std::shared_ptr<bs::LogManager> logManager() const { return logMgr_; }

private:
   bool processGetRequest(const bs::message::Envelope &
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest &);
   bool processPutRequest(const BlockSettle::Terminal::SettingsMessage_SettingsResponse &);
   bool processArmoryServer(const BlockSettle::Terminal::SettingsMessage_ArmoryServer &);
   bool processSetArmoryServer(const bs::message::Envelope&, int index);
   bool processGetArmoryServers(const bs::message::Envelope&);
   bool processAddArmoryServer(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_ArmoryServer&);
   bool processDelArmoryServer(const bs::message::Envelope&, int index);
   bool processUpdArmoryServer(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_ArmoryServerUpdate&);
   bool processSignerSettings(const bs::message::Envelope &);
   bool processSignerSetKey(const BlockSettle::Terminal::SettingsMessage_SignerSetKey &);
   bool processSignerReset();
   bool processGetSigners(const bs::message::Envelope&);
   bool processSetSigner(const bs::message::Envelope&, int);
   bool processAddSigner(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_SignerServer&);
   bool processDelSigner(const bs::message::Envelope&, int);
   bool processRemoteSettings(uint64_t msgId);
   bool processGetState(const bs::message::Envelope&);
   bool processReset(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_SettingsRequest&);
   bool processResetToState(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bool processBootstrap(const bs::message::Envelope&, const std::string&);
   bool processApiPrivKey(const bs::message::Envelope&);
   bool processApiClientsList(const bs::message::Envelope&);

private:
   std::shared_ptr<bs::message::User>  user_;
   std::shared_ptr<bs::LogManager>     logMgr_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<BootstrapDataManager>  bootstrapDataManager_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::shared_ptr<CCFileManager>         ccFileManager_;
   std::shared_ptr<bs::TradeSettings>     tradeSettings_;

   std::map<uint64_t, bs::message::Envelope> remoteSetReqs_;
};


#endif	// SETTINGS_ADAPTER_H

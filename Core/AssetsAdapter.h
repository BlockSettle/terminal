/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ASSETS_ADAPTER_H
#define ASSETS_ADAPTER_H

#include "Message/Adapter.h"
#include "AssetManager.h"
#include "CCFileManager.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class MatchingMessage_LoggedIn;
      class MatchingMessage_SubmittedAuthAddresses;
      class SettingsMessage_BootstrapData;
      class SettingsMessage_SettingsResponse;
   }
}

class AssetsAdapter : public bs::message::Adapter
   , public AssetCallbackTarget, public CCCallbackTarget
{
public:
   AssetsAdapter(const std::shared_ptr<spdlog::logger> &);
   ~AssetsAdapter() override = default;

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Assets"; }

private:    // AssetMgr callbacks override
   void onCcPriceChanged(const std::string& currency) override;
   void onXbtPriceChanged(const std::string& currency) override;
   void onFxBalanceLoaded() override;
   void onFxBalanceCleared() override;

   void onBalanceChanged(const std::string& currency) override;
   void onTotalChanged() override;
   void onSecuritiesChanged() override;

   //CC callbacks override
   void onCCSecurityDef(const bs::network::CCSecurityDef& sd) override;
   void onLoaded() override;

   //internal processing
   bool processGetSettings(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   void onBSSignAddress(const std::string&);
   bool processBootstrap(const BlockSettle::Terminal::SettingsMessage_BootstrapData&);
   bool onMatchingLogin(const BlockSettle::Terminal::MatchingMessage_LoggedIn&);
   bool processSubmittedAuth(const BlockSettle::Terminal::MatchingMessage_SubmittedAuthAddresses&);
   bool processSubmitAuth(const std::string&);
   bool processBalance(const std::string& currency, double);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
   std::unique_ptr<AssetManager>       assetMgr_;
   std::unique_ptr<CCFileManager>      ccFileMgr_;
};


#endif	// ASSETS_ADAPTER_H

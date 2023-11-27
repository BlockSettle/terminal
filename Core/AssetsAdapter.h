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
   , public AssetCallbackTarget
{
public:
   AssetsAdapter(const std::shared_ptr<spdlog::logger> &);
   ~AssetsAdapter() override = default;

   bs::message::ProcessingResult process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "Assets"; }

private:    // AssetMgr callbacks override
   void onCcPriceChanged(const std::string& currency) override;
   void onXbtPriceChanged(const std::string& currency) override;
   void onFxBalanceLoaded() override;
   void onFxBalanceCleared() override;

   void onBalanceChanged(const std::string& currency) override;
   void onTotalChanged() override;
   void onSecuritiesChanged() override;

   //internal processing
   bs::message::ProcessingResult processGetSettings(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bool onMatchingLogin(const BlockSettle::Terminal::MatchingMessage_LoggedIn&);
   bs::message::ProcessingResult processSubmittedAuth(const BlockSettle::Terminal::MatchingMessage_SubmittedAuthAddresses&);
   bs::message::ProcessingResult processSubmitAuth(const std::string&);
   bs::message::ProcessingResult processBalance(const std::string& currency, double);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
   std::unique_ptr<AssetManager>       assetMgr_;
};

#endif	// ASSETS_ADAPTER_H

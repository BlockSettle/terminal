/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef BS_SERVER_ADAPTER_H
#define BS_SERVER_ADAPTER_H

#include "ApplicationSettings.h"
#include "FutureValue.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response_SignPayinRequest;
         class Response_SignPayoutRequest;
         class Response_UpdateOrders;
         class Response_UnsignedPayinRequest;
      }
   }
}
namespace BlockSettle {
   namespace Terminal {
      class BsServerMessage_XbtTransaction;
      class SettingsMessage_SettingsResponse;
   }
}
class ConnectionManager;
class RequestReplyCommand;

class BsServerAdapter : public bs::message::Adapter
{
public:
   BsServerAdapter(const std::shared_ptr<spdlog::logger> &);
   ~BsServerAdapter() override = default;

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "BS Servers"; }

private:
   void start();
   bool processOwnRequest(const bs::message::Envelope &);
   bool processLocalSettings(const BlockSettle::Terminal::SettingsMessage_SettingsResponse &);
   bool processPuBKeyResponse(bool);
   bool processTimeout(const std::string& id);
   bool processOpenConnection();
   bool processStartLogin(const std::string&);
   bool processCancelLogin();
   //bool processSubmitAuthAddr(const bs::message::Envelope&, const std::string &addr);
   //void processUpdateOrders(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrders&);
   //void processUnsignedPayin(const Blocksettle::Communication::ProxyTerminalPb::Response_UnsignedPayinRequest&);
   //void processSignPayin(const Blocksettle::Communication::ProxyTerminalPb::Response_SignPayinRequest&);
   //void processSignPayout(const Blocksettle::Communication::ProxyTerminalPb::Response_SignPayoutRequest&);

   //bool processOutUnsignedPayin(const BlockSettle::Terminal::BsServerMessage_XbtTransaction&);
   //bool processOutSignedPayin(const BlockSettle::Terminal::BsServerMessage_XbtTransaction&);
   //bool processOutSignedPayout(const BlockSettle::Terminal::BsServerMessage_XbtTransaction&);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userSettl_, userSettings_;
   std::shared_ptr<bs::message::User>  userMtch_, userWallets_;
   std::shared_ptr<ConnectionManager>  connMgr_;
   ApplicationSettings::EnvConfiguration  envConfig_{ ApplicationSettings::EnvConfiguration::Unknown };
   bool  connected_{ false };
   std::string currentLogin_;

   std::shared_ptr<FutureValue<bool>>     futPuBkey_;
   std::unordered_map<std::string, std::function<void()>>   timeouts_;
};


#endif	// BS_SERVER_ADAPTER_H

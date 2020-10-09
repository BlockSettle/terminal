/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MKT_DATA_ADAPTER_H
#define MKT_DATA_ADAPTER_H

#include "MarketDataProvider.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
class BSMarketDataProvider;

class MktDataAdapter : public bs::message::Adapter, public MDCallbackTarget
{
public:
   MktDataAdapter(const std::shared_ptr<spdlog::logger> &);
   ~MktDataAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "MktData"; }

protected:  //MD callbacks override
   void userWantsToConnect() override;
   void waitingForConnectionDetails() override;

   void connected() override;
   void disconnected() override;

   void onMDUpdate(bs::network::Asset::Type, const std::string&
      , bs::network::MDFields) override;
   void onMDSecurityReceived(const std::string&
      , const bs::network::SecurityDef&) override;
   void allSecuritiesReceived() override;

   void onNewFXTrade(const bs::network::NewTrade&) override;
   void onNewXBTTrade(const bs::network::NewTrade&) override;
   void onNewPMTrade(const bs::network::NewPMTrade&) override;

private:
   void sendTrade(const bs::network::NewTrade&);
   bool processStartConnection(int env);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::User>     user_;
   std::shared_ptr<BSMarketDataProvider>  mdProvider_;
   bool connected_{ false };
};


#endif	// MKT_DATA_ADAPTER_H

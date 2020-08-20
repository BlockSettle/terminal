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

protected:
   void userWantsToConnect() override;
   void waitingForConnectionDetails() override;

private:

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::User>     user_;
   std::shared_ptr<BSMarketDataProvider>  mdProvider_;
};


#endif	// MKT_DATA_ADAPTER_H

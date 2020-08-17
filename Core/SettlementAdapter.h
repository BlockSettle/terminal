/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SETTLEMENT_ADAPTER_H
#define SETTLEMENT_ADAPTER_H

#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}

class SettlementAdapter : public bs::message::Adapter
{
public:
   SettlementAdapter(const std::shared_ptr<spdlog::logger> &);
   ~SettlementAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Settlement"; }

private:

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
};


#endif	// SETTLEMENT_ADAPTER_H

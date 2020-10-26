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

#include "CommonTypes.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class AcceptRFQ;
      class MatchingMessage_Order;
      class Quote;
      class SettlementMessage_SendRFQ;
   }
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
   bool processMatchingQuote(const BlockSettle::Terminal::Quote&);
   bool processMatchingOrder(const BlockSettle::Terminal::MatchingMessage_Order&);

   bool processCancelRFQ(const std::string& rfqId);
   bool processAcceptRFQ(const bs::message::Envelope&
      , const BlockSettle::Terminal::AcceptRFQ&);
   bool processSendRFQ(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettlementMessage_SendRFQ&);

   void unreserve(const std::string& id, const std::string& subId = {});

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userMtch_, userWallets_;

   struct Settlement {
      bs::message::Envelope   env;
      bool dealer{ false };
      bs::network::RFQ     rfq;
      std::string          reserveId;
      bs::network::Quote   quote;
   };
   std::unordered_map<std::string, Settlement>  settlByRfqId_;
   std::unordered_map<std::string, Settlement>  settlByQuoteId_;
   std::unordered_map<std::string, Settlement>  settlBySettlId_;
};


#endif	// SETTLEMENT_ADAPTER_H

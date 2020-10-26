/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettlementAdapter.h"
#include <spdlog/spdlog.h>
#include "MessageUtils.h"
#include "TerminalMessage.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


SettlementAdapter::SettlementAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::Settlement))
   , userMtch_(std::make_shared<UserTerminal>(TerminalUsers::Matching))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
{}

bool SettlementAdapter::process(const bs::message::Envelope &env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id);
         return true;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         Envelope envBC{ 0, UserTerminal::create(TerminalUsers::System), nullptr
            , {}, {}, admMsg.SerializeAsString() };
         pushFill(envBC);
      }
   }
   else if (env.sender->value() == userMtch_->value()) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse matching msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kQuote:
         return processMatchingQuote(msg.quote());
      case MatchingMessage::kOrder:
         return processMatchingOrder(msg.order());
      default: break;
      }
   }
   else if (env.receiver && (env.receiver->value() == user_->value())) {
      SettlementMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own request #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case SettlementMessage::kCancelRfq:
         return processCancelRFQ(msg.cancel_rfq());
      case SettlementMessage::kAcceptRfq:
         return processAcceptRFQ(env, msg.accept_rfq());
      case SettlementMessage::kSendRfq:
         return processSendRFQ(env, msg.send_rfq());
      default:
         logger_->warn("[{}] unknown settlement request {}", __func__, msg.data_case());
         break;
      }
   }
   return true;
}

bool SettlementAdapter::processMatchingQuote(const BlockSettle::Terminal::Quote& response)
{
   const auto& itSettl = settlByRfqId_.find(response.request_id());
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, response.request_id());
      return true;
   }
   SettlementMessage msg;
   auto msgResp = msg.mutable_quote();
   *msgResp = response;
   Envelope env{ 0, user_, itSettl->second.env.sender, {}, {}
      , msg.SerializeAsString() };
   return pushFill(env);
}

bool SettlementAdapter::processMatchingOrder(const MatchingMessage_Order& response)
{
   const auto& itSettl = settlByQuoteId_.find(response.quote_id());
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, response.quote_id());
      return true;
   }
   SettlementMessage msg;
   const auto& order = fromMsg(response);
   if (order.status == bs::network::Order::Status::Filled) {
      settlBySettlId_[response.settlement_id()] = itSettl->second;
      auto msgResp = msg.mutable_matched_quote();
      msgResp->set_rfq_id(itSettl->second.rfq.requestId);
      msgResp->set_quote_id(response.quote_id());
      msgResp->set_price(response.price());
   }
   else if (order.status == bs::network::Order::Status::Failed) {
      auto msgResp = msg.mutable_failed_quote();
      msgResp->set_rfq_id(itSettl->second.rfq.requestId);
      msgResp->set_quote_id(response.quote_id());
      msgResp->set_info(order.info);
   }
   else {
      logger_->debug("[{}] {} unprocessed order status {}", __func__, order.quoteId
         , (int)order.status);
      return true;
   }
   Envelope env{ 0, user_, itSettl->second.env.sender, {}, {}
      , msg.SerializeAsString() };
   settlByQuoteId_.erase(itSettl);
   return pushFill(env);
}

bool SettlementAdapter::processCancelRFQ(const std::string& rfqId)
{
   const auto& itSettl = settlByRfqId_.find(rfqId);
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, rfqId);
      return true;
   }
   settlByRfqId_.erase(itSettl);
   unreserve(rfqId);
   MatchingMessage msg;
   msg.set_cancel_rfq(rfqId);
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

bool SettlementAdapter::processAcceptRFQ(const bs::message::Envelope& env
   , const AcceptRFQ& request)
{
   const auto& itSettl = settlByRfqId_.find(request.rfq_id());
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, request.rfq_id());
      return true;
   }
   itSettl->second.env = env;
   itSettl->second.quote = fromMsg(request.quote());
   settlByQuoteId_[request.quote().quote_id()] = itSettl->second;
   settlByRfqId_.erase(itSettl);

   MatchingMessage msg;
   auto msgReq = msg.mutable_accept_rfq();
   *msgReq = request;
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

bool SettlementAdapter::processSendRFQ(const bs::message::Envelope& env
   , const SettlementMessage_SendRFQ& request)
{
   const auto& rfq = fromMsg(request.rfq());
   settlByRfqId_[rfq.requestId] = Settlement{ env, false, rfq, request.reserve_id() };

   MatchingMessage msg;
   auto msgReq = msg.mutable_send_rfq();
   *msgReq = request.rfq();
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

void SettlementAdapter::unreserve(const std::string& id, const std::string &subId)
{
   WalletsMessage msg;
   auto msgReq = msg.mutable_unreserve_utxos();
   msgReq->set_id(id);
   msgReq->set_sub_id(subId);
   Envelope envReq{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   pushFill(envReq);
}

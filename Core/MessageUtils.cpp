/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MessageUtils.h"
#include "terminal.pb.h"

using namespace bs::message;
using namespace BlockSettle::Terminal;

bs::network::RFQ bs::message::fromMsg(const BlockSettle::Terminal::RFQ& msg)
{
   bs::network::RFQ rfq;
   rfq.requestId = msg.id();
   rfq.security = msg.security();
   rfq.product = msg.product();
   rfq.assetType = static_cast<bs::network::Asset::Type>(msg.asset_type());
   rfq.side = msg.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   rfq.quantity = msg.quantity();
   rfq.requestorAuthPublicKey = msg.auth_pub_key();
   rfq.receiptAddress = msg.receipt_address();
   rfq.coinTxInput = msg.coin_tx_input();
   return rfq;
}

void bs::message::toMsg(const bs::network::RFQ& rfq, BlockSettle::Terminal::RFQ* msg)
{
   msg->set_id(rfq.requestId);
   msg->set_security(rfq.security);
   msg->set_product(rfq.product);
   msg->set_asset_type((int)rfq.assetType);
   msg->set_buy(rfq.side == bs::network::Side::Buy);
   msg->set_quantity(rfq.quantity);
   msg->set_auth_pub_key(rfq.requestorAuthPublicKey);
   msg->set_receipt_address(rfq.receiptAddress);
   msg->set_coin_tx_input(rfq.coinTxInput);
}


bs::network::Quote bs::message::fromMsg(const BlockSettle::Terminal::Quote& msg)
{
   bs::network::Quote quote;
   quote.requestId = msg.request_id();
   quote.quoteId = msg.quote_id();
   quote.security = msg.security();
   quote.product = msg.product();
   quote.price = msg.price();
   quote.quantity = msg.quantity();
   quote.side = msg.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   quote.assetType = static_cast<bs::network::Asset::Type>(msg.asset_type());
   quote.quotingType = static_cast<bs::network::Quote::QuotingType>(msg.quoting_type());
   quote.requestorAuthPublicKey = msg.req_auth_pub_key();
   quote.dealerAuthPublicKey = msg.deal_auth_pub_key();
   quote.settlementId = msg.settlement_id();
   quote.dealerTransaction = msg.dealer_tx();
   quote.expirationTime = QDateTime::fromSecsSinceEpoch(msg.expiration_time());
   quote.timeSkewMs = msg.time_skew_ms();
   quote.celerTimestamp = msg.timestamp();
   return quote;
}

void bs::message::toMsg(const bs::network::Quote& quote, BlockSettle::Terminal::Quote* msg)
{
   msg->set_request_id(quote.requestId);
   msg->set_quote_id(quote.quoteId);
   msg->set_security(quote.security);
   msg->set_product(quote.product);
   msg->set_price(quote.price);
   msg->set_quantity(quote.quantity);
   msg->set_buy(quote.side == bs::network::Side::Buy);
   msg->set_asset_type((int)quote.assetType);
   msg->set_quoting_type((int)quote.quotingType);
   msg->set_req_auth_pub_key(quote.requestorAuthPublicKey);
   msg->set_deal_auth_pub_key(quote.dealerAuthPublicKey);
   msg->set_settlement_id(quote.settlementId);
   msg->set_dealer_tx(quote.dealerTransaction);
   msg->set_expiration_time(quote.expirationTime.toSecsSinceEpoch());
   msg->set_time_skew_ms(quote.timeSkewMs);
   msg->set_timestamp(quote.celerTimestamp);
}


bs::network::Order bs::message::fromMsg(const BlockSettle::Terminal::MatchingMessage_Order& msg)
{
   bs::network::Order order;
   order.clOrderId = msg.cl_order_id();
   order.exchOrderId = QString::fromStdString(msg.exchange_id());
   order.quoteId = msg.quote_id();
   order.dateTime = QDateTime::fromMSecsSinceEpoch(msg.timestamp());
   order.security = msg.security();
   order.product = msg.product();
   order.settlementId = BinaryData::fromString(msg.settlement_id());
   order.reqTransaction = msg.requester_tx();
   order.dealerTransaction = msg.dealer_tx();
   order.pendingStatus = msg.pending_status();
   order.quantity = msg.quantity();
   order.leavesQty = msg.left_qty();
   order.price = msg.price();
   order.avgPx = msg.avg_price();
   order.side = msg.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   order.assetType = static_cast<bs::network::Asset::Type>(msg.asset_type());
   order.status = static_cast<bs::network::Order::Status>(msg.status());
   order.info = msg.info();
   return order;
}

void bs::message::toMsg(const bs::network::Order& order, MatchingMessage_Order* msg)
{
   msg->set_cl_order_id(order.clOrderId);
   msg->set_exchange_id(order.exchOrderId.toStdString());
   msg->set_quote_id(order.quoteId);
   msg->set_timestamp(order.dateTime.toMSecsSinceEpoch());
   msg->set_security(order.security);
   msg->set_product(order.product);
   msg->set_settlement_id(order.settlementId.toBinStr());
   msg->set_requester_tx(order.reqTransaction);
   msg->set_dealer_tx(order.dealerTransaction);
   msg->set_pending_status(order.pendingStatus);
   msg->set_quantity(order.quantity);
   msg->set_left_qty(order.leavesQty);
   msg->set_price(order.price);
   msg->set_avg_price(order.avgPx);
   msg->set_buy(order.side == bs::network::Side::Buy);
   msg->set_asset_type((int)order.assetType);
   msg->set_status((int)order.status);
   msg->set_info(order.info);
}


bs::network::QuoteReqNotification bs::message::fromMsg(const BlockSettle::Terminal::IncomingRFQ& msg)
{
   bs::network::QuoteReqNotification result;
   result.quantity = msg.rfq().quantity();
   result.quoteRequestId = msg.rfq().id();
   result.security = msg.rfq().security();
   result.product = msg.rfq().product();
   result.requestorAuthPublicKey = msg.rfq().auth_pub_key();
   result.requestorRecvAddress = msg.rfq().receipt_address();
   result.side = msg.rfq().buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   result.assetType = static_cast<bs::network::Asset::Type>(msg.rfq().asset_type());
   result.settlementId = msg.settlement_id();
   result.sessionToken = msg.session_token();
   result.party = msg.party();
   result.reason = msg.reason();
   result.account = msg.account();
   result.status = static_cast<bs::network::QuoteReqNotification::Status>(msg.status());
   result.expirationTime = msg.expiration_ms();
   result.timestamp = msg.timestamp_ms();
   result.timeSkewMs = msg.time_skew_ms();
   return result;
}

void bs::message::toMsg(const bs::network::QuoteReqNotification& qrn
   , BlockSettle::Terminal::IncomingRFQ* msg)
{
   auto msgRFQ = msg->mutable_rfq();
   msgRFQ->set_quantity(qrn.quantity);
   msgRFQ->set_id(qrn.quoteRequestId);
   msgRFQ->set_security(qrn.security);
   msgRFQ->set_product(qrn.product);
   msgRFQ->set_auth_pub_key(qrn.requestorAuthPublicKey);
   msgRFQ->set_receipt_address(qrn.requestorRecvAddress);
   msgRFQ->set_buy(qrn.side == bs::network::Side::Buy);
   msgRFQ->set_asset_type((int)qrn.assetType);
   msg->set_settlement_id(qrn.settlementId);
   msg->set_session_token(qrn.sessionToken);
   msg->set_party(qrn.party);
   msg->set_reason(qrn.reason);
   msg->set_account(qrn.account);
   msg->set_status((int)qrn.status);
   msg->set_expiration_ms(qrn.expirationTime);
   msg->set_timestamp_ms(qrn.timestamp);
   msg->set_time_skew_ms(qrn.timeSkewMs);
}


bs::network::QuoteNotification bs::message::fromMsg(const BlockSettle::Terminal::ReplyToRFQ& msg)
{
   bs::network::QuoteNotification result;
   result.authKey = msg.quote().deal_auth_pub_key();
   result.reqAuthKey = msg.quote().req_auth_pub_key();
   result.settlementId = msg.quote().settlement_id();
   result.quoteRequestId = msg.quote().request_id();
   result.security = msg.quote().security();
   result.product = msg.quote().product();
   result.transactionData = msg.quote().dealer_tx();
   result.assetType = static_cast<bs::network::Asset::Type>(msg.quote().asset_type());
   result.side = msg.quote().buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   result.validityInS = (msg.quote().expiration_time() - msg.quote().timestamp()) / 1000;
   result.price = msg.quote().price();
   result.quantity = msg.quote().quantity();
   result.sessionToken = msg.session_token();
   result.account = msg.account();
   result.receiptAddress = msg.dealer_recv_addr();
   return result;
}

void bs::message::toMsg(const bs::network::QuoteNotification& qn
   , BlockSettle::Terminal::ReplyToRFQ* msg)
{
   auto msgQuote = msg->mutable_quote();
   msgQuote->set_deal_auth_pub_key(qn.authKey);
   msgQuote->set_req_auth_pub_key(qn.reqAuthKey);
   msgQuote->set_settlement_id(qn.settlementId);
   msgQuote->set_request_id(qn.quoteRequestId);
   msgQuote->set_security(qn.security);
   msgQuote->set_product(qn.product);
   msgQuote->set_dealer_tx(qn.transactionData);
   msgQuote->set_asset_type((int)qn.assetType);
   msgQuote->set_buy(qn.side == bs::network::Side::Buy);
   msgQuote->set_price(qn.price);
   msgQuote->set_quantity(qn.quantity);
   msg->set_session_token(qn.sessionToken);
   msg->set_account(qn.account);
   msg->set_dealer_recv_addr(qn.receiptAddress);

   const auto& timeNow = std::chrono::system_clock::now();
   msgQuote->set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(timeNow.time_since_epoch()).count());
   msgQuote->set_expiration_time(std::chrono::duration_cast<std::chrono::milliseconds>(
      (timeNow + std::chrono::seconds{qn.validityInS}).time_since_epoch()).count());
}

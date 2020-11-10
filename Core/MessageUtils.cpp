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

/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MatchingAdapter.h"
#include <spdlog/spdlog.h>
#include "Celer/CommonUtils.h"
#include "Celer/CancelQuoteNotifSequence.h"
#include "Celer/CancelRFQSequence.h"
#include "Celer/CelerClientProxy.h"
#include "Celer/CreateOrderSequence.h"
#include "Celer/CreateFxOrderSequence.h"
#include "Celer/GetAssignedAccountsListSequence.h"
#include "Celer/SubmitQuoteNotifSequence.h"
#include "Celer/SubmitRFQSequence.h"
#include "CurrencyPair.h"
#include "MessageUtils.h"
#include "ProtobufUtils.h"
#include "TerminalMessage.h"

#include "terminal.pb.h"
#include "DownstreamQuoteProto.pb.h"
#include "DownstreamOrderProto.pb.h"
#include "bitcoin/DownstreamBitcoinTransactionSigningProto.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;
using namespace com::celertech::marketmerchant::api::enums::orderstatus;
using namespace com::celertech::marketmerchant::api::enums::producttype::quotenotificationtype;
using namespace com::celertech::marketmerchant::api::enums::side;
using namespace com::celertech::marketmerchant::api::order;
using namespace com::celertech::marketmerchant::api::quote;


MatchingAdapter::MatchingAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::Matching))
   , userSettl_(std::make_shared<UserTerminal>(TerminalUsers::Settlement))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
{
   celerConnection_ = std::make_unique<ClientCelerConnection>(logger, this, true, true);

   celerConnection_->RegisterHandler(CelerAPI::QuoteDownstreamEventType
      , [this](const std::string& data) {
      return onQuoteResponse(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::QuoteRequestRejectDownstreamEventType
      , [this](const std::string& data) {
      return onQuoteReject(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::CreateOrderRequestRejectDownstreamEventType
      , [this](const std::string& data) {
      return onOrderReject(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::BitcoinOrderSnapshotDownstreamEventType
      , [this](const std::string& data) {
      return onBitcoinOrderSnapshot(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::FxOrderSnapshotDownstreamEventType
      , [this](const std::string& data) {
      return onFxOrderSnapshot(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::QuoteCancelDownstreamEventType
      , [this](const std::string& data) {
      return onQuoteCancelled(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::SignTransactionNotificationType
      , [this](const std::string& data) {
      return onSignTxNotif(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::QuoteAckDownstreamEventType
      , [this](const std::string& data) {
      return onQuoteAck(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::QuoteRequestNotificationType
      , [this](const std::string& data) {
      return onQuoteReqNotification(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::QuoteCancelNotifReplyType
      , [this](const std::string& data) {
      return onQuoteNotifCancelled(data);
   });
   celerConnection_->RegisterHandler(CelerAPI::SubLedgerSnapshotDownstreamEventType
      , [](const std::string&) { return true; });  // remove warning from log
}

void MatchingAdapter::connectedToServer()
{
   logger_->debug("[MatchingAdapter::connectedToServer]");
   MatchingMessage msg;
   auto loggedIn = msg.mutable_logged_in();
   loggedIn->set_user_type(static_cast<int>(celerConnection_->celerUserType()));
   loggedIn->set_user_id(celerConnection_->userId());
   loggedIn->set_user_name(celerConnection_->userName());
   pushBroadcast(user_, msg.SerializeAsString());

   sendSetUserId(celerConnection_->userId());

   const auto &cbAccounts = [this](const std::vector<std::string>& accVec)
   {  // Remove duplicated entries if possible
      std::set<std::string> accounts(accVec.cbegin(), accVec.cend());
      if (accounts.size() == 1) {
         assignedAccount_ = *accounts.cbegin();
         logger_->debug("[MatchingAdapter] assigned account: {}", assignedAccount_);
      }
      else if (accounts.empty()) {
         logger_->error("[MatchingAdapter::onCelerConnected] no accounts");
      } else {
         logger_->error("[MatchingAdapter::onCelerConnected] too many accounts ({})"
            , accounts.size());
         for (const auto& account : accounts) {
            logger_->error("[MatchingAdapter::onCelerConnected] acc: {}", account);
         }
      }
   };
   if (!celerConnection_->ExecuteSequence(std::make_shared<bs::celer::GetAssignedAccountsListSequence>(
      logger_, cbAccounts))) {
      logger_->error("[{}] failed to get accounts", __func__);
   }
}

void MatchingAdapter::connectionClosed()
{
   sendSetUserId({});

   assignedAccount_.clear();
   celerConnection_->CloseConnection();

   MatchingMessage msg;
   msg.mutable_logged_out();
   pushBroadcast(user_, msg.SerializeAsString());
}

void MatchingAdapter::connectionError(int errorCode)
{
   logger_->debug("[MatchingAdapter::connectionError] {}", errorCode);
   MatchingMessage msg;
   msg.set_connection_error(errorCode);
   pushBroadcast(user_, msg.SerializeAsString());
}

bool MatchingAdapter::process(const bs::message::Envelope &env)
{
   if (!env.isRequest() && (env.sender->value<bs::message::TerminalUsers>() == bs::message::TerminalUsers::BsServer)) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse BsServer message #{}", __func__, env.id());
         return true;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kRecvMatching:
         celerConnection_->recvData(static_cast<CelerAPI::CelerMessageType>(msg.recv_matching().message_type())
            , msg.recv_matching().data());
         break;
      default: break;
      }
   }
   else if (env.receiver->value() == user_->value()) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse message #{}", __func__, env.id());
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kLogin:
         return processLogin(msg.login());
      case MatchingMessage::kLogout:
         if (celerConnection_ && celerConnection_->IsConnected()) {
            celerConnection_->CloseConnection();
         }
         break;
      case MatchingMessage::kGetSubmittedAuthAddresses:
         return processGetSubmittedAuth(env);
      case MatchingMessage::kSubmitAuthAddress:
         return processSubmitAuth(env, msg.submit_auth_address());
      case MatchingMessage::kSendRfq:
         return processSendRFQ(msg.send_rfq());
      case MatchingMessage::kAcceptRfq:
         return processAcceptRFQ(msg.accept_rfq());
      case MatchingMessage::kCancelRfq:
         return processCancelRFQ(msg.cancel_rfq());
      case MatchingMessage::kSubmitQuoteNotif:
         return processSubmitQuote(msg.submit_quote_notif());
      case MatchingMessage::kPullQuoteNotif:
         return processPullQuote(msg.pull_quote_notif());
      default:
         logger_->warn("[MatchingAdapter::process] unknown msg {} #{} from {}"
            , msg.data_case(), env.id(), env.sender->name());
         break;
      }
   }
   return true;
}

bool MatchingAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse administrative message #{}", __func__, env.id());
         return false;
      }
      if (msg.data_case() == AdministrativeMessage::kStart) {
         AdministrativeMessage admMsg;
         admMsg.set_component_loading(user_->value());
         pushBroadcast(UserTerminal::create(TerminalUsers::System)
            , admMsg.SerializeAsString());
         return true;
      }
   }
   return false;
}

bool MatchingAdapter::processLogin(const MatchingMessage_Login& request)
{
   return celerConnection_->SendLogin(request.matching_login()
      , request.terminal_login(), {});
}

bool MatchingAdapter::processGetSubmittedAuth(const bs::message::Envelope& env)
{
   MatchingMessage msg;
   auto msgResp = msg.mutable_submitted_auth_addresses();
   for (const auto& addr : celerConnection_->GetSubmittedAuthAddressSet()) {
      msgResp->add_addresses(addr);
   }
   return pushResponse(user_, env, msg.SerializeAsString());
}

bool MatchingAdapter::processSubmitAuth(const bs::message::Envelope& env
   , const std::string& address)
{
   auto submittedAddresses = celerConnection_->GetSubmittedAuthAddressSet();
   submittedAddresses.insert(address);
   celerConnection_->SetSubmittedAuthAddressSet(submittedAddresses);
   return processGetSubmittedAuth(env);
}

bool MatchingAdapter::processSendRFQ(const BlockSettle::Terminal::RFQ& request)
{
   if (assignedAccount_.empty()) {
      logger_->error("[MatchingAdapter::processSendRFQ] submitting with empty account name");
   }
   bs::network::RFQ rfq;
   rfq.requestId = request.id();
   rfq.security = request.security();
   rfq.product = request.product();
   rfq.assetType = static_cast<bs::network::Asset::Type>(request.asset_type());
   rfq.side = request.buy() ? bs::network::Side::Buy : bs::network::Side::Sell;
   rfq.quantity = request.quantity();
   rfq.requestorAuthPublicKey = request.auth_pub_key();
   rfq.receiptAddress = request.receipt_address();
   rfq.coinTxInput = request.coin_tx_input();
   auto sequence = std::make_shared<bs::celer::SubmitRFQSequence>(assignedAccount_, rfq
      , logger_, true);
   if (!celerConnection_->ExecuteSequence(sequence)) {
      logger_->error("[MatchingAdapter::processSendRFQ] failed to execute CelerSubmitRFQSequence");
   } else {
      logger_->debug("[MatchingAdapter::processSendRFQ] RFQ submitted: {}", rfq.requestId);
      submittedRFQs_[rfq.requestId] = rfq;
   }
   return true;
}

bool MatchingAdapter::processAcceptRFQ(const AcceptRFQ& request)
{
   if (assignedAccount_.empty()) {
      logger_->error("[MatchingAdapter::processAcceptRFQ] accepting with empty account name");
   }
   const auto& reqId = QString::fromStdString(request.rfq_id());
   const auto& quote = fromMsg(request.quote());
   if (quote.assetType == bs::network::Asset::SpotFX) {
      auto sequence = std::make_shared<bs::celer::CreateFxOrderSequence>(assignedAccount_
         , reqId, quote, logger_);
      if (!celerConnection_->ExecuteSequence(sequence)) {
         logger_->error("[MatchingAdapter::processAcceptRFQ] failed to execute CelerCreateFxOrderSequence");
      } else {
         logger_->debug("[MatchingAdapter::processAcceptRFQ] FX Order submitted");
      }
   }
   else {
      auto sequence = std::make_shared<bs::celer::CreateOrderSequence>(assignedAccount_
         , reqId, quote, request.payout_tx(), logger_);
      if (!celerConnection_->ExecuteSequence(sequence)) {
         logger_->error("[MatchingAdapter::processAcceptRFQ] failed to execute CelerCreateOrderSequence");
      } else {
         logger_->debug("[MatchingAdapter::processAcceptRFQ] Order submitted");
      }
   }
   return true;
}

bool MatchingAdapter::processCancelRFQ(const std::string& rfqId)
{
   const auto &sequence = std::make_shared<bs::celer::CancelRFQSequence>(
      QString::fromStdString(rfqId), logger_);
   if (!celerConnection_->ExecuteSequence(sequence)) {
      logger_->error("[MatchingAdapter::processCancelRFQ] failed to execute CelerCancelRFQSequence");
      return false;
   } else {
      logger_->debug("[MatchingAdapter::processCancelRFQ] RFQ {} cancelled", rfqId);
   }
   return true;
}

bool MatchingAdapter::processSubmitQuote(const ReplyToRFQ& request)
{
   if (assignedAccount_.empty()) {
      logger_->warn("[MatchingAdapter::processSubmitQuote] account name not set");
   }
   const auto& qn = fromMsg(request);
   const auto &sequence = std::make_shared<bs::celer::SubmitQuoteNotifSequence>(assignedAccount_, qn, logger_);
   if (!celerConnection_->ExecuteSequence(sequence)) {
      logger_->error("[MatchingAdapter::processSubmitQuote] failed to execute CelerSubmitQuoteNotifSequence");
   } else {
      logger_->debug("[MatchingAdapter::processSubmitQuote] QuoteNotification on {} submitted"
         , qn.quoteRequestId);
   }
   return true;
}

bool MatchingAdapter::processPullQuote(const PullRFQReply& request)
{
   std::shared_ptr<bs::celer::CancelQuoteNotifSequence> sequence;
   try {
      sequence = std::make_shared<bs::celer::CancelQuoteNotifSequence>(request.rfq_id()
         , request.session_token(), logger_);
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to init: {}", __func__, e.what());
      return true;
   }
   if (!celerConnection_->ExecuteSequence(sequence)) {
      logger_->error("[MatchingAdapter::processPullQuote] failed to execute CancelQuoteNotifSequence");
   } else {
      logger_->debug("[MatchingAdapter::processPullQuote] QuoteNotification on {} pulled"
         , request.rfq_id());
   }
   return true;
}

void MatchingAdapter::saveQuoteRequestCcy(const std::string& id, const std::string& ccy)
{
   quoteCcys_.emplace(id, ccy);
}

void MatchingAdapter::cleanQuoteRequestCcy(const std::string& id)
{
   auto it = quoteCcys_.find(id);
   if (it != quoteCcys_.end()) {
      quoteCcys_.erase(it);
   }
}

void MatchingAdapter::sendSetUserId(const std::string& userId)
{
   logger_->debug("[{}] setting userId {}", __func__, userId);
   WalletsMessage msg;
   msg.set_set_user_id(celerConnection_->userId());
   pushRequest(user_, userWallets_, msg.SerializeAsString());
}

std::string MatchingAdapter::getQuoteRequestCcy(const std::string& id) const
{
   std::string ccy;
   auto it = quoteCcys_.find(id);
   if (it != quoteCcys_.end()) {
      ccy = it->second;
   }
   return ccy;
}

bool MatchingAdapter::onQuoteResponse(const std::string& data)
{
   QuoteDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteResponse] Failed to parse QuoteDownstreamEvent");
      return false;
   }
   logger_->debug("[MatchingAdapter::onQuoteResponse]: {}", ProtobufUtils::toJsonCompact(response));

   bs::network::Quote quote;
   quote.quoteId = response.quoteid();
   quote.requestId = response.quoterequestid();
   quote.security = response.securitycode();
   quote.assetType = bs::celer::fromCelerProductType(response.producttype());
   quote.side = bs::celer::fromCeler(response.side());

   if (quote.assetType == bs::network::Asset::PrivateMarket) {
      quote.dealerAuthPublicKey = response.dealerreceiptaddress();
      quote.dealerTransaction = response.dealercointransactioninput();
   }

   switch (response.quotingtype()) {
   case com::celertech::marketmerchant::api::enums::quotingtype::AUTOMATIC:
      quote.quotingType = bs::network::Quote::Automatic;
      break;
   case com::celertech::marketmerchant::api::enums::quotingtype::MANUAL:
      quote.quotingType = bs::network::Quote::Manual;
      break;
   case com::celertech::marketmerchant::api::enums::quotingtype::DIRECT:
      quote.quotingType = bs::network::Quote::Direct;
      break;
   case com::celertech::marketmerchant::api::enums::quotingtype::INDICATIVE:
      quote.quotingType = bs::network::Quote::Indicative;
      break;
   case com::celertech::marketmerchant::api::enums::quotingtype::TRADEABLE:
      quote.quotingType = bs::network::Quote::Tradeable;
      break;
   default:
      quote.quotingType = bs::network::Quote::Indicative;
      break;
   }

   quote.expirationTime = QDateTime::fromMSecsSinceEpoch(response.validuntiltimeutcinmillis());
   quote.timeSkewMs = QDateTime::fromMSecsSinceEpoch(response.quotetimestamputcinmillis()).msecsTo(QDateTime::currentDateTime());
   quote.celerTimestamp = response.quotetimestamputcinmillis();

   logger_->debug("[MatchingAdapter::onQuoteResponse] timeSkew = {}", quote.timeSkewMs);
   CurrencyPair cp(quote.security);

   const auto& grp = response.legquotegroup(0);
   quote.product = grp.currency();

   const auto& itRFQ = submittedRFQs_.find(response.quoterequestid());
   if (itRFQ == submittedRFQs_.end()) {   // Quote for dealer to indicate GBBO   //WTF?
      const auto quoteCcy = getQuoteRequestCcy(quote.requestId);
      if (!quoteCcy.empty()) {
         double price = 0;

         if ((quote.side == bs::network::Side::Sell) ^ (quoteCcy != cp.NumCurrency())) {
            price = response.offerpx();
         } else {
            price = response.bidpx();
         }

         const bool own = response.has_quotedbysessionkey() && !response.quotedbysessionkey().empty();
         MatchingMessage msg;
         auto msgBest = msg.mutable_best_quoted_price();
         msgBest->set_quote_req_id(response.quoterequestid());
         msgBest->set_price(price);
         msgBest->set_own(own);
         pushRequest(user_, userSettl_, msg.SerializeAsString());
      }
      quote.quantity = grp.bidsize();  // equal to offersize/offerpx regardless of side
      quote.price = response.bidpx();
   } else {
      if (response.legquotegroup_size() != 1) {
         logger_->error("[MatchingAdapter::onQuoteResponse] invalid leg number: {}\n{}"
            , response.legquotegroup_size()
            , ProtobufUtils::toJsonCompact(response));
         return false;
      }

      if (quote.assetType == bs::network::Asset::SpotXBT) {
         quote.requestorAuthPublicKey = itRFQ->second.requestorAuthPublicKey;
      }

      if ((quote.side == bs::network::Side::Sell) ^ (itRFQ->second.product != cp.NumCurrency())) {
         quote.price = response.offerpx();
         quote.quantity = grp.offersize();
      } else {
         quote.price = response.bidpx();
         quote.quantity = grp.bidsize();
      }

      if (quote.quotingType == bs::network::Quote::Tradeable) {
         submittedRFQs_.erase(itRFQ);
      }
   }

   if (quote.assetType == bs::network::Asset::SpotXBT) {
      quote.dealerAuthPublicKey = response.dealerauthenticationaddress();
      quote.dealerTransaction = response.dealertransaction();
      if (response.has_settlementid() && !response.settlementid().empty()) {
         quote.settlementId = response.settlementid();
      }
   }
   MatchingMessage msg;
   toMsg(quote, msg.mutable_quote());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onQuoteReject(const std::string& data)
{
   QuoteRequestRejectDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteReject] failed to parse");
      return false;
   }
   logger_->debug("[MatchingAdapter::onQuoteReject] {}", ProtobufUtils::toJsonCompact(response));

   MatchingMessage msg;
   auto msgReq = msg.mutable_quote_reject();
   msgReq->set_rfq_id(response.quoterequestid());
   if (response.quoterequestrejectgroup_size() > 0) {
      const QuoteRequestRejectGroup& rejGrp = response.quoterequestrejectgroup(0);
      msgReq->set_reject_text(rejGrp.text());
   }
   msgReq->set_reject_code((int)response.quoterequestrejectreason());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onOrderReject(const std::string& data)
{
   CreateOrderRequestRejectDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteReject] failed to parse");
      return false;
   }
   logger_->debug("[MatchingAdapter::onOrderReject] {}", ProtobufUtils::toJsonCompact(response));

   MatchingMessage msg;
   auto msgReq = msg.mutable_order_reject();
   msgReq->set_order_id(response.externalclorderid());
   msgReq->set_quote_id(response.quoteid());
   msgReq->set_reject_text(response.rejectreason());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onBitcoinOrderSnapshot(const std::string& data)
{
   BitcoinOrderSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onBitcoinOrderSnapshot] failed to parse");
      return false;
   }
   logger_->debug("[MatchingAdapter::onBitcoinOrderSnapshot] {}", ProtobufUtils::toJsonCompact(response));

   auto orderDate = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   //auto ageSeconds = orderDate.secsTo(QDateTime::currentDateTime());

   bs::network::Order order;
   order.exchOrderId = QString::number(response.orderid());
   order.clOrderId = response.externalclorderid();
   order.quoteId = response.quoteid();
   order.dateTime = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   order.security = response.securitycode();
   order.quantity = response.qty();
   order.price = response.price();
   order.product = response.currency();
   order.side = bs::celer::fromCeler(response.side());
   order.assetType = bs::celer::fromCelerProductType(response.producttype());
   try {
      order.settlementId = BinaryData::CreateFromHex(response.settlementid());
   }
   catch (const std::exception& e) {
      logger_->error("[MatchingAdapter::onBitcoinOrderSnapshot] failed to parse settlement id");
      return false;
   }
   order.reqTransaction = response.requestortransaction();
   order.dealerTransaction = response.dealertransaction();
   order.status = bs::celer::mapBtcOrderStatus(response.orderstatus());
   order.pendingStatus = response.info();

   MatchingMessage msg;
   toMsg(order, msg.mutable_order());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onFxOrderSnapshot(const std::string& data)
{
   FxOrderSnapshotDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onFxOrderSnapshot] Failed to parse FxOrderSnapshotDownstreamEvent");
      return false;
   }
   logger_->debug("[MatchingAdapter::onFxOrderSnapshot] {}", response.DebugString());

   bs::network::Order order;
   order.exchOrderId = QString::number(response.orderid());
   order.clOrderId = response.externalclorderid();
   order.quoteId = response.quoteid();
   order.dateTime = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   order.security = response.securitycode();
   order.quantity = response.qty();
   order.leavesQty = response.leavesqty();
   order.price = response.price();
   order.avgPx = response.avgpx();
   order.product = response.currency();
   order.side = bs::celer::fromCeler(response.side());
   order.assetType = bs::network::Asset::SpotFX;

   order.status = bs::celer::mapFxOrderStatus(response.orderstatus());
   order.info = response.info();

   MatchingMessage msg;
   toMsg(order, msg.mutable_order());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onQuoteCancelled(const std::string& data)
{
   QuoteCancelDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteCancelled] failed to parse");
      return false;
   }
   logger_->debug("[MatchingAdapter::onQuoteCancelled] {}", ProtobufUtils::toJsonCompact(response));

   MatchingMessage msg;
   auto msgData = msg.mutable_quote_cancelled();
   msgData->set_rfq_id(response.quoterequestid());
   msgData->set_quote_id(response.quoteid());
   msgData->set_by_user(response.quotecanceltype() ==
      com::celertech::marketmerchant::api::enums::quotecanceltype::CANCEL_ALL_QUOTES);
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onSignTxNotif(const std::string&)
{
   return false;
}

bool MatchingAdapter::onQuoteAck(const std::string&)
{
   return false;
}

bool MatchingAdapter::onQuoteReqNotification(const std::string& data)
{
   QuoteRequestNotification response;

   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteReqNotification] failed to parse");
      return false;
   }

   if (response.quoterequestnotificationgroup_size() < 1) {
      logger_->error("[MatchingAdapter::onQuoteReqNotification] missing at least 1 QRN group");
      return false;
   }  // For SpotFX and SpotXBT there should be only 1 group

   const QuoteRequestNotificationGroup& respgrp = response.quoterequestnotificationgroup(0);

   if (respgrp.quoterequestnotificationleggroup_size() != 1) {
      logger_->error("[MatchingAdapter::onQuoteReqNotification] wrong leg group size: {}\n{}"
         , respgrp.quoterequestnotificationleggroup_size()
         , ProtobufUtils::toJsonCompact(response));
      return false;
   }

   const auto& legGroup = respgrp.quoterequestnotificationleggroup(0);

   bs::network::QuoteReqNotification qrn;
   qrn.quoteRequestId = response.quoterequestid();
   qrn.security = respgrp.securitycode();
   qrn.sessionToken = response.requestorsessiontoken();
   qrn.quantity = legGroup.qty();
   qrn.product = respgrp.currency();
   qrn.party = respgrp.partyid();
   //qrn.reason = response.reason();
   //qrn.account = response.account();
   qrn.expirationTime = response.expiretimeinutcinmillis();
   qrn.timestamp = response.timestampinutcinmillis();
   qrn.timeSkewMs = QDateTime::fromMSecsSinceEpoch(qrn.timestamp).msecsTo(QDateTime::currentDateTime());

   qrn.side = bs::celer::fromCeler(legGroup.side());
   qrn.assetType = bs::celer::fromCelerProductType(respgrp.producttype());

   switch (response.quotenotificationtype()) {
   case QUOTE_WITHDRAWN:
      qrn.status = bs::network::QuoteReqNotification::Withdrawn;
      break;
   case PENDING_ACKNOWLEDGE:
      qrn.status = bs::network::QuoteReqNotification::PendingAck;
      break;
   default:
      qrn.status = bs::network::QuoteReqNotification::StatusUndefined;
      break;
   }

   if (response.has_settlementid() && !response.settlementid().empty()) {
      qrn.settlementId = response.settlementid();
   }

   switch (qrn.assetType) {
   case bs::network::Asset::SpotXBT:
      qrn.requestorAuthPublicKey = response.requestorauthenticationaddress();
      break;
   case bs::network::Asset::PrivateMarket:
      qrn.requestorAuthPublicKey = respgrp.requestorcointransactioninput();
      qrn.requestorRecvAddress = response.requestorreceiptaddress();
      break;
   default: break;
   }

   saveQuoteRequestCcy(qrn.quoteRequestId, qrn.product);

   logger_->debug("[MatchingAdapter::onQuoteReqNotification] {}", ProtobufUtils::toJsonCompact(response));
   MatchingMessage msg;
   toMsg(qrn, msg.mutable_incoming_rfq());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingAdapter::onQuoteNotifCancelled(const std::string& data)
{
   QuoteCancelDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MatchingAdapter::onQuoteNotifCancelled] failed to parse");
      return false;
   }
   logger_->debug("[MatchingAdapter::onQuoteNotifCancelled] {}", ProtobufUtils::toJsonCompact(response));

   MatchingMessage msg;
   auto msgReq = msg.mutable_quote_cancelled();
   msgReq->set_rfq_id(response.quoterequestid());
   if (response.quotecanceltype() != com::celertech::marketmerchant::api::enums::quotecanceltype::CANCEL_ALL_QUOTES) {
      msgReq->set_quote_id(response.quoteid());
   }
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}


ClientCelerConnection::ClientCelerConnection(const std::shared_ptr<spdlog::logger>& logger
   , MatchingAdapter* parent, bool userIdRequired, bool useRecvTimer)
   : BaseCelerClient(logger, parent, userIdRequired, useRecvTimer)
   , parent_(parent)
{}

void ClientCelerConnection::onSendData(CelerAPI::CelerMessageType messageType
   , const std::string& data)
{
   BsServerMessage msg;
   auto msgReq = msg.mutable_send_matching();
   msgReq->set_message_type((int)messageType);
   msgReq->set_data(data);
   auto env = Envelope::makeRequest(parent_->user_, UserTerminal::create(TerminalUsers::BsServer)
      , msg.SerializeAsString());
   parent_->pushFill(env);
}

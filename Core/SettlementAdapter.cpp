/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettlementAdapter.h"
#include <spdlog/spdlog.h>
#include "BSErrorCode.h"
#include "CurrencyPair.h"
#include "MessageUtils.h"
#include "ProtobufHeadlessUtils.h"
#include "TerminalMessage.h"
#include "UiUtils.h" // only for actualXbtPrice() and displayPriceXBT()

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;

constexpr auto kHandshakeTimeout = std::chrono::seconds{ 30 };


SettlementAdapter::SettlementAdapter(const std::shared_ptr<spdlog::logger> &logger)
   : logger_(logger)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::Settlement))
   , userBS_(std::make_shared<UserTerminal>(TerminalUsers::BsServer))
   , userMtch_(std::make_shared<UserTerminal>(TerminalUsers::Matching))
   , userWallets_(std::make_shared<UserTerminal>(TerminalUsers::Wallets))
   , userSigner_(std::make_shared<UserTerminal>(TerminalUsers::Signer))
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
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Blockchain) {
      ArmoryMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse armory msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case ArmoryMessage::kZcReceived:
         return processZC(msg.zc_received());
      default: break;
      }
      if (!env.receiver || env.receiver->isBroadcast()) {
         return false;
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
      case MatchingMessage::kIncomingRfq:
         return processMatchingInRFQ(msg.incoming_rfq());
      default: break;
      }
      if (!env.receiver || env.receiver->isBroadcast()) {
         return false;
      }
   }
   else if (env.sender->value() == userBS_->value()) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse BS msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kUnsignedPayinRequested:
         return processBsUnsignedPayin(BinaryData::fromString(msg.unsigned_payin_requested()));
      case BsServerMessage::kSignedPayinRequested:
         return processBsSignPayin(msg.signed_payin_requested());
      case BsServerMessage::kSignedPayoutRequested:
         return processBsSignPayout(msg.signed_payout_requested());
      default: break;
      }
      if (!env.receiver || env.receiver->isBroadcast()) {
         return false;
      }
   }
   else if (env.sender->value() == userWallets_->value()) {
      WalletsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse wallets msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case WalletsMessage::kXbtTxResponse:
         return processXbtTx(env.id, msg.xbt_tx_response());
      default: break;
      }
      if (!env.receiver || env.receiver->isBroadcast()) {
         return false;
      }
   } 
   else if (env.sender->value() == userSigner_->value()) {
      SignerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse signer msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case SignerMessage::kSignTxResponse:
         return processSignedTx(env.id, msg.sign_tx_response());
      default: break;
      }
      if (!env.receiver || env.receiver->isBroadcast()) {
         return false;
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
      case SettlementMessage::kHandshakeTimeout:
         return processHandshakeTimeout(msg.handshake_timeout());
      case SettlementMessage::kQuoteReqTimeout:
         return processInRFQTimeout(msg.quote_req_timeout());
      case SettlementMessage::kReplyToRfq:
         return processSubmitQuote(env, msg.reply_to_rfq());
      case SettlementMessage::kPullRfqReply:
         return processPullQuote(env, msg.pull_rfq_reply());
      default:
         logger_->warn("[{}] unknown settlement request {}", __func__, msg.data_case());
         break;
      }
   }
   return true;
}

bool SettlementAdapter::processZC(const ArmoryMessage_ZCReceived& zcData)
{
   for (const auto& zcEntry : zcData.tx_entries()) {
      const auto& txHash = BinaryData::fromString(zcEntry.tx_hash());
      const auto& itZC = pendingZCs_.find(txHash);
      if (itZC == pendingZCs_.end()) {
         continue;
      }
      const auto settlementId = itZC->second;
      pendingZCs_.erase(itZC);
      const auto& itSettl = settlBySettlId_.find(settlementId);
      if (itSettl == settlBySettlId_.end()) {
         logger_->warn("[{}] settlement not found for {}", __func__, settlementId.toHexStr());
         continue;
      }
      SettlementMessage msg;
      auto msgResponse = msg.mutable_settlement_complete();
      msgResponse->set_rfq_id(itSettl->second->rfq.requestId);
      msgResponse->set_quote_id(itSettl->second->quote.quoteId);
      msgResponse->set_settlement_id(settlementId.toBinStr());
      Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(env);
      close(settlementId);
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
   itSettl->second->quote = fromMsg(response);
   const auto& itQuote = settlByQuoteId_.find(response.quote_id());
   if (itQuote == settlByQuoteId_.end()) {
      settlByQuoteId_[response.quote_id()] = itSettl->second;
   }
   SettlementMessage msg;
   *msg.mutable_quote() = response;
   Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}
      , msg.SerializeAsString() };
   return pushFill(env);
}

bool SettlementAdapter::processMatchingOrder(const MatchingMessage_Order& response)
{
   const auto& itSettl = settlByQuoteId_.find(response.quote_id());
   if (itSettl == settlByQuoteId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, response.quote_id());
      return true;
   }

   SettlementMessage msg;
   const auto& order = fromMsg(response);
   if (order.status == bs::network::Order::Status::Filled) {
      auto msgResp = msg.mutable_matched_quote();
      msgResp->set_rfq_id(itSettl->second->rfq.requestId);
      msgResp->set_quote_id(response.quote_id());
      msgResp->set_price(response.price());
   }
   else if (order.status == bs::network::Order::Status::Failed) {
      auto msgResp = msg.mutable_failed_settlement();
      msgResp->set_rfq_id(itSettl->second->rfq.requestId);
      msgResp->set_quote_id(response.quote_id());
      msgResp->set_info(order.info);
   }
   else if (order.status == bs::network::Order::Status::Pending) {
      if (/*itSettl->second->dealer &&*/ (itSettl->second->quote.assetType == bs::network::Asset::SpotXBT)
         && (itSettl->second->quote.quotingType == bs::network::Quote::Tradeable)) {
         if (((itSettl->second->quote.side == bs::network::Side::Buy) ||
            (itSettl->second->quote.product != bs::network::XbtCurrency))
            && itSettl->second->dealerRecvAddress.empty()) {
            //maybe obtain receiving address here instead of when constructing pay-out in WalletsAdapter
         }
         if (startXbtSettlement(itSettl->second->quote)) {
            logger_->debug("[{}] started XBT settlement on {}", __func__, response.quote_id());
         } else {
            return true;
         }
      }
      auto msgResp = msg.mutable_pending_settlement();
      auto msgIds = msgResp->mutable_ids();
      msgIds->set_rfq_id(itSettl->second->rfq.requestId);
      msgIds->set_quote_id(response.quote_id());
      msgIds->set_settlement_id(response.settlement_id());
      msgResp->set_time_left_ms(kHandshakeTimeout.count() * 1000);
   }
   else {
      logger_->debug("[{}] {} unprocessed order status {}", __func__, order.quoteId
         , (int)order.status);
      return true;
   }
   Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}
      , msg.SerializeAsString() };
   return pushFill(env);
}

bool SettlementAdapter::processMatchingInRFQ(const IncomingRFQ& qrn)
{
   SettlementMessage msg;
   *msg.mutable_quote_req_notif() = qrn;
   Envelope envBC{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };

   const auto& rfq = fromMsg(qrn.rfq());
   const auto& settlement = std::make_shared<Settlement>();
   settlement->rfq = rfq;
   settlement->dealer = true;
   try {
      settlement->settlementId = BinaryData::CreateFromHex(qrn.settlement_id());
   }
   catch (const std::exception&) {
      logger_->error("[{}] invalid settlement id", __func__);
   }
   settlByRfqId_[rfq.requestId] = settlement;

   msg.set_quote_req_timeout(rfq.requestId);
   const auto& timeNow = bs::message::bus_clock::now();
   const auto expTime = std::chrono::milliseconds(qrn.expiration_ms()) - timeNow.time_since_epoch();
   if (expTime.count() < 0) {
      logger_->error("[{}] outdated expiry {} for {}", __func__, expTime.count(), rfq.requestId);
      return true;
   }
   Envelope envTO{ 0, user_, user_, timeNow, timeNow + expTime, msg.SerializeAsString(), true };
   return (pushFill(envTO) && pushFill(envBC));
}

bool SettlementAdapter::processBsUnsignedPayin(const BinaryData& settlementId)
{
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl == settlBySettlId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, settlementId.toHexStr());
      return true;
   }
   if (itSettl->second->ownAuthAddr.empty() || itSettl->second->counterKey.empty()) {
      return false;  // postpone processing until auth addresses are set
   }

   logger_->debug("[{}] {} {}", __func__, settlementId.toHexStr(), itSettl->second->amount.GetValue());
   WalletsMessage msg;
   auto msgReq = msg.mutable_payin_request();
   msgReq->set_own_auth_address(itSettl->second->ownAuthAddr.display());
   msgReq->set_counter_auth_address(itSettl->second->counterAuthAddr.display());
   msgReq->set_counter_auth_pubkey(itSettl->second->counterKey.toBinStr());
   msgReq->set_settlement_id(settlementId.toBinStr());
   msgReq->set_reserve_id(itSettl->second->reserveId);
   msgReq->set_amount(itSettl->second->amount.GetValue());
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   if (pushFill(env)) {
      payinRequests_[env.id] = settlementId;
   }
   return true;
}

bs::sync::PasswordDialogData SettlementAdapter::getDialogData(const QDateTime& timestamp
   , const Settlement& settlement) const
{
   bs::sync::PasswordDialogData dialogData;
   dialogData.setValue(bs::sync::PasswordDialogData::SettlementId, settlement.settlementId.toHexStr());
   dialogData.setValue(bs::sync::PasswordDialogData::DurationLeft
      , (int)((kHandshakeTimeout.count() - 1) * 1000));
   dialogData.setValue(bs::sync::PasswordDialogData::DurationTotal
      , (int)(kHandshakeTimeout.count() * 1000));

   // Set timestamp that will be used by auth eid server to update timers.
   dialogData.setValue(bs::sync::PasswordDialogData::DurationTimestamp, static_cast<int>(timestamp.toSecsSinceEpoch()));

   dialogData.setValue(bs::sync::PasswordDialogData::ProductGroup
      , QObject::tr(bs::network::Asset::toString(settlement.rfq.assetType)));
   dialogData.setValue(bs::sync::PasswordDialogData::Security, settlement.rfq.security);
   dialogData.setValue(bs::sync::PasswordDialogData::Product, settlement.rfq.product);
   dialogData.setValue(bs::sync::PasswordDialogData::Side
      , QObject::tr(bs::network::Side::toString(settlement.rfq.side)));
   dialogData.setValue(bs::sync::PasswordDialogData::ExpandTxInfo, true);  //TODO: make configurable?

   dialogData.setValue(bs::sync::PasswordDialogData::Market, "XBT");
   dialogData.setValue(bs::sync::PasswordDialogData::AutoSignCategory
      , static_cast<int>(settlement.dealer ? bs::signer::AutoSignCategory::SettlementDealer
         : bs::signer::AutoSignCategory::SettlementRequestor));

   dialogData.setValue(bs::sync::PasswordDialogData::SettlementId, settlement.settlementId.toHexStr());
   dialogData.setValue(bs::sync::PasswordDialogData::SettlementAddress, settlement.settlementAddress.display());

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;
   QString fxProd = QString::fromStdString(settlement.fxProduct);

   dialogData.setValue(bs::sync::PasswordDialogData::Price, UiUtils::displayPriceXBT(settlement.quote.price));
   dialogData.setValue(bs::sync::PasswordDialogData::FxProduct, fxProd);

   bool isFxProd = (settlement.rfq.product != bs::network::XbtCurrency);

   if (isFxProd) {
      dialogData.setValue(bs::sync::PasswordDialogData::Quantity, QObject::tr("%1 %2")
         .arg(UiUtils::displayAmountForProduct(settlement.rfq.quantity, fxProd, bs::network::Asset::Type::SpotXBT))
         .arg(fxProd));

      dialogData.setValue(bs::sync::PasswordDialogData::TotalValue, QObject::tr("%1 XBT")
         .arg(UiUtils::displayAmount(settlement.rfq.quantity / settlement.quote.price)));
   } else {
      dialogData.setValue(bs::sync::PasswordDialogData::Quantity, QObject::tr("%1 XBT")
         .arg(UiUtils::displayAmount(settlement.amount)));

      dialogData.setValue(bs::sync::PasswordDialogData::TotalValue, QObject::tr("%1 %2")
         .arg(UiUtils::displayAmountForProduct(settlement.amount.GetValueBitcoin() * settlement.quote.price
            , fxProd, bs::network::Asset::Type::SpotXBT))
         .arg(fxProd));
   }

   dialogData.setValue(bs::sync::PasswordDialogData::RequesterAuthAddress, settlement.ownAuthAddr.display());
   dialogData.setValue(bs::sync::PasswordDialogData::RequesterAuthAddressVerified, true);
   dialogData.setValue(bs::sync::PasswordDialogData::TxInputProduct, UiUtils::XbtCurrency);

   dialogData.setValue(bs::sync::PasswordDialogData::ResponderAuthAddress, settlement.counterAuthAddr.display());
   dialogData.setValue(bs::sync::PasswordDialogData::IsDealer, settlement.dealer);

   return dialogData;
}

bs::sync::PasswordDialogData SettlementAdapter::getPayinDialogData(const QDateTime& timestamp
   , const Settlement& settlement) const
{
   auto dlgData = getDialogData(timestamp, settlement);
   dlgData.setValue(bs::sync::PasswordDialogData::ResponderAuthAddressVerified, true); //TODO: put actual value
   dlgData.setValue(bs::sync::PasswordDialogData::SigningAllowed, true);               //TODO: same value here

   dlgData.setValue(bs::sync::PasswordDialogData::Title, QObject::tr("Settlement Pay-In"));
   dlgData.setValue(bs::sync::PasswordDialogData::SettlementPayInVisible, true);
   return dlgData;
}

bs::sync::PasswordDialogData SettlementAdapter::getPayoutDialogData(const QDateTime& timestamp
   , const Settlement& settlement) const
{
   auto dlgData = getDialogData(timestamp, settlement);
   dlgData.setValue(bs::sync::PasswordDialogData::Title, QObject::tr("Settlement Pay-Out"));
   dlgData.setValue(bs::sync::PasswordDialogData::SettlementPayOutVisible, true);

   dlgData.setValue(bs::sync::PasswordDialogData::ResponderAuthAddressVerified, true);
   dlgData.setValue(bs::sync::PasswordDialogData::SigningAllowed, true);
   return dlgData;
}

bool SettlementAdapter::processBsSignPayin(const BsServerMessage_SignXbtHalf& request)
{
   const auto &settlementId = BinaryData::fromString(request.settlement_id());
   const auto& payinHash = BinaryData::fromString(request.payin_hash());
   logger_->debug("[{}] {}: payin hash {}", __func__, settlementId.toHexStr()
      , payinHash.toHexStr());
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl == settlBySettlId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, settlementId.toHexStr());
      return true;
   }
   if (!itSettl->second->payin.isValid()) {
      logger_->error("[{}] invalid payin TX for {}", __func__, settlementId.toHexStr());
      unreserve(itSettl->second->rfq.requestId);
      SettlementMessage msg;
      auto msgFail = msg.mutable_failed_settlement();
      msgFail->set_settlement_id(settlementId.toBinStr());
      msgFail->set_info("invalid handshake flow");
      Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}, msg.SerializeAsString() };
      pushFill(env);
      settlByQuoteId_.erase(itSettl->second->quote.quoteId);
      settlBySettlId_.erase(itSettl);
      return true;
   }
   pendingZCs_[payinHash] = settlementId;
   itSettl->second->payin.txHash = payinHash;
   auto dlgData = getPayinDialogData(QDateTime::fromMSecsSinceEpoch(request.timestamp())
      , *itSettl->second);
   SignerMessage msg;
   auto msgReq = msg.mutable_sign_settlement_tx();
   msgReq->set_settlement_id(settlementId.toBinStr());
   *msgReq->mutable_tx_request() = bs::signer::coreTxRequestToPb(itSettl->second->payin);
   *msgReq->mutable_details() = dlgData.toProtobufMessage();
   Envelope env{ 0, user_, userSigner_, {}, {}, msg.SerializeAsString(), true };
   if (pushFill(env)) {
      payinRequests_[env.id] = settlementId;
      return true;
   }
   return false;
}

bool SettlementAdapter::processBsSignPayout(const BsServerMessage_SignXbtHalf& request)
{
   const auto settlementId = BinaryData::fromString(request.settlement_id());
   logger_->debug("[{}] {}", __func__, settlementId.toHexStr());
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl == settlBySettlId_.end()) {
      logger_->error("[{}] unknown settlement for {}", __func__, settlementId.toHexStr());
      return true;
   }
   const bs::Address& recvAddr = itSettl->second->dealer ?
      itSettl->second->dealerRecvAddress : itSettl->second->recvAddress;
/*   if (recvAddr.empty()) {
      logger_->debug("[{}] waiting for own receiving address", __func__);
      return false;
   }*/   // recvAddr is now obtained in WalletsAdapter if empty
   WalletsMessage msg;
   auto msgReq = msg.mutable_payout_request();
   msgReq->set_own_auth_address(itSettl->second->ownAuthAddr.display());
   msgReq->set_settlement_id(settlementId.toBinStr());
   msgReq->set_counter_auth_pubkey(itSettl->second->counterKey.toBinStr());
   msgReq->set_amount(itSettl->second->amount.GetValue());
   msgReq->set_payin_hash(request.payin_hash());
   msgReq->set_recv_address(recvAddr.display());
   Envelope env{ 0, user_, userWallets_, {}, {}, msg.SerializeAsString(), true };
   if (pushFill(env)) {
      payoutRequests_[env.id] = settlementId;
   }
   return true;
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
   itSettl->second->env = env;
   const auto& quote = fromMsg(request.quote());
   itSettl->second->quote = quote;
   settlByQuoteId_[request.quote().quote_id()] = itSettl->second;
   settlByRfqId_.erase(itSettl);

   if (quote.assetType == bs::network::Asset::SpotFX) {
      MatchingMessage msg;
      auto msgReq = msg.mutable_accept_rfq();
      *msgReq = request;
      Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
      return pushFill(envReq);
   }
   switch (quote.assetType) {
   case bs::network::Asset::SpotXBT:
      startXbtSettlement(quote);
      break;
   case bs::network::Asset::PrivateMarket:
      startCCSettlement(quote);
      break;
   default:
      logger_->error("[{}] unknown asset type {}", __func__, (int)quote.assetType);
      break;
   }
}

bool SettlementAdapter::processSendRFQ(const bs::message::Envelope& env
   , const SettlementMessage_SendRFQ& request)
{
   const auto& rfq = fromMsg(request.rfq());
   const auto &settlement = std::make_shared<Settlement>(Settlement{ env
      , false, rfq, request.reserve_id() });
   if ((rfq.assetType != bs::network::Asset::SpotFX) &&
      (rfq.side == bs::network::Side::Buy)) {
      settlement->recvAddress = bs::Address::fromAddressString(rfq.receiptAddress);
   }
   settlByRfqId_[rfq.requestId] = settlement;

   MatchingMessage msg;
   auto msgReq = msg.mutable_send_rfq();
   *msgReq = request.rfq();
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

bool SettlementAdapter::processSubmitQuote(const bs::message::Envelope& env
   , const ReplyToRFQ& request)
{
   const auto& itSettl = settlByRfqId_.find(request.quote().request_id());
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] RFQ {} not found", __func__, request.quote().request_id());
      return true;
   }
   logger_->debug("[{}] sess token: {}, account: {}, recv addr: {}", __func__
     , request.session_token(), request.account(), request.dealer_recv_addr());
   itSettl->second->quote = fromMsg(request.quote());
   itSettl->second->env = env;
   if (!request.dealer_recv_addr().empty()) {
      try {
         itSettl->second->dealerRecvAddress = bs::Address::fromAddressString(request.dealer_recv_addr());
      } catch (const std::exception&) {
         logger_->warn("[{}] invalid dealer recv address {}", __func__, request.dealer_recv_addr());
      }
   }

   BinaryData settlementId;
   try {
      settlementId = BinaryData::CreateFromHex(itSettl->second->quote.settlementId);
   }
   catch (const std::exception&) {
      logger_->warn("[{}] invalid settlementId format: {}", __func__, itSettl->second->quote.settlementId);
   }
   if (!settlementId.empty()) {
      if (!itSettl->second->settlementId.empty() && (itSettl->second->settlementId != settlementId)) {
         logger_->error("[{}] settlementId mismatch", __func__);
         return true;
      }
      itSettl->second->settlementId = settlementId;
      itSettl->second->reserveId = itSettl->second->quote.requestId;
      settlBySettlId_[settlementId] = itSettl->second;
   }

   MatchingMessage msg;
   *msg.mutable_submit_quote_notif() = request;
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

bool SettlementAdapter::processPullQuote(const bs::message::Envelope& env
   , const PullRFQReply& request)
{
   const auto& itSettl = settlByRfqId_.find(request.rfq_id());
   if (itSettl == settlByRfqId_.end()) {
      logger_->error("[{}] RFQ {} not found", __func__, request.rfq_id());
      return true;
   }
   if (!itSettl->second->env.sender || (itSettl->second->env.sender->value() != env.sender->value())) {
      logger_->error("[{}] invalid or different sender of quote submit", __func__);
      return true;
   }

   const auto& itSettlById = settlBySettlId_.find(itSettl->second->settlementId);
   if (itSettlById != settlBySettlId_.end()) {
      unreserve(itSettl->first);
      settlBySettlId_.erase(itSettlById);
   }

   MatchingMessage msg;
   auto msgData = msg.mutable_pull_quote_notif();
   *msgData = request;
   Envelope envReq{ 0, user_, userMtch_, {}, {}, msg.SerializeAsString(), true };
   return pushFill(envReq);
}

bool SettlementAdapter::processQuoteCancelled(const QuoteCancelled& request)
{
   const auto& itRFQ = settlByRfqId_.find(request.rfq_id());
   if (itRFQ == settlByRfqId_.end()) {
      logger_->error("[{}] unknown RFQ {}", __func__, request.rfq_id());
      return true;
   }
   const auto& itQuote = settlByQuoteId_.find(request.quote_id());
   if (itQuote == settlByQuoteId_.end()) {
      logger_->error("[{}] quote {} not found", __func__, request.quote_id());
      return true;
   }
   settlByQuoteId_.erase(itQuote);

   SettlementMessage msg;
   *msg.mutable_quote_cancelled() = request;
   Envelope env{ 0, user_, itRFQ->second->env.sender, {}, {}, msg.SerializeAsString() };
   return pushFill(env);
}

bool SettlementAdapter::processXbtTx(uint64_t msgId, const WalletsMessage_XbtTxResponse& response)
{
   const auto& itPayin = payinRequests_.find(msgId);
   if (itPayin != payinRequests_.end()) {
      const auto& itSettl = settlBySettlId_.find(itPayin->second);
      if (itSettl != settlBySettlId_.end()) {
         if (response.error_text().empty()) {
            itSettl->second->payin = bs::signer::pbTxRequestToCore(response.tx_request(), logger_);
            try {
               itSettl->second->settlementAddress = bs::Address::fromAddressString(response.settlement_address());
            }
            catch (const std::exception&) {
               logger_->error("[{}] invalid settlement address", __func__);
            }
            BsServerMessage msg;
            auto msgReq = msg.mutable_send_unsigned_payin();
            msgReq->set_settlement_id(itPayin->second.toBinStr());
            msgReq->set_tx(itSettl->second->payin.serializeState().SerializeAsString());
            Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
            pushFill(env);
         }
         else {
            SettlementMessage msg;
            auto msgResp = msg.mutable_failed_settlement();
            msgResp->set_rfq_id(itSettl->second->rfq.requestId);
            msgResp->set_settlement_id(itPayin->second.toBinStr());
            msgResp->set_info(response.error_text());
            Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}
               , msg.SerializeAsString() };
            pushFill(env);
            cancel(itPayin->second);
         }
      }
      else {
         logger_->error("[{}] settlement {} for payin not found", __func__
            , itPayin->second.toHexStr());
      }
      payinRequests_.erase(itPayin);
      return true;
   }
   const auto& itPayout = payoutRequests_.find(msgId);
   if (itPayout != payoutRequests_.end()) {
      const auto& itSettl = settlBySettlId_.find(itPayout->second);
      if (itSettl != settlBySettlId_.end()) {
         logger_->debug("[{}] got payout {}", __func__, itPayout->second.toHexStr());
         try {
            itSettl->second->settlementAddress = bs::Address::fromAddressString(response.settlement_address());
         } catch (const std::exception&) {
            logger_->error("[{}] invalid settlement address", __func__);
         }
         auto dlgData = getPayoutDialogData(QDateTime::currentDateTime(), *itSettl->second);
         SignerMessage msg;
         auto msgReq = msg.mutable_sign_settlement_tx();
         msgReq->set_settlement_id(itPayout->second.toBinStr());
         *msgReq->mutable_tx_request() = response.tx_request();
         *msgReq->mutable_details() = dlgData.toProtobufMessage();
         msgReq->set_contra_auth_pubkey(itSettl->second->counterKey.toBinStr());
         msgReq->set_own_key_first(true);
         Envelope env{ 0, user_, userSigner_, {}, {}, msg.SerializeAsString(), true };
         if (pushFill(env)) {
            payoutRequests_[env.id] = itPayout->second;
         }
      }
      else {
         logger_->error("[{}] settlement {} for payout not found", __func__
            , itPayout->second.toHexStr());
      }
      payoutRequests_.erase(itPayout);
      return true;
   }
   logger_->error("[{}] unknown XBT TX response #{}", __func__, msgId);
   return true;
}

bool SettlementAdapter::processSignedTx(uint64_t msgId
   , const SignerMessage_SignTxResponse& response)
{
   const auto& settlementId = BinaryData::fromString(response.id());
   logger_->debug("[{}] {}", __func__, settlementId.toHexStr());
   const auto& sendSignedTx = [this, response, settlementId](bool payin)
   {
      if (static_cast<bs::error::ErrorCode>(response.error_code()) == bs::error::ErrorCode::TxCancelled) {
         cancel(settlementId);
         return;
      }
      BsServerMessage msg;
      auto msgReq = payin ? msg.mutable_send_signed_payin() : msg.mutable_send_signed_payout();
      msgReq->set_settlement_id(settlementId.toBinStr());
      msgReq->set_tx(response.signed_tx());
      Envelope env{ 0, user_, userBS_, {}, {}, msg.SerializeAsString(), true };
      logger_->debug("[SettlementAdapter::processSignedTx::sendSignedTX] {}", BinaryData::fromString(response.signed_tx()).toHexStr());
      pushFill(env);
   };
   const auto& itPayin = payinRequests_.find(msgId);
   if (itPayin != payinRequests_.end()) {
      if (itPayin->second != settlementId) {
         logger_->error("[{}] payin settlement id {} mismatch", __func__
            , settlementId.toHexStr());
         payinRequests_.erase(itPayin);
         return true;   //TODO: decide the consequences of this
      }
      const auto& itSettl = settlBySettlId_.find(itPayin->second);
      payinRequests_.erase(itPayin);
      if (itSettl == settlBySettlId_.end()) {
         logger_->error("[{}] settlement for {} not found", __func__
            , settlementId.toHexStr());
         return true;
      }
      itSettl->second->handshakeComplete = true;
      sendSignedTx(true);
      return true;
   }

   const auto& itPayout = payoutRequests_.find(msgId);
   if (itPayout != payoutRequests_.end()) {
      if (itPayout->second != settlementId) {
         logger_->error("[{}] payout settlement id mismatch", __func__);
         payoutRequests_.erase(itPayout);
         return true;   //TODO: decide the consequences of this
      }
      try {
         const Tx tx(BinaryData::fromString(response.signed_tx()));
         pendingZCs_[tx.getThisHash()] = itPayout->second;
      }
      catch (const std::exception& e) {
         logger_->error("[{}] invalid signed payout TX: {}", __func__, e.what());
         cancel(itPayout->second);
         payoutRequests_.erase(itPayout);
         return true;
      }
      const auto& itSettl = settlBySettlId_.find(itPayout->second);
      payoutRequests_.erase(itPayout);
      if (itSettl == settlBySettlId_.end()) {
         logger_->error("[{}] settlement for {} not found", __func__
            , settlementId.toHexStr());
         return true;
      }
      itSettl->second->handshakeComplete = true;
      sendSignedTx(false);
      return true;
   }
   logger_->error("[{}] unknown signed TX #{}", __func__, msgId);
   return true;
}

bool SettlementAdapter::processHandshakeTimeout(const std::string& id)
{
   const auto& settlementId = BinaryData::fromString(id);
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl != settlBySettlId_.end()) {
      if (!itSettl->second->handshakeComplete) {
         logger_->error("[{}] settlement {} handshake timeout", __func__
            , settlementId.toHexStr());
         SettlementMessage msg;
         auto msgFail = msg.mutable_failed_settlement();
         msgFail->set_settlement_id(settlementId.toBinStr());
         msgFail->set_info("handshake timeout");
         Envelope env{ 0, user_, itSettl->second->env.sender, {}, {}
            , msg.SerializeAsString() };
         return pushFill(env);
      }
   }
   return true;
}

bool SettlementAdapter::processInRFQTimeout(const std::string& id)
{
   const auto& itSettl = settlByRfqId_.find(id);
   if (itSettl != settlByRfqId_.end()) {  // do nothing - just remove unanswered RFQ
      logger_->debug("[{}] {}", __func__, id);
      settlByRfqId_.erase(itSettl);
   }
   return true;
}

bool SettlementAdapter::startXbtSettlement(const bs::network::Quote& quote)
{
   const auto& sendFailedQuote = [this, quote](const std::string& info)
   {
      logger_->error("[SettlementAdapter::startXbtSettlement] {} - aborting "
         "settlement", info);
      unreserve(quote.requestId);
      SettlementMessage msg;
      auto msgResp = msg.mutable_failed_settlement();
      msgResp->set_rfq_id(quote.requestId);
      msgResp->set_quote_id(quote.quoteId);
      msgResp->set_info(info);
      Envelope env{ 0, user_, nullptr, {}, {}, msg.SerializeAsString() };
      pushFill(env);
   };
   const auto& itSettl = settlByQuoteId_.find(quote.quoteId);
   if (itSettl == settlByQuoteId_.end()) {
      sendFailedQuote("unknown quote id");
      return false;
   }
   if (quote.settlementId.empty()) {
      sendFailedQuote("no settlement id");
      settlByQuoteId_.erase(itSettl);
      return false;
   }

   CurrencyPair cp(quote.security);
   const bool isFxProd = (quote.product != bs::network::XbtCurrency);
   itSettl->second->fxProduct = cp.ContraCurrency(bs::network::XbtCurrency);
   const double amount = isFxProd ? quote.quantity / quote.price : quote.quantity;
   const auto xbtAmount = bs::XBTAmount(amount);
   itSettl->second->amount = xbtAmount;

   // BST-2545: Use price as it see Genoa (and it computes it as ROUNDED_CCY / XBT)
   const auto actualXbtPrice = UiUtils::actualXbtPrice(xbtAmount, quote.price);
   itSettl->second->actualXbtPrice = actualXbtPrice;

   const auto side = (quote.product == bs::network::XbtCurrency)
      ? bs::network::Side::invert(quote.side) : quote.side;
   itSettl->second->txComment = fmt::format("{} {} @ {}", bs::network::Side::toString(side)
      , quote.security, UiUtils::displayPriceXBT(actualXbtPrice).toStdString());
//   itSettl->second.dealerAddrValidationReqd = xbtAmount > bs::XBTAmount(tier1XbtLimit);

   BinaryData settlementId;
   try {
      settlementId = BinaryData::CreateFromHex(quote.settlementId);
   }
   catch (const std::exception&) {
      sendFailedQuote("invalid settlement id format");
      settlByQuoteId_.erase(itSettl);
      return false;
   }
   itSettl->second->settlementId = settlementId;

   auto &data = settlBySettlId_[settlementId] = itSettl->second;
   try {
      if (data->dealer) {
         if (data->ownKey.empty()) {
            data->ownKey = BinaryData::CreateFromHex(quote.dealerAuthPublicKey);
         }
         if (data->counterKey.empty()) {
            if (quote.requestorAuthPublicKey.empty()) {
               data->counterKey = BinaryData::CreateFromHex(data->rfq.requestorAuthPublicKey);
            }
            else {
               data->counterKey = BinaryData::CreateFromHex(quote.requestorAuthPublicKey);
            }
         }
      } else {
         data->ownKey = BinaryData::CreateFromHex(quote.requestorAuthPublicKey);
         data->counterKey = BinaryData::CreateFromHex(quote.dealerAuthPublicKey);
      }
      data->ownAuthAddr = bs::Address::fromPubKey(data->ownKey, AddressEntryType_P2WPKH);
      data->counterAuthAddr = bs::Address::fromPubKey(data->counterKey, AddressEntryType_P2WPKH);
   }
   catch (const std::exception&) {
      sendFailedQuote("failed to decode data");
      settlBySettlId_.erase(data->settlementId);
      settlByQuoteId_.erase(itSettl);
      return false;
   }

   const auto& timeNow = bs::message::bus_clock::now();
   SettlementMessage msg;
   msg.set_handshake_timeout(settlementId.toBinStr());
   Envelope env{ 0, user_, user_, timeNow, timeNow + kHandshakeTimeout
      , msg.SerializeAsString(), true };
   pushFill(env);

   if (data->dealerAddrValidationReqd) {
      //TODO: push counterAuthAddr to OnChainTracker for checking
   }

   if (!itSettl->second->dealer) {
      MatchingMessage msgMtch;
      auto msgReq = msgMtch.mutable_accept_rfq();
      msgReq->set_rfq_id(quote.requestId);
      toMsg(quote, msgReq->mutable_quote());
      msgReq->set_payout_tx("not used");  // copied from ReqXBTSettlementContainer
      Envelope envReq{ 0, user_, userMtch_, {}, {}, msgMtch.SerializeAsString(), true };
      return pushFill(envReq);
   }
   return true;
}

void SettlementAdapter::startCCSettlement(const bs::network::Quote&)
{
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

void SettlementAdapter::close(const BinaryData& settlementId)
{
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl == settlBySettlId_.end()) {
      return;
   }
   logger_->debug("[{}] {}", __func__, settlementId.toHexStr());
   unreserve(itSettl->second->rfq.requestId);
   settlByQuoteId_.erase(itSettl->second->quote.quoteId);
   settlBySettlId_.erase(itSettl);
}

void SettlementAdapter::cancel(const BinaryData& settlementId)
{
   const auto& itSettl = settlBySettlId_.find(settlementId);
   if (itSettl == settlBySettlId_.end()) {
      return;
   }
   const auto sender = itSettl->second->env.sender;
   close(settlementId);
   SettlementMessage msg;
   msg.set_settlement_cancelled(settlementId.toBinStr());
   Envelope env{ 0, user_, sender, {}, {}, msg.SerializeAsString() };
   pushFill(env);
}

/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TestAdapters.h"
#include <QDateTime>
#include <spdlog/spdlog.h>
#include "MessageUtils.h"
#include "ProtobufHeadlessUtils.h"
#include "TestEnv.h"

#include "common.pb.h"
#include "headless.pb.h"
#include "terminal.pb.h"

//#define MSG_DEBUGGING   // comment this out if msg id debug output is not needed

using namespace bs::message;
using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;

constexpr auto kExpirationTimeout = std::chrono::seconds{ 5 };

bool TestSupervisor::process(const Envelope &env)
{
#ifdef MSG_DEBUGGING
   StaticLogger::loggerPtr->debug("[{}] {}{}: {}({}) -> {}({}), {} bytes"
      , name(), env.request ? '/' : '\\', env.id, env.sender->name()
      , env.sender->value(), env.receiver ? env.receiver->name() : "null"
      , env.receiver ? env.receiver->value() : -1, env.message.size());
#endif   //MSG_DEBUGGING
   {
      std::unique_lock<std::mutex> lock(mtxWait_);
      std::vector<uint32_t> filtersToDelete;
      for (const auto &filter : filterMultWait_) {
         if (filter.second.first(env)) {
            const auto seqNo = filter.first;
            auto &filtPair = filterMap_[seqNo];
            filtPair.second.push_back(env);
            if (filtPair.second.size() >= filtPair.first) {
               filter.second.second->set_value(filtPair.second);
               filtersToDelete.push_back(seqNo);
               break;
            }
            return false;
         }
      }
      for (const auto &filter : filterWait_) {
         if (filter.second.first(env)) {
            const auto seqNo = filter.first;
            filter.second.second->set_value(env);
            filtersToDelete.push_back(seqNo);
            break;
         }
      }
      if (!filtersToDelete.empty()) {
         for (const auto &seqNo : filtersToDelete) {
            filterMultWait_.erase(seqNo);
            filterMap_.erase(seqNo);
            filterWait_.erase(seqNo);
         }
         return false;
      }
   }
   return true;
}

bs::message::SeqId TestSupervisor::send(bs::message::TerminalUsers sender, bs::message::TerminalUsers receiver
   , const std::string &message, bs::message::SeqId respId)
{
   return pushResponse(UserTerminal::create(sender), UserTerminal::create(receiver)
      , message, respId);
}

std::future<std::vector<bs::message::Envelope>> TestSupervisor::waitFor(const FilterCb &cb
   , size_t count, uint32_t *seqNoExt)
{
   const auto seqNo = ++seqNo_;
   if (seqNoExt != nullptr) {
      *seqNoExt = seqNo;
   }
   auto promWait = std::make_shared<std::promise<std::vector<bs::message::Envelope>>>();
   std::unique_lock<std::mutex> lock(mtxWait_);
   filterMultWait_[seqNo] = { cb, promWait };
   filterMap_[seqNo] = { count, {} };
   return promWait->get_future();
}

std::future<bs::message::Envelope> TestSupervisor::waitFor(const FilterCb &cb
   , uint32_t *seqNoExt)
{
   const auto seqNo = ++seqNo_;
   if (seqNoExt != nullptr) {
      *seqNoExt = seqNo;
   }
   auto promWait = std::make_shared<std::promise<bs::message::Envelope>>();
   std::unique_lock<std::mutex> lock(mtxWait_);
   filterWait_[seqNo] = { cb, promWait };
   return promWait->get_future();
}

void TestSupervisor::unwaitFor(const uint32_t seqNo)
{
   std::unique_lock<std::mutex> lock(mtxWait_);
   filterMultWait_.erase(seqNo);
   filterMap_.erase(seqNo);
   filterWait_.erase(seqNo);
}


MatchingMock::MatchingMock(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& name, const std::string &email
   , const std::shared_ptr<ArmoryInstance>& armoryInst)
   : logger_(logger), name_(name), email_(email)
   , armoryInst_(armoryInst)
   , user_(UserTerminal::create(TerminalUsers::Matching))
   , userBS_(UserTerminal::create(TerminalUsers::BsServer))
   , userSettl_(UserTerminal::create(TerminalUsers::Settlement))
{}

bool MatchingMock::process(const bs::message::Envelope& env)
{
   if (env.isRequest() && (env.receiver->value() == user_->value())) {
      MatchingMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own request #{}", __func__, env.id());
         return true;
      }
      switch (msg.data_case()) {
      case MatchingMessage::kSendRfq:
         if (siblings_.empty()) {
            logger_->warn("[{}] {}: no one to notify", __func__, name());
            //TODO: send result back to requester
         }
         else {
            matches_[msg.send_rfq().id()] = { {}, msg.send_rfq().auth_pub_key() };
            for (const auto& sibling : siblings_) {
               sibling->inject(msg, email_);
            }
         }
         break;

      case MatchingMessage::kSubmitQuoteNotif:
         if (siblings_.empty()) {
            logger_->error("[{}] {}: no siblings", __func__, name());
         } else {
            auto quote = msg.submit_quote_notif().quote();
            const auto& itMatch = matches_.find(quote.request_id());
            if (itMatch == matches_.end()) {
               logger_->info("[{}] {}: not our quote", __func__, name());
               break;
            }
            quote.set_quote_id(CryptoPRNG::generateRandom(6).toHexStr());
            quote.set_quoting_type((int)bs::network::Quote::Tradeable);
            auto msgCopy = msg;
            *msgCopy.mutable_submit_quote_notif()->mutable_quote() = quote;
            itMatch->second.quote = fromMsg(msgCopy.submit_quote_notif().quote());
            for (const auto& sibling : siblings_) {
               sibling->inject(msgCopy, email_);
            }
            const auto& timeNow = bs::message::bus_clock::now();
            pushRequest(user_, user_, quote.quote_id()    //FIXME: put actual quote's expirationTime
               , timeNow + kExpirationTimeout);

            MatchingMessage msgSettl;
            toMsg(itMatch->second.quote, msgSettl.mutable_quote());
            pushResponse(user_, userSettl_, msgSettl.SerializeAsString());
         }
         break;

      case MatchingMessage::kOrder: {  // only requester's processing
         const auto& status = static_cast<bs::network::Order::Status>(msg.order().status());
         for (auto& match : matches_) {
            if (match.second.quote.quoteId == msg.order().quote_id()) {
               if ((status == bs::network::Order::Filled) || (status == bs::network::Order::Failed)) {
                  matches_.erase(match.first);
               }
               else {
                  match.second.order = fromMsg(msg.order());
               }
               return pushResponse(user_, userSettl_, msg.SerializeAsString());
            }
         }
      }
         break;

      case MatchingMessage::kAcceptRfq: { // only requester's processing
         const auto& itMatch = matches_.find(msg.accept_rfq().rfq_id());
         if (itMatch == matches_.end()) {
            logger_->warn("[{}] not our RFQ {}", __func__, msg.accept_rfq().rfq_id());
            break;
         }
         if (itMatch->second.quote.assetType == bs::network::Asset::SpotFX) {
            sendFilledOrder(itMatch->first);
            matches_.erase(itMatch);
            for (const auto& sibling : siblings_) {
               sibling->inject(msg, email_);
            }
         }
      }
         break;
      default: break;
      }
   }
   if (env.isRequest() && (env.receiver->value() == userBS_->value())) {
      BsServerMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse own request #{}", __func__, env.id());
         return true;
      }
      switch (msg.data_case()) {
      case BsServerMessage::kSendUnsignedPayin:
         return processUnsignedPayin(msg.send_unsigned_payin());
      case BsServerMessage::kSendSignedPayin:
         return processSignedTX(msg.send_signed_payin(), true, true);
      case BsServerMessage::kSendSignedPayout:
         return processSignedTX(msg.send_signed_payout(), false, true);
      default: break;
      }
   }
   else if (env.receiver->value() == env.sender->value()
      && (env.sender->value() == user_->value())) {   //own to self
      for (const auto& match : matches_) {
         if (match.second.quote.quoteId == env.message) {
            return sendPendingOrder(match.first);
         }
      }
   }
   return true;
}

bool MatchingMock::processBroadcast(const bs::message::Envelope&)
{
   return false;
}

void MatchingMock::link(const std::shared_ptr<MatchingMock>& sibling)
{
   siblings_.insert(sibling);
}

bool MatchingMock::inject(const MatchingMessage& msg, const std::string &email)
{
   switch (msg.data_case()) {
   case MatchingMessage::kSendRfq:
      return sendIncomingRFQ(msg.send_rfq(), email);
   case MatchingMessage::kSubmitQuoteNotif:
      return sendQuoteReply(msg.submit_quote_notif(), email);
   case MatchingMessage::kAcceptRfq:   // only for responder
      return sendFilledOrder(msg.accept_rfq().rfq_id());

   case MatchingMessage::kOrder: // only for requester
      for (auto& match : matches_) {
         if (match.second.quote.quoteId == msg.order().quote_id()) {
            match.second.order = fromMsg(msg.order());
            return pushResponse(user_, userSettl_, msg.SerializeAsString());
         }
      }
      logger_->warn("[{}] match for {} not found", __func__, msg.order().quote_id());
      break;
   default: break;
   }
   return true;
}

bool MatchingMock::inject(const BsServerMessage& msg, const std::string& email)
{
   switch (msg.data_case()) {
   case BsServerMessage::kUnsignedPayinRequested:
      return sendUnsignedPayinRequest(msg.unsigned_payin_requested());
   case BsServerMessage::kSignedPayoutRequested:
      return sendSignedPayoutRequest(msg.signed_payout_requested());
   case BsServerMessage::kSignedPayinRequested:
      return sendSignedPayinRequest(msg.signed_payin_requested());
   case BsServerMessage::kSendSignedPayin:
      return processSignedTX(msg.send_signed_payin(), true);
   case BsServerMessage::kSendSignedPayout:
      return processSignedTX(msg.send_signed_payout(), false);
   default: break;
   }
   return true;
}

bool MatchingMock::sendIncomingRFQ(const RFQ& rfq, const std::string &email)
{
   MatchingMessage msg;
   auto msgInRFQ = msg.mutable_incoming_rfq();
   *msgInRFQ->mutable_rfq() = rfq;
   Match match{ CryptoPRNG::generateRandom(4).toHexStr() };
   msgInRFQ->set_session_token(match.sesToken);
   if (static_cast<bs::network::Asset::Type>(rfq.asset_type()) == bs::network::Asset::SpotXBT) {
      msgInRFQ->set_settlement_id(CryptoPRNG::generateRandom(32).toHexStr());
   }
   msgInRFQ->set_party(email);
   const auto& timeNow = std::chrono::system_clock::now();
   msgInRFQ->set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
      timeNow.time_since_epoch()).count());
   msgInRFQ->set_expiration_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
      (timeNow + kExpirationTimeout).time_since_epoch()).count());
   matches_[rfq.id()] = std::move(match);
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingMock::sendQuoteReply(const ReplyToRFQ& reply, const std::string& email)
{
   const auto& itMatch = matches_.find(reply.quote().request_id());
   if (itMatch == matches_.end()) {
      logger_->info("[{}] {}: not our quote", __func__, name());
      return true;
   }
   itMatch->second.quote = fromMsg(reply.quote());
   itMatch->second.quote.requestorAuthPublicKey = itMatch->second.reqAuthPubKey;
   itMatch->second.sesToken = reply.session_token();
   return sendQuote(itMatch->second.quote);
}

bool MatchingMock::sendQuote(const bs::network::Quote& quote)
{
   MatchingMessage msg;
   toMsg(quote, msg.mutable_quote());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingMock::sendPendingOrder(const std::string& rfqId)
{
   const auto& itMatch = matches_.find(rfqId);
   if (itMatch == matches_.end()) {
      logger_->error("[{}] {}: unknown RFQ {}", __func__, name(), rfqId);
      return true;
   }
   bs::network::Order order;
   order.clOrderId = CryptoPRNG::generateRandom(7).toHexStr();
   order.exchOrderId = QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr());
   order.quoteId = itMatch->second.quote.quoteId;
   order.dateTime = QDateTime::currentDateTime();
   order.security = itMatch->second.quote.security;
   order.product = itMatch->second.quote.product;
   order.settlementId = BinaryData::CreateFromHex(itMatch->second.quote.settlementId);
   order.quantity = itMatch->second.quote.quantity;
   order.price = itMatch->second.quote.price;
   order.avgPx = itMatch->second.quote.price;
   order.assetType = itMatch->second.quote.assetType;
   order.side = itMatch->second.quote.side;
   order.status = bs::network::Order::Pending;
   itMatch->second.order = order;
   MatchingMessage msg;
   toMsg(order, msg.mutable_order());
   pushResponse(user_, userSettl_, msg.SerializeAsString());

   order.side = bs::network::Side::invert(order.side);
   toMsg(order, msg.mutable_order());
   for (const auto& sibling : siblings_) {
      sibling->inject(msg, email_);
   }

   if (itMatch->second.quote.assetType == bs::network::Asset::SpotXBT) {
      const auto& settlementId = BinaryData::CreateFromHex(itMatch->second.quote.settlementId);
      BsServerMessage msgBS;
      msgBS.set_unsigned_payin_requested(settlementId.toBinStr());
      if (itMatch->second.isSellXBT()) {
         for (const auto& sibling : siblings_) {
            sibling->inject(msgBS, email_);
         }
      }
      else {
         pushResponse(userBS_, userSettl_, msgBS.SerializeAsString());
      }
   }
   return true;
}

bool MatchingMock::sendFilledOrder(const std::string& rfqId)
{
   const auto& itMatch = matches_.find(rfqId);
   if (itMatch == matches_.end()) {
      return true;   // not our RFQ
   }
   itMatch->second.order.status = bs::network::Order::Filled;
   MatchingMessage msg;
   toMsg(itMatch->second.order, msg.mutable_order());
   return pushResponse(user_, userSettl_, msg.SerializeAsString());
}

bool MatchingMock::sendUnsignedPayinRequest(const std::string& settlIdBin)
{
   const auto& settlIdHex = BinaryData::fromString(settlIdBin).toHexStr();
   const auto& itMatch = std::find_if(matches_.cbegin(), matches_.cend()
      , [settlIdHex](const std::pair<std::string, Match>& match) {
      return (match.second.quote.settlementId == settlIdHex);
   });
   if (itMatch == matches_.end()) {
      logger_->warn("[{}] can't find match with settlement id {}", __func__, settlIdHex);
      return true;
   }
   BsServerMessage msgBS;
   msgBS.set_unsigned_payin_requested(settlIdBin);
   return pushResponse(userBS_, userSettl_, msgBS.SerializeAsString());
}

bool MatchingMock::sendSignedPayinRequest(const BsServerMessage_SignXbtHalf& request)
{
   const auto& settlIdHex = BinaryData::fromString(request.settlement_id()).toHexStr();
   const auto& itMatch = std::find_if(matches_.cbegin(), matches_.cend()
      , [settlIdHex](const std::pair<std::string, Match>& match) {
      return (match.second.quote.settlementId == settlIdHex);
   });
   if (itMatch == matches_.end()) {
      logger_->warn("[{}] can't find match with settlement id {}", __func__, settlIdHex);
      return true;
   }
   BsServerMessage msgBS;
   *msgBS.mutable_signed_payin_requested() = request;
   return pushResponse(userBS_, userSettl_, msgBS.SerializeAsString());
}

bool MatchingMock::sendSignedPayoutRequest(const BsServerMessage_SignXbtHalf& request)
{
   const auto& settlIdHex = BinaryData::fromString(request.settlement_id()).toHexStr();
   const auto& itMatch = std::find_if(matches_.cbegin(), matches_.cend()
      , [settlIdHex](const std::pair<std::string, Match>& match) {
      return (match.second.quote.settlementId == settlIdHex);
   });
   if (itMatch == matches_.end()) {
      logger_->warn("[{}] can't find match with settlement id {}", __func__, settlIdHex);
      return true;
   }
   BsServerMessage msgBS;
   *msgBS.mutable_signed_payout_requested() = request;
   return pushResponse(userBS_, userSettl_, msgBS.SerializeAsString());
}

bool MatchingMock::processUnsignedPayin(const BsServerMessage_XbtTransaction& response)
{
   Codec_SignerState::SignerState signerState;
   if (!signerState.ParseFromString(response.tx())) {
      logger_->error("[{}] failed to parse SignerState", __func__);
      return true;   //TODO: send settlement failure?
   }
   bs::core::wallet::TXSignRequest txReq;
   txReq.armorySigner_.deserializeState(signerState);
   const auto& payinHash = txReq.txId();
   if (payinHash.empty()) {
      logger_->error("[{}] invalid payin hash", __func__);
      return true;   //TODO: send settlement failure?
   }
   BsServerMessage msg;
   auto msgReq = msg.mutable_signed_payout_requested();
   msgReq->set_settlement_id(response.settlement_id());
   msgReq->set_payin_hash(payinHash.toBinStr());
   for (const auto& sibling : siblings_) {
      sibling->inject(msg, email_);
   }
   msgReq = msg.mutable_signed_payin_requested();
   msgReq->set_settlement_id(response.settlement_id());
   msgReq->set_unsigned_payin(response.tx());
   msgReq->set_payin_hash(payinHash.toBinStr());
   const auto curTime = QDateTime::currentDateTime();
   msgReq->set_timestamp(curTime.toMSecsSinceEpoch());
   return pushResponse(userBS_, userSettl_, msg.SerializeAsString());
}

bool MatchingMock::processSignedTX(const BsServerMessage_XbtTransaction& response
   , bool payin, bool recurse)
{
   const auto& settlIdHex = BinaryData::fromString(response.settlement_id()).toHexStr();
   const auto &itMatch = std::find_if(matches_.begin(), matches_.end()
      , [settlIdHex](const std::pair<std::string, Match> &match) {
      return (match.second.quote.settlementId == settlIdHex);
   });
   if (itMatch == matches_.end()) {
      logger_->warn("[{}] unknown settlement id {}", __func__, settlIdHex);
      return true;
   }
   {
      const auto& tx = BinaryData::fromString(response.tx());
      if (payin) {
         itMatch->second.signedPayin = tx;
      } else {
         itMatch->second.signedPayout = tx;
      }
   }
   if (!itMatch->second.signedPayin.empty() && !itMatch->second.signedPayout.empty()) {
      armoryInst_->pushZC(itMatch->second.signedPayin);
      armoryInst_->pushZC(itMatch->second.signedPayout);

      //TODO: monitor new blocks and provide gradual order status update on each conf
      sendFilledOrder(itMatch->first);
   }
   else if (recurse) {
      BsServerMessage msg;
      if (payin) {
         *msg.mutable_send_signed_payin() = response;
      } else {
         *msg.mutable_send_signed_payout() = response;
      }
      for (const auto& sibling : siblings_) {
         sibling->inject(msg, email_);
      }
   }
   return true;
}

bool MatchingMock::Match::isSellXBT() const
{
   return ((quote.side == bs::network::Side::Buy)
      || (quote.product != bs::network::XbtCurrency));
}

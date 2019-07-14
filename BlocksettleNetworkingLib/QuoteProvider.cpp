#include "QuoteProvider.h"

#include "AssetManager.h"
#include "CelerCancelOrderSequence.h"
#include "CelerCancelQuoteNotifSequence.h"
#include "CelerCancelRFQSequence.h"
#include "CelerClient.h"
#include "CelerCreateFxOrderSequence.h"
#include "CelerCreateOrderSequence.h"
#include "CelerSignTxSequence.h"
#include "CelerSubmitQuoteNotifSequence.h"
#include "CelerSubmitRFQSequence.h"
#include "CurrencyPair.h"
#include "FastLock.h"

#include "DownstreamQuoteProto.pb.h"
#include "DownstreamOrderProto.pb.h"
#include "bitcoin/DownstreamBitcoinTransactionSigningProto.pb.h"

#include <spdlog/spdlog.h>
#include <chrono>

#include <QDateTime>

using namespace bs::network;
using namespace com::celertech::marketmerchant::api::enums::orderstatus;
using namespace com::celertech::marketmerchant::api::enums::producttype::quotenotificationtype;
using namespace com::celertech::marketmerchant::api::enums::side;
using namespace com::celertech::marketmerchant::api::order;
using namespace com::celertech::marketmerchant::api::quote;

bool QuoteProvider::isRepliableStatus(const bs::network::QuoteReqNotification::Status status)
{
   return ((status == bs::network::QuoteReqNotification::PendingAck)
      || (status == bs::network::QuoteReqNotification::Replied));
}

SecureBinaryData bs::PayinsContainer::get(const std::string& settlementId) const
{
   SecureBinaryData payin;
   {
      FastLock locker(payInsLock_);

      auto it = payIns_.find(settlementId);
      if (it != payIns_.end()) {
         payin = it->second;
      }
   }

   if (payin.isNull()) {
      logger_->error("[PayinsContainer::get] Could not find payin transaction for {}", settlementId);
   }
   return payin;
}

bool bs::PayinsContainer::save(const std::string& settlementId, const SecureBinaryData& payin)
{
   size_t count = 0;
   bool inserted = false;

   {
      FastLock locker(payInsLock_);

      auto it = payIns_.find(settlementId);
      if (it == payIns_.end()) {
         payIns_.emplace(settlementId, payin);
         inserted = true;
      }
      else {
         it->second = payin;
      }

      count = payIns_.size();
   }

   if (inserted) {
      logger_->debug("[PayinsContainer::save] save payin transaction for {}. Current count {}"
         , settlementId, count);
   }
   else {
      logger_->debug("[PayinsContainer::save] payin transaction replaced for {}. Current count {}"
         , settlementId, count);
   }
   return inserted;
}

bool bs::PayinsContainer::erase(const std::string& settlementId)
{
   size_t count = 0;
   bool erased = false;

   {
      FastLock locker(payInsLock_);

      auto it = payIns_.find(settlementId);
      if (it != payIns_.end()) {
         payIns_.erase(it);
         erased = true;
      }

      count = payIns_.size();
   }

   if (erased) {
      logger_->debug("[PayinsContainer::erase] erased payin transaction for {}. Current count {}"
         , settlementId, count);
   }
   else {
      logger_->debug("[PayinsContainer::erase] no payin transaction for {}. Current count {}"
         , settlementId, count);
   }
   return erased;
}


QuoteProvider::QuoteProvider(const std::shared_ptr<AssetManager>& assetManager
      , const std::shared_ptr<spdlog::logger>& logger
      , bool debugTraffic)
 : logger_(logger)
 , assetManager_(assetManager)
 , dealerPayins_(logger)
 , celerLoggedInTimestampUtcInMillis_(0)
 , debugTraffic_(debugTraffic)
{
}

QuoteProvider::~QuoteProvider() noexcept = default;

void QuoteProvider::ConnectToCelerClient(const std::shared_ptr<BaseCelerClient>& celerClient)
{
   celerClient_ = celerClient;

   celerClient->RegisterHandler(CelerAPI::QuoteDownstreamEventType, [this] (const std::string& data) {
      return this->onQuoteResponse(data);
   });
   celerClient->RegisterHandler(CelerAPI::QuoteRequestRejectDownstreamEventType, [this](const std::string &data) {
      return this->onQuoteReject(data);
   });
   celerClient->RegisterHandler(CelerAPI::CreateOrderRequestRejectDownstreamEventType, [this](const std::string &data) {
      return this->onOrderReject(data);
   });
   celerClient->RegisterHandler(CelerAPI::BitcoinOrderSnapshotDownstreamEventType, [this] (const std::string& data) {
      return this->onBitcoinOrderSnapshot(data);
   });
   celerClient->RegisterHandler(CelerAPI::FxOrderSnapshotDownstreamEventType, [this](const std::string& data) {
      return this->onFxOrderSnapshot(data);
   });
   celerClient->RegisterHandler(CelerAPI::QuoteCancelDownstreamEventType, [this](const std::string& data) {
      return this->onQuoteCancelled(data);
   });
   celerClient->RegisterHandler(CelerAPI::SignTransactionNotificationType, [this](const std::string& data) {
      return this->onSignTxNotif(data);
   });
   celerClient->RegisterHandler(CelerAPI::QuoteAckDownstreamEventType, [this](const std::string &data) {
      return this->onQuoteAck(data);
   });

   celerClient->RegisterHandler(CelerAPI::QuoteRequestNotificationType, [this] (const std::string& data) {
      return this->onQuoteReqNotification(data);
   });
   celerClient->RegisterHandler(CelerAPI::QuoteCancelNotifReplyType, [this](const std::string& data) {
      return this->onQuoteNotifCancelled(data);
   });

   connect(celerClient.get(), &BaseCelerClient::OnConnectedToServer, this, &QuoteProvider::onConnectedToCeler);
}

void QuoteProvider::onConnectedToCeler()
{
   const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   celerLoggedInTimestampUtcInMillis_ =  timestamp.count();

   auto sequence = std::make_shared<CelerFindAllOrdersSequence>(logger_);
   sequence->SetCallback([this](const CelerFindAllOrdersSequence::Messages &msgs) {
      for (const auto &msg : msgs) {
         switch (msg.type) {
         case CelerAPI::FxOrderSnapshotDownstreamEventType:
            onFxOrderSnapshot(msg.payload, true);
            break;

         case CelerAPI::BitcoinOrderSnapshotDownstreamEventType:
            onBitcoinOrderSnapshot(msg.payload, true);
            break;

         default:
            logger_->debug("[QuoteProvider::onConnectedToCeler] unknown msg type {}", msg.type);
            break;
         }
      }
   });
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::onConnectedToCeler] failed to execute CelerFindAllOrdersSequence");
   }
}

bool QuoteProvider::onQuoteResponse(const std::string& data)
{
   QuoteDownstreamEvent response;

   if (!response.ParseFromString(data)) {
         logger_->error("[QuoteProvider::onQuoteResponse] Failed to parse QuoteDownstreamEvent");
         return false;
   }
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteResponse]: {}", response.DebugString());
   }

   Quote quote;

   quote.quoteId = response.quoteid();
   quote.requestId = response.quoterequestid();
   quote.security = response.securitycode();
   quote.assetType = bs::network::Asset::fromCelerProductType(response.producttype());
   quote.side = bs::network::Side::fromCeler(response.side());

   if (quote.assetType == bs::network::Asset::PrivateMarket) {
      quote.dealerAuthPublicKey = response.dealerreceiptaddress();
      quote.dealerTransaction = response.dealercointransactioninput();
   }

   switch (response.quotingtype())
   {
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

   logger_->debug("[QuoteProvider::onQuoteResponse] timeSkew = {}", quote.timeSkewMs);
   CurrencyPair cp(quote.security);

   const auto itRFQ = submittedRFQs_.find(response.quoterequestid());
   if (itRFQ == submittedRFQs_.end()) {   // Quote for dealer to indicate GBBO
      const auto quoteCcy = getQuoteRequestCcy(quote.requestId);
      if (!quoteCcy.empty()) {
         double price = 0;

         if ((quote.side == bs::network::Side::Sell) ^ (quoteCcy != cp.NumCurrency())) {
            price = response.offerpx();
         } else {
            price = response.bidpx();
         }

         const bool own = IsOwnPrice(response.quoterequestid(), price);

         emit bestQuotePrice(QString::fromStdString(response.quoterequestid()), price, own);

         // it is tradeable. so will be deleted from dealing tab. need to remove saved price
         if (response.quotingtype() == com::celertech::marketmerchant::api::enums::quotingtype::TRADEABLE) {
            RemoveQuotePrice(response.quoterequestid());
         }
      }
   }
   else {
      if (response.legquotegroup_size() != 1) {
         logger_->error("[QuoteProvider::onQuoteResponse] invalid leg number: {}\n{}"
            , response.legquotegroup_size()
            , response.DebugString());
         return false;
      }

      const auto& grp = response.legquotegroup(0);

      if (quote.assetType == bs::network::Asset::SpotXBT) {
         quote.dealerAuthPublicKey = response.dealerauthenticationaddress();
         quote.requestorAuthPublicKey = itRFQ->second.requestorAuthPublicKey;

         if (response.has_settlementid() && !response.settlementid().empty()) {
            quote.settlementId = response.settlementid();
         }

         quote.dealerTransaction = response.dealertransaction();
         if (quote.dealerTransaction.empty()) {
            logger_->warn("[QuoteProvider::onQuoteResponse] empty dealerTransaction will not allow to send requestorTransaction");
         }
      }

      if ((quote.side == bs::network::Side::Sell) ^ (itRFQ->second.product != cp.NumCurrency())) {
         quote.price = response.offerpx();
         quote.quantity = grp.offersize();
      }
      else {
         quote.price = response.bidpx();
         quote.quantity = grp.bidsize();
      }

      quote.product = grp.currency();

      if (quote.quotingType == bs::network::Quote::Tradeable) {
         submittedRFQs_.erase(itRFQ);
      }
   }

   saveQuoteReqId(quote.requestId, quote.quoteId);
   emit quoteReceived(quote);
   return true;
}

static QString getQuoteRejReason(const com::celertech::marketmerchant::api::enums::quoterequestrejectreason::QuoteRequestRejectReason &reason)
{
   switch (reason)
   {
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::UNKNOWN_SYMBOL:
      return QObject::tr("Unknown symbol");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::EXCHANGE:
      return  QObject::tr("Exchange reject");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::QUOTE_REQUEST_EXCEEDS_LIMIT:
      return  QObject::tr("Exceeds limit");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::TOO_LATE_TO_ENTER:
      return  QObject::tr("Too late");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::INVALID_PRICE:
      return  QObject::tr("Invalid price");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::NOT_AUTHORIZED_TO_REQUEST_QUOTE:
      return  QObject::tr("Not authorized");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::NO_MATCH_FOR_INQUIRY:
      return  QObject::tr("No match for inquiry");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::NO_MARKET_FOR_INSTRUMENT:
      return  QObject::tr("No market for instrument");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::NO_INVENTORY:
      return  QObject::tr("No inventory");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::PASS:
      return  QObject::tr("Pass");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::INSUFFICIENT_CREDIT:
      return  QObject::tr("Insufficient credit");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::OTHER:
      return  QObject::tr("Other reason");
   case com::celertech::marketmerchant::api::enums::quoterequestrejectreason::UNABLE_TO_QUOTE:
      return  QObject::tr("Unable to quote");
   default:
      return  QObject::tr("Unknown reason");
   }
}

bool QuoteProvider::onQuoteReject(const std::string& data)
{
   QuoteRequestRejectDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onQuoteReject] Failed to parse QuoteRequestRejectDownstreamEvent");
      return false;
   }
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteReject] {}", response.DebugString());
   }

   QString text;
   if (response.quoterequestrejectgroup_size() > 0) {
      const QuoteRequestRejectGroup &rejGrp = response.quoterequestrejectgroup(0);
      text = QString::fromStdString(rejGrp.text());
   }
   if (text.isEmpty()) {
      text = getQuoteRejReason(response.quoterequestrejectreason());
   }
   emit quoteRejected(QString::fromStdString(response.quoterequestid()), text);

   return true;
}

static QString getQuoteRejReason(const com::celertech::marketmerchant::api::enums::quoterejectreason::QuoteRejectReason &reason)
{
   switch (reason)
   {
   case com::celertech::marketmerchant::api::enums::quoterejectreason::UNKNOWN_SYMBOL:
      return QObject::tr("Unknown symbol");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::EXCHANGE_CLOSED:
      return  QObject::tr("Exchange closed");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::QUOTE_REQUEST_EXCEEDS_LIMIT:
      return  QObject::tr("Exceeds limit");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::TOO_LATE_TO_ENTER:
      return  QObject::tr("Too late");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::INVALID_PRICE:
      return  QObject::tr("Invalid price");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::UNKNOWN_QUOTE:
      return  QObject::tr("Unknown quote");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::DUPLICATE_QUOTE:
      return  QObject::tr("Duplicate quote");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::NOT_AUTHORIZED_TO_QUOTE_SECURITY:
      return  QObject::tr("Not authorized to quote security");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::PRICE_EXCEEDS_CURRENT_PRICE_BAND:
      return  QObject::tr("Price exceeds current price band");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::QUOTE_LOCKED_UNABLE_TO_UPDATE_CANCEL:
      return  QObject::tr("Quote is locked");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::INVALID_OR_UNKNOWN_SECURITY_ISSUER:
      return  QObject::tr("Invalid security issuer");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::INVALID_OR_UNKNOW_ISSUER_OF_UNDERLYING_SECURITY:
      return  QObject::tr("Invalid underlying issuer");
   case com::celertech::marketmerchant::api::enums::quoterejectreason::OTHER:
      return  QObject::tr("Other reason");
   default:
      return  QObject::tr("Unknown reason");
   }
}

bool QuoteProvider::onQuoteAck(const std::string& data)
{
   QuoteAcknowledgementDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onQuoteAck] Failed to parse QuoteAcknowledgementDownstreamEvent");
      return false;
   }

   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteAck] {}", response.DebugString());
   }

   switch (response.quotestatus()) {
   case com::celertech::marketmerchant::api::enums::quotestatus::REJECTED:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCEL_FOR_SYMBOLS:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCELED_DUE_TO_LOCK_MARKET:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCELED_DUE_TO_CROSS_MARKET:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCELED_FOR_SECURITY_TYPES:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCELED_FOR_UNDERLYING:
   case com::celertech::marketmerchant::api::enums::quotestatus::CANCELED:
      {
         auto text = QString::fromStdString(response.text());
         if (text.isEmpty()) {
            text = getQuoteRejReason(response.quoterejectreason());
         }
         emit quoteRejected(QString::fromStdString(response.quoterequestid()), text);
      }
      break;

   default: break;
   }

   return true;
}

bool QuoteProvider::onOrderReject(const std::string& data)
{
   CreateOrderRequestRejectDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onQuoteReject] Failed to parse CreateOrderRequestRejectDownstreamEvent");
      return false;
   }

   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onOrderReject] {} ", response.DebugString());
   }
   emit orderRejected(QString::fromStdString(response.externalclorderid()), QString::fromStdString(response.rejectreason()));

   return true;
}

void QuoteProvider::SubmitRFQ(const bs::network::RFQ& rfq)
{
   if (!assetManager_->HaveAssignedAccount()) {
      logger_->error("[QuoteProvider::SubmitRFQ] submitting RFQ with empty account name");
   }
   auto sequence = std::make_shared<CelerSubmitRFQSequence>(assetManager_->GetAssignedAccount(), rfq, logger_, debugTraffic_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::SubmitRFQ] failed to execute CelerSubmitRFQSequence");
   } else {
      logger_->debug("[QuoteProvider::SubmitRFQ] RFQ submitted {}", rfq.requestId);
      submittedRFQs_[rfq.requestId] = rfq;
   }
}

void QuoteProvider::AcceptQuote(const QString &reqId, const Quote& quote, const std::string &payoutTx)
{
   if (!assetManager_->HaveAssignedAccount()) {
      logger_->error("[QuoteProvider::AcceptQuote] accepting XBT quote with empty account name");
   }
   auto sequence = std::make_shared<CelerCreateOrderSequence>(assetManager_->GetAssignedAccount(), reqId, quote, payoutTx, logger_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::AcceptQuote] failed to execute CelerCreateOrderSequence");
   } else {
      logger_->debug("[QuoteProvider::AcceptQuote] Order submitted");
   }
}

void QuoteProvider::AcceptQuoteFX(const QString &reqId, const Quote& quote)
{
   if (!assetManager_->HaveAssignedAccount()) {
      logger_->error("[QuoteProvider::AcceptQuoteFX] accepting FX quote with empty account name");
   }
   auto sequence = std::make_shared<CelerCreateFxOrderSequence>(assetManager_->GetAssignedAccount(), reqId, quote, logger_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::AcceptQuoteFX] failed to execute CelerCreateFxOrderSequence");
   }
   else {
      logger_->debug("[QuoteProvider::AcceptQuoteFX] FX Order submitted");
   }
}

void QuoteProvider::CancelQuote(const QString &reqId)
{
   auto sequence = std::make_shared<CelerCancelRFQSequence>(reqId, logger_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::CancelQuote] failed to execute CelerCancelRFQSequence sequence");
   }
   else {
      logger_->debug("[QuoteProvider::CancelQuote] RFQ {} cancelled", reqId.toStdString());
   }
}

void QuoteProvider::SignTxRequest(const QString &orderId, const std::string &txData)
{
   auto sequence = std::make_shared<CelerSignTxSequence>(orderId, txData, logger_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::SignTxRequest] failed to execute CelerSignTxSequence sequence");
   }
   else {
      logger_->debug("[QuoteProvider::SignTxRequest] Signed TX sent on {}", orderId.toStdString());
   }
}

static bs::network::Order::Status mapBtcOrderStatus(OrderStatus status)
{
   switch (status) {
      case FILLED:   return Order::Filled;
      case REJECTED: return Order::Failed;
      case PENDING_NEW: return Order::Pending;
      case NEW:      return Order::New;
      default:       return Order::Pending;
   }
   return Order::Pending;
}

static bs::network::Order::Status mapFxOrderStatus(OrderStatus status)
{
   switch (status) {
      case FILLED:   return Order::Filled;
      case REJECTED: return Order::Failed;
      case PENDING_NEW:
      case NEW:      return Order::New;
      default:       return Order::Pending;
   }
   return Order::Pending;
}

bool QuoteProvider::onBitcoinOrderSnapshot(const std::string& data, bool resync)
{
   BitcoinOrderSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
         logger_->error("[QuoteProvider::onBitcoinOrderSnapshot] Failed to parse BitcoinOrderSnapshotDownstreamEvent");
         return false;
   }
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onBitcoinOrderSnapshot] {}", response.DebugString());
   }

   auto orderDate = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   auto ageSeconds = orderDate.secsTo(QDateTime::currentDateTime());

   Order order;
   order.exchOrderId = QString::number(response.orderid());
   order.clOrderId = response.externalclorderid();
   order.quoteId = response.quoteid();
   order.dateTime = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   order.security = response.securitycode();
   order.quantity = response.qty();
   order.price = response.price();
   order.product = response.currency();
   order.side = bs::network::Side::fromCeler(response.side());
   order.assetType = bs::network::Asset::fromCelerProductType(response.producttype());
   order.settlementId = response.settlementid();
   order.reqTransaction = response.requestortransaction();
   order.dealerTransaction = response.dealertransaction();

   order.status = mapBtcOrderStatus(response.orderstatus());
   order.pendingStatus = response.info();

   if (!resync && response.updatedtimestamputcinmillis() > celerLoggedInTimestampUtcInMillis_) {

      switch(order.status)
      {
         case Order::Failed:
            emit orderFailed(response.quoteid(), response.info());
            // fall through
         case Order::Filled:
            CleanupXBTOrder(order);
            break;
      }

   }

   emit orderUpdated(order);

   return true;
}

bool QuoteProvider::onFxOrderSnapshot(const std::string& data, bool resync) const
{
   FxOrderSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onFxOrderSnapshot] Failed to parse FxOrderSnapshotDownstreamEvent");
      return false;
   }
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::FxOrderSnapshot] {}", response.DebugString());
   }

   if (!resync && (response.orderstatus() == FILLED)) {
      if (response.updatedtimestamputcinmillis() > celerLoggedInTimestampUtcInMillis_) {
         emit quoteOrderFilled(response.quoteid());
      }
   }

   Order order;
   order.exchOrderId = QString::number(response.orderid());
   order.clOrderId = response.externalclorderid();
   order.dateTime = QDateTime::fromMSecsSinceEpoch(response.createdtimestamputcinmillis());
   order.security = response.securitycode();
   order.quantity = response.qty();
   order.leavesQty = response.leavesqty();
   order.price = response.price();
   order.avgPx = response.avgpx();
   order.product = response.currency();
   order.side = bs::network::Side::fromCeler(response.side());
   order.assetType = bs::network::Asset::SpotFX;

   order.status = mapFxOrderStatus(response.orderstatus());

   if (!resync && (order.status == Order::Failed)) {
      if (response.updatedtimestamputcinmillis() > celerLoggedInTimestampUtcInMillis_) {
         emit orderFailed(response.quoteid(), response.info());
      }
   }

   emit orderUpdated(order);
   return true;
}

bool QuoteProvider::onQuoteCancelled(const std::string& data)
{
   QuoteCancelDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onQuoteCancelled] Failed to parse QuoteCancelDownstreamEvent");
      return false;
   }

   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteCancelled] {}", response.DebugString());
   }

   cleanQuoteRequestCcy(response.quoterequestid());

   emit quoteCancelled(QString::fromStdString(response.quoterequestid())
      , response.quotecanceltype() == com::celertech::marketmerchant::api::enums::quotecanceltype::CANCEL_ALL_QUOTES
      /*&& (response.quotecancelreason() == "QUOTE_CANCEL_BY_USER")*/);

   RemoveQuotePrice(response.quoterequestid());
   return true;
}

bool QuoteProvider::onSignTxNotif(const std::string& data)
{
   bitcoin::SignTransactionNotification response;

   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onSignTxNotif] Failed to parse SignTransactionNotification");
      return false;
   }
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onSignTxNotif] {}", response.DebugString());
   }

   emit signTxRequested(QString::fromStdString(response.orderid()), QString::fromStdString(response.quoterequestid()));
   return true;
}


void QuoteProvider::SubmitQuoteNotif(const bs::network::QuoteNotification &qn)
{
   if (!assetManager_->HaveAssignedAccount()) {
      logger_->error("[QuoteProvider::SubmitQuoteNotif] account name not set");
   }

   auto sequence = std::make_shared<CelerSubmitQuoteNotifSequence>(assetManager_->GetAssignedAccount(), qn, logger_);
   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::SubmitQuoteNotif] failed to execute CelerSubmitQuoteNotifSequence");
   } else {
      logger_->debug("[QuoteProvider::SubmitQuoteNotif] QuoteNotification on {} submitted", qn.quoteRequestId);
      // bidPx is always same as offerPx
      // see QuoteNotification::QuoteNotification()
      SaveQuotePrice(qn.quoteRequestId, qn.bidPx);

      if (qn.assetType == bs::network::Asset::SpotXBT) {
         saveSubmittedXBTQuoteNotification(qn);
      }
   }
}

void QuoteProvider::CancelQuoteNotif(const QString &reqId, const QString &reqSessToken)
{
   auto sequence = std::make_shared<CelerCancelQuoteNotifSequence>(reqId, reqSessToken, logger_);

   if (!celerClient_->ExecuteSequence(sequence)) {
      logger_->error("[QuoteProvider::CancelQuoteNotif] failed to execute CelerCancelQuoteNotifSequence");
   } else {
      logger_->debug("[QuoteProvider::CancelQuoteNotif] CancelQuoteNotification on {} submitted", reqId.toStdString());

      RemoveQuotePrice(reqId.toStdString());
   }
}

bool QuoteProvider::onQuoteReqNotification(const std::string& data)
{
   QuoteRequestNotification response;

   if (!response.ParseFromString(data)) {
         logger_->error("[QuoteProvider::onQuoteReqNotification] Failed to parse QuoteRequestNotification");
         return false;
   }

   if (response.quoterequestnotificationgroup_size() < 1) {
      logger_->error("[QuoteProvider::onQuoteReqNotification] missing at least 1 QRN group");
      return false;
   }  // For SpotFX and SpotXBT there should be only 1 group

   const QuoteRequestNotificationGroup& respgrp = response.quoterequestnotificationgroup(0);

   if (respgrp.quoterequestnotificationleggroup_size() != 1) {
      logger_->error("[QuoteProvider::onQuoteReqNotification] wrong leg group size: {}\n{}"
         , respgrp.quoterequestnotificationleggroup_size()
         , response.DebugString());
      return false;
   }

   const auto& legGroup = respgrp.quoterequestnotificationleggroup(0);

   QuoteReqNotification qrn;
   qrn.quoteRequestId = response.quoterequestid();
   qrn.security = respgrp.securitycode();
   qrn.sessionToken = response.requestorsessiontoken();
   qrn.quantity = legGroup.qty();
   qrn.product = respgrp.currency();
   qrn.party = respgrp.partyid();
   qrn.reason = response.reason();
   qrn.account = response.account();
   qrn.expirationTime = QDateTime::fromMSecsSinceEpoch(response.expiretimeinutcinmillis());
   qrn.celerTimestamp = response.timestampinutcinmillis();
   qrn.timeSkewMs = QDateTime::fromMSecsSinceEpoch(response.timestampinutcinmillis()).msecsTo(QDateTime::currentDateTime());

   qrn.side = bs::network::Side::fromCeler(legGroup.side());
   qrn.assetType = bs::network::Asset::fromCelerProductType(respgrp.producttype());

   switch (response.quotenotificationtype()) {
      case QUOTE_WITHDRAWN:
         qrn.status = QuoteReqNotification::Withdrawn;
         break;
      case PENDING_ACKNOWLEDGE:
         qrn.status = QuoteReqNotification::PendingAck;
         break;
      default:
         qrn.status = QuoteReqNotification::StatusUndefined;
         break;
   }

   if (response.has_settlementid() && !response.settlementid().empty()) {
      qrn.settlementId = response.settlementid();
   }

   if (qrn.assetType == bs::network::Asset::SpotXBT) {
      qrn.requestorAuthPublicKey = response.requestorauthenticationaddress();
   }
   else {
      qrn.requestorAuthPublicKey = respgrp.requestorcointransactioninput();
      qrn.requestorRecvAddress = response.requestorreceiptaddress();
   }

   saveQuoteRequestCcy(qrn.quoteRequestId, qrn.product);

   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteReqNotif] {}", response.DebugString());
   }
   emit quoteReqNotifReceived(qrn);

   return true;
}

bool QuoteProvider::onQuoteNotifCancelled(const std::string& data)
{
   QuoteCancelDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[QuoteProvider::onQuoteNotifCancelled] Failed to parse QuoteCancelDownstreamEvent");
      return false;
   }

   emit quoteNotifCancelled(QString::fromStdString(response.quoterequestid()));
   if (debugTraffic_) {
      logger_->debug("[QuoteProvider::onQuoteNotifCancelled] {}", response.DebugString());
   }
   return true;
}

SecureBinaryData QuoteProvider::getDealerPayin(const std::string& settlementId) const
{
   return dealerPayins_.get(settlementId);
}

void QuoteProvider::saveDealerPayin(const std::string& settlementId, const SecureBinaryData& dealerPayin)
{
   dealerPayins_.save(settlementId, dealerPayin);
}

bs::network::QuoteNotification QuoteProvider::getSubmittedXBTQuoteNotification(const std::string& quoteRequestId)
{
   {
      FastLock locker(submittedNotificationsLock_);

      auto it = submittedNotifications_.find(quoteRequestId);
      if (it != submittedNotifications_.end()) {
         return it->second;
      } else {
         logger_->debug("[QuoteProvider::getSubmittedXBTQuoteNotification] Could not find quote notification for {}", quoteRequestId);
         return bs::network::QuoteNotification{};
      }
   }
}

void QuoteProvider::saveSubmittedXBTQuoteNotification(const bs::network::QuoteNotification& qn)
{
   size_t count = 0;
   bool inserted = false;

   {
      FastLock locker(submittedNotificationsLock_);

      auto it = submittedNotifications_.find(qn.settlementId);
      if (it == submittedNotifications_.end()) {
         submittedNotifications_.emplace(qn.settlementId, qn);
         inserted = true;
      } else {
         it->second = qn;
      }

      count = submittedNotifications_.size();
   }

   // DEBUG
   if (inserted) {
      logger_->debug("[QuoteProvider::saveSubmittedXBTQuoteNotification] save submitted quote notification for {}. Current count {}"
         , qn.settlementId, count);
   } else {
      logger_->debug("[QuoteProvider::saveSubmittedXBTQuoteNotification] quote notification replaced for {}. Current count {}"
         , qn.settlementId, count);
   }
}

void QuoteProvider::eraseSubmittedXBTQuoteNotification(const std::string& settlementId)
{
   size_t count = 0;
   bool erased = false;

   {
      FastLock locker(submittedNotificationsLock_);

      auto it = submittedNotifications_.find(settlementId);
      if (it != submittedNotifications_.end()) {
         submittedNotifications_.erase(it);
         erased = true;
      }

      count = submittedNotifications_.size();
   }

   if (erased) {
      logger_->debug("[QuoteProvider::eraseSubmittedXBTQuoteNotification] erased quote notification for {}. Current count {}"
         , settlementId, count);
   } else {
      logger_->error("[QuoteProvider::eraseSubmittedXBTQuoteNotification] no quote notification for {}. Current count {}"
         , settlementId, count);
   }
}

void QuoteProvider::CleanupXBTOrder(const bs::network::Order& order)
{
   logger_->debug("[QuoteProvider::CleanupXBTOrder] complete quote: {}", order.quoteId);

   dealerPayins_.erase(order.settlementId);
   eraseSubmittedXBTQuoteNotification(order.settlementId);
}

void QuoteProvider::saveQuoteReqId(const std::string &quoteReqId, const std::string &quoteId)
{
   quoteIdMap_[quoteId] = quoteReqId;
   quoteIds_[quoteReqId].push_back(quoteId);
}

std::string QuoteProvider::getQuoteReqId(const std::string &quoteId) const
{
   const auto &itQuoteId = quoteIdMap_.find(quoteId);
   return (itQuoteId == quoteIdMap_.end()) ? std::string{} : itQuoteId->second;
}

void QuoteProvider::delQuoteReqId(const std::string &quoteReqId)
{
   const auto &itQuoteId = quoteIds_.find(quoteReqId);
   if (itQuoteId != quoteIds_.end()) {
      for (const auto &id : itQuoteId->second) {
         quoteIdMap_.erase(id);
      }
      quoteIds_.erase(itQuoteId);
   }
}

void QuoteProvider::saveQuoteRequestCcy(const std::string& id, const std::string& ccy)
{
   FastLock locker(quoteCcysLock_);

   quoteCcys_.emplace(id, ccy);
}

void QuoteProvider::cleanQuoteRequestCcy(const std::string& id)
{
   FastLock locker(quoteCcysLock_);

   auto it = quoteCcys_.find(id);
   if (it != quoteCcys_.end()) {
      quoteCcys_.erase(it);
   }
}

std::string QuoteProvider::getQuoteRequestCcy(const std::string& id) const
{
   std::string ccy;
   {
      FastLock locker(quoteCcysLock_);

      auto it = quoteCcys_.find(id);
      if (it != quoteCcys_.end()) {
         ccy = it->second;
      }
   }

   return ccy;
}

void QuoteProvider::SaveQuotePrice(const std::string& rfqId, double price)
{
   FastLock locker{quotedPricesLock_};
   auto it = quotedPrices_.find(rfqId);
   if (it == quotedPrices_.end()) {
      quotedPrices_.emplace_hint(it, rfqId, price);
   } else {
      it->second = price;
   }
}

bool QuoteProvider::IsOwnPrice(const std::string& rfqId, double receivedPrice) const
{
   FastLock locker{quotedPricesLock_};
   auto it = quotedPrices_.find(rfqId);
   if (it != quotedPrices_.end()) {
      return it->second == receivedPrice;
   }

   return false;
}

void QuoteProvider::RemoveQuotePrice(const std::string& rfqId)
{
   FastLock locker{quotedPricesLock_};
   quotedPrices_.erase(rfqId);
}

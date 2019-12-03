/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_QUOTE_PROVIDER_H__
#define __CELER_QUOTE_PROVIDER_H__

#include <QObject>
#include <unordered_map>
#include <memory>
#include <string>
#include "CommonTypes.h"


namespace spdlog {
   class logger;
}

namespace bs {
   class SettlementAddressEntry;
   namespace network {
      struct QuoteNotification;
   }
}

class AssetManager;
class BaseCelerClient;

class QuoteProvider : public QObject
{
Q_OBJECT

public:
   static bool isRepliableStatus(const bs::network::QuoteReqNotification::Status status);

public:
   QuoteProvider(const std::shared_ptr<AssetManager> &assetManager
      , const std::shared_ptr<spdlog::logger>& logger
      , bool debugTraffic = true);
   ~QuoteProvider() noexcept;

   QuoteProvider(const QuoteProvider&) = delete;
   QuoteProvider& operator = (const QuoteProvider&) = delete;

   QuoteProvider(QuoteProvider&&) = delete;
   QuoteProvider& operator = (QuoteProvider&&) = delete;

   void ConnectToCelerClient(const std::shared_ptr<BaseCelerClient>& celerClient);

   bs::network::QuoteNotification getSubmittedXBTQuoteNotification(const std::string& settlementId);

   std::string getQuoteReqId(const std::string &quoteId) const;
   void delQuoteReqId(const std::string &quoteReqId);

public slots:
   void SubmitRFQ(const bs::network::RFQ& rfq);
   void AcceptQuote(const QString &reqId, const bs::network::Quote& quote, const std::string &payoutTx);
   void AcceptQuoteFX(const QString &reqId, const bs::network::Quote& quote);
   void CancelQuote(const QString &reqId);
   void SignTxRequest(const QString &orderId, const std::string &txData);

   void SubmitQuoteNotif(const bs::network::QuoteNotification& qn);
   void CancelQuoteNotif(const QString &reqId, const QString& reqSessToken);

signals:
   void quoteReceived(const bs::network::Quote& quote) const;
   void quoteRejected(const QString &reqId, const QString &reason) const;
   void quoteCancelled(const QString &reqId, bool userCancelled) const;
   void quoteOrderFilled(const std::string& quoteId) const;
   void orderUpdated(const bs::network::Order& order) const;
   void orderFailed(const std::string& quoteId, const std::string& reason) const;
   void orderRejected(const QString &id, const QString &reason) const;
   void signTxRequested(QString orderId, QString reqId, QDateTime timestamp) const;
   void bestQuotePrice(const QString reqId, double price, bool own) const;

   void quoteReqNotifReceived(const bs::network::QuoteReqNotification& qrn);
   void quoteNotifCancelled(const QString &reqId);

private slots:
   void onConnectedToCeler();

private:
   bool onBitcoinOrderSnapshot(const std::string& data);
   bool onFxOrderSnapshot(const std::string& data);

   bool onQuoteResponse(const std::string& data);
   bool onQuoteReject(const std::string& data);
   bool onOrderReject(const std::string& data);
   bool onQuoteCancelled(const std::string& data);
   bool onSignTxNotif(const std::string &data);
   bool onQuoteAck(const std::string& data);

   bool onQuoteReqNotification(const std::string& data);
   bool onQuoteNotifCancelled(const std::string& data);

   void saveQuoteReqId(const std::string &quoteReqId, const std::string &quoteId);

   void saveSubmittedXBTQuoteNotification(const bs::network::QuoteNotification& qn);
   void eraseSubmittedXBTQuoteNotification(const std::string& settlementId);

   void CleanupXBTOrder(const bs::network::Order& order);

   void saveQuoteRequestCcy(const std::string& id, const std::string& product);
   void cleanQuoteRequestCcy(const std::string& id);

   std::string getQuoteRequestCcy(const std::string& id) const;

   void SaveQuotePrice(const std::string& rfqId, double price);
   bool IsOwnPrice(const std::string& rfqId, double receivedPrice) const;
   void RemoveQuotePrice(const std::string& rfqId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<AssetManager>    assetManager_;
   std::shared_ptr<BaseCelerClient>     celerClient_;
   std::unordered_map<std::string, bs::network::RFQ>   submittedRFQs_;

   std::unordered_map<std::string, std::string> quoteIdMap_;
   std::unordered_map<std::string, std::vector<std::string>>   quoteIds_;

   mutable std::atomic_flag               quotedPricesLock_ = ATOMIC_FLAG_INIT;
   std::unordered_map<std::string,double> quotedPrices_;

   // key - quoteRequestId
   using quoteNotificationsCollection = std::unordered_map<std::string, bs::network::QuoteNotification>;
   quoteNotificationsCollection  submittedNotifications_;
   mutable std::atomic_flag      submittedNotificationsLock_ = ATOMIC_FLAG_INIT;

   // key quote request id
   std::unordered_map<std::string, std::string> quoteCcys_;
   mutable std::atomic_flag      quoteCcysLock_ = ATOMIC_FLAG_INIT;

   int64_t celerLoggedInTimestampUtcInMillis_;

   bool debugTraffic_;
};

#endif // __CELER_QUOTE_PROVIDER_H__

#ifndef QUOTEREQUESTSMODEL_H
#define QUOTEREQUESTSMODEL_H

#include <QStandardItemModel>
#include <QTimer>
#include <memory>
#include <unordered_map>
#include <functional>
#include "CommonTypes.h"


namespace bs {
   namespace network {
      struct QuoteNotification;
   }
   class StatsCollector;
   class SecurityStatsCollector;
   class SettlementContainer;
}
class AssetManager;

class QuoteRequestsModel : public QStandardItemModel
{
Q_OBJECT

public:
   struct Header {
      enum Index {
         first,
         SecurityID = first,
         Product,
         Side,
         Quantity,
         Party,
         Status,
         QuotedPx,
         IndicPx,
         BestPx,
         Empty,
         last
      };
      static QString toString(Index);
      static QStringList labels();
   };
   struct Role {
      enum Index {
         ReqId = Qt::UserRole,
         Side,
         ShowProgress,
         Timeout,
         TimeLeft,
         BidPrice,
         OfferPrice,
         Grade,
         AssetType,
         QuotedPrice,
         BestQPrice,
         Product,
         AllowFiltering
      };
   };

public:
   QuoteRequestsModel(const std::shared_ptr<bs::SecurityStatsCollector> &
      , QObject* parent);
   ~QuoteRequestsModel();
   QuoteRequestsModel(const QuoteRequestsModel&) = delete;
   QuoteRequestsModel& operator=(const QuoteRequestsModel&) = delete;
   QuoteRequestsModel(QuoteRequestsModel&&) = delete;
   QuoteRequestsModel& operator=(QuoteRequestsModel&&) = delete;

   void SetAssetManager(const std::shared_ptr<AssetManager>& assetManager);
   const bs::network::QuoteReqNotification &getQuoteReqNotification(const std::string &id) const;
   double getPrice(const std::string &security, Role::Index) const;

   void addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &);

signals:
   void quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn);

public:
   void onQuoteReqNotifReplied(const bs::network::QuoteNotification &qn);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqCancelled(const QString &reqId, bool byUser);
   void onQuoteRejected(const QString &reqId, const QString &reason);
   void onSecurityMDUpdated(const QString &security, const bs::network::MDFields &);
   void onQuoteReqNotifReceived(const bs::network::QuoteReqNotification &qrn);
   void onBestQuotePrice(const QString reqId, double price, bool own);

private slots:
   void ticker();
   void onSettlementExpired();
   void onSettlementCompleted();
   void onSettlementFailed();

private:
   using Prices = std::map<Role::Index, double>;
   using MDPrices = std::unordered_map<std::string, Prices>;

   std::shared_ptr<AssetManager> assetManager_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification>         notifications_;
   std::unordered_map<std::string, std::shared_ptr<bs::SettlementContainer>>  settlContainers_;
   QTimer      timer_;
   MDPrices    mdPrices_;
   const QString groupNameSettlements_ = tr("Settlements");
   std::shared_ptr<bs::SecurityStatsCollector> secStatsCollector_;
   std::unordered_set<std::string>  pendingDeleteIds_;
   unsigned int   settlCompleted_ = 0;
   unsigned int   settlFailed_ = 0;

private:
   using cbItem = std::function<void(QStandardItem *grp, int itemIndex)>;

   void updateRow(QStandardItem *group, const bs::network::QuoteReqNotification &qrn);
   void forSpecificId(const std::string &, const cbItem &);
   void forEachSecurity(const QString &, const cbItem &);
   void setStatus(const std::string &reqId, bs::network::QuoteReqNotification::Status, const QString &details = {});
   void updateSettlementCounters();
   void deleteSettlement(bs::SettlementContainer *);
   static QString quoteReqStatusDesc(bs::network::QuoteReqNotification::Status status);
   static QBrush bgColorForStatus(bs::network::QuoteReqNotification::Status status);
   static QBrush colorForQuotedPrice(double quotedPx, double bestQuotedPx, bool own = false);
};


class QuoteGroupReqItem : public QStandardItem
{
public:
   QuoteGroupReqItem(const std::shared_ptr<bs::StatsCollector> &
      , const QString &text, const QString &key = {});
   QVariant data(int role = Qt::UserRole + 1) const override;

private:
   std::shared_ptr<bs::StatsCollector> statsCollector_;
   QString  key_;
};

class QuoteReqItem : public QuoteGroupReqItem
{
public:
   QuoteReqItem(const std::shared_ptr<bs::StatsCollector> &
      , const QString &text, const QString &key = {});
};


#endif // QUOTEREQUESTSMODEL_H

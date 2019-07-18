#ifndef QUOTEREQUESTSMODEL_H
#define QUOTEREQUESTSMODEL_H

#include <QAbstractItemModel>
#include <QTimer>
#include <QBrush>
#include <QFont>
#include <QPersistentModelIndex>

#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

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
class BaseCelerClient;
class ApplicationSettings;

class QuoteRequestsModel : public QAbstractItemModel
{
   Q_OBJECT

signals:
   void quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn);
   void invalidateFilterModel();
   void deferredUpdate(const QPersistentModelIndex&);

public:
   enum class Column {
      SecurityID = 0,
      Product,
      Side,
      Quantity,
      Party,
      Status,
      QuotedPx,
      IndicPx,
      BestPx,
      Empty
   };

   enum class Role {
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
      AllowFiltering,
      HasHiddenChildren,
      Quoted,
      Type,
      LimitOfRfqs,
      QuotedRfqsCount,
      Visible,
      StatText,
      CountOfRfqs
   };

   enum class DataType {
      RFQ = 0,
      Group,
      Market,
      Unknown
   };

public:
   QuoteRequestsModel(const std::shared_ptr<bs::SecurityStatsCollector> &
                      , std::shared_ptr<BaseCelerClient> celerClient
                      , std::shared_ptr<ApplicationSettings> appSettings
                      , QObject* parent);
   ~QuoteRequestsModel() override;

   QuoteRequestsModel(const QuoteRequestsModel&) = delete;
   QuoteRequestsModel& operator=(const QuoteRequestsModel&) = delete;
   QuoteRequestsModel(QuoteRequestsModel&&) = delete;
   QuoteRequestsModel& operator=(QuoteRequestsModel&&) = delete;

   void SetAssetManager(const std::shared_ptr<AssetManager>& assetManager);
   const bs::network::QuoteReqNotification &getQuoteReqNotification(const std::string &id) const;
   double getPrice(const std::string &security, Role) const;

   void addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &);

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &index) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;   
   QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const override;

public:
   void onQuoteReqNotifReplied(const bs::network::QuoteNotification &qn);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqCancelled(const QString &reqId, bool byUser);
   void onQuoteRejected(const QString &reqId, const QString &reason);
   void onSecurityMDUpdated(const QString &security, const bs::network::MDFields &);
   void onQuoteReqNotifReceived(const bs::network::QuoteReqNotification &qrn);
   void onBestQuotePrice(const QString reqId, double price, bool own);
   void limitRfqs(const QModelIndex &index, int limit);
   QModelIndex findMarketIndex(const QString &name) const;
   void setPriceUpdateInterval(int interval);
   void showQuotedRfqs(bool on  = true);

private slots:
   void ticker();
   void onSettlementExpired();
   void onSettlementCompleted();
   void onSettlementFailed();
   void clearModel();
   void onDeferredUpdate(const QPersistentModelIndex &index);
   void onPriceUpdateTimer();

private:
   using Prices = std::map<Role, double>;
   using MDPrices = std::unordered_map<std::string, Prices>;

   std::shared_ptr<AssetManager> assetManager_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification>         notifications_;
   std::unordered_map<std::string, std::shared_ptr<bs::SettlementContainer>>  settlContainers_;
   QTimer      timer_;
   QTimer      priceUpdateTimer_;
   MDPrices    mdPrices_;
   const QString groupNameSettlements_ = tr("Settlements");
   std::shared_ptr<bs::SecurityStatsCollector> secStatsCollector_;
   std::shared_ptr<BaseCelerClient>     celerClient_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::unordered_set<std::string>  pendingDeleteIds_;
   unsigned int   settlCompleted_ = 0;
   unsigned int   settlFailed_ = 0;
   int priceUpdateInterval_;
   bool showQuoted_;

   struct IndexHelper {
      IndexHelper *parent_;
      void *data_;
      DataType type_;

      IndexHelper()
         : parent_(nullptr)
         , data_(nullptr)
         , type_(DataType::Unknown)
      {}

      IndexHelper(IndexHelper *parent, void *data, DataType type)
         : parent_(parent)
         , data_(data)
         , type_(type)
      {}
   };

   struct Status {
      QString status_;
      bool showProgress_;
      int timeout_;
      int timeleft_;

      Status()
         : showProgress_(false)
         , timeout_(0)
         , timeleft_(0)
      {}

      Status(const QString &status,
         bool showProgress,
         int timeout = 0,
         int timeleft = 0)
         : status_(status)
         , showProgress_(showProgress)
         , timeout_(timeout)
         , timeleft_(timeleft)
      {}
   };

   struct RFQ {
      QString security_;
      QString product_;
      QString sideString_;
      QString party_;
      QString quantityString_;
      QString quotedPriceString_;
      QString indicativePxString_;
      QString bestQuotedPxString_;
      Status status_;
      double indicativePx_;
      double quotedPrice_;
      double bestQuotedPx_;
      bs::network::Side::Type side_;
      bs::network::Asset::Type assetType_;
      std::string reqId_;
      QBrush quotedPriceBrush_;
      QBrush indicativePxBrush_;
      QBrush stateBrush_;
      IndexHelper idx_;
      bool quoted_;
      bool visible_;

      RFQ()
         : idx_(nullptr, this, DataType::RFQ)
         , quoted_(false)
         , visible_(false)
      {}

      RFQ(const QString &security,
         const QString &product,
         const QString &sideString,
         const QString &party,
         const QString &quantityString,
         const QString &quotedPriceString,
         const QString &indicativePxString,
         const QString &bestQuotedPxString,
         const Status &status,
         double indicativePx,
         double quotedPrice,
         double bestQuotedPx,
         bs::network::Side::Type side,
         bs::network::Asset::Type assetType,
         const std::string &reqId)
         : security_(security)
         , product_(product)
         , sideString_(sideString)
         , party_(party)
         , quantityString_(quantityString)
         , quotedPriceString_(quotedPriceString)
         , indicativePxString_(indicativePxString)
         , bestQuotedPxString_(bestQuotedPxString)
         , status_(status)
         , indicativePx_(indicativePx)
         , quotedPrice_(quotedPrice)
         , bestQuotedPx_(bestQuotedPx)
         , side_(side)
         , assetType_(assetType)
         , reqId_(reqId)
         , idx_(nullptr, this, DataType::RFQ)
         , quoted_(false)
         , visible_(false)
      {}
   };

   struct Group {
      QString security_;
      QFont font_;
      std::vector<std::unique_ptr<RFQ>> rfqs_;
      IndexHelper idx_;
      int limit_;
      int quotedRfqsCount_;
      int visibleCount_;

      Group()
         : idx_(nullptr, this, DataType::Group)
         , limit_(5)
         , quotedRfqsCount_(0)
         , visibleCount_(0)
      {}

      explicit Group(const QString &security, int limit, const QFont & font = QFont())
         : security_(security)
         , font_(font)
         , idx_(nullptr, this, DataType::Group)
         , limit_(limit)
         , quotedRfqsCount_(0)
         , visibleCount_(0)
      {}
   };

   struct Market {
      QString security_;
      QFont font_;
      std::vector<std::unique_ptr<Group>> groups_;
      IndexHelper idx_;
      Group settl_;
      int limit_;

      Market()
         : idx_(nullptr, this, DataType::Market)
         , limit_(5)
      {
         settl_.idx_ = idx_;
      }

      explicit Market(const QString &security, int limit, const QFont & font = QFont())
         : security_(security)
         , font_(font)
         , idx_(nullptr, this, DataType::Market)
         , limit_(limit)
      {
         settl_.idx_ = idx_;
      }
   };

   std::vector<std::unique_ptr<Market>> data_;

   struct BestQuotePrice {
      double price_;
      bool own_;
   };

   std::map<QString, BestQuotePrice> bestQuotePrices_;
   std::map<QString, std::pair<bs::network::MDField, bs::network::MDField>> prices_;

private:
   int findGroup(IndexHelper *idx) const;
   Group* findGroup(Market *market, const QString &security) const;
   int findMarket(IndexHelper *idx) const;
   Market* findMarket(const QString &name) const;
   void updatePrices(const QString &security, const bs::network::MDField &pxBid,
      const bs::network::MDField &pxOffer, std::vector<std::pair<QModelIndex, QModelIndex>> *idxs = nullptr);
   void showRfqsFromBack(Group *g);
   void showRfqsFromFront(Group *g);
   void clearVisibleFlag(Group *g);
   void updateBestQuotePrice(const QString &reqId, double price, bool own,
      std::vector<std::pair<QModelIndex, QModelIndex>> *idxs = nullptr);

private:
   using cbItem = std::function<void(Group *g, int itemIndex)>;

   void insertRfq(Group *group, const bs::network::QuoteReqNotification &qrn);
   void forSpecificId(const std::string &, const cbItem &);
   void forEachSecurity(const QString &, const cbItem &);
   void setStatus(const std::string &reqId, bs::network::QuoteReqNotification::Status, const QString &details = {});
   void updateSettlementCounters();
   void deleteSettlement(bs::SettlementContainer *);
   static QString quoteReqStatusDesc(bs::network::QuoteReqNotification::Status status);
   static QBrush bgColorForStatus(bs::network::QuoteReqNotification::Status status);
   static QBrush colorForQuotedPrice(double quotedPx, double bestQuotedPx, bool own = false);
};

#endif // QUOTEREQUESTSMODEL_H

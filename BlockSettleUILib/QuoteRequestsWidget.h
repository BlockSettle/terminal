#ifndef __QUOTE_REQUESTS_WIDGET_H__
#define __QUOTE_REQUESTS_WIDGET_H__

#include "ApplicationSettings.h"
#include "QuoteRequestsModel.h"

#include <QWidget>
#include <QTimer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QApplication>

#include <memory>
#include <unordered_map>
#include <set>

namespace Ui {
    class QuoteRequestsWidget;
};
namespace spdlog {
   class logger;
}
class QuoteProvider;

namespace bs {
   class SettlementContainer;
   namespace network {
      struct QuoteReqNotification;
      struct QuoteNotification;
   }

   class StatsCollector
   {
   public:
      virtual QColor getColorFor(const std::string &key) const = 0;
      virtual unsigned int getGradeFor(const std::string &key) const = 0;
      virtual void saveState() {}
   };

   class SecurityStatsCollector : public QObject, public StatsCollector
   {
      Q_OBJECT
   public:
      SecurityStatsCollector(const std::shared_ptr<ApplicationSettings>, ApplicationSettings::Setting param);

      QColor getColorFor(const std::string &key) const override;
      unsigned int getGradeFor(const std::string &key) const override;

      void saveState() override;

   public slots:
      void onQuoteSubmitted(const network::QuoteNotification &);

   private slots:
      void onSettingChanged(int setting, QVariant val);

   private:
      std::shared_ptr<ApplicationSettings>            appSettings_;
      ApplicationSettings::Setting                    param_;
      std::unordered_map<std::string, unsigned int>   counters_;
      std::vector<QColor>        gradeColors_;
      std::vector<unsigned int>  gradeBoundary_;
      QTimer                     timer_;
      bool                       modified_ = false;

   private:
      void recalculate();
      void resetCounters();
   };

   class SettlementStatsCollector : public StatsCollector
   {
   public:
      SettlementStatsCollector(const std::shared_ptr<bs::SettlementContainer> &container)
         : container_(container) {}
      QColor getColorFor(const std::string &key) const override;
      unsigned int getGradeFor(const std::string &key) const override;

   private:
      std::shared_ptr<bs::SettlementContainer>  container_;
   };

}  // namespace bs


class ProgressDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   explicit ProgressDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

   void paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const override;
};


class AssetManager;
class CelerClient;
class QuoteRequestsModel;
class QuoteReqSortModel;

class QuoteRequestsWidget : public QWidget
{
Q_OBJECT

public:
   QuoteRequestsWidget(QWidget* parent = nullptr);
   ~QuoteRequestsWidget() override = default;

   void init(std::shared_ptr<spdlog::logger> logger, const std::shared_ptr<QuoteProvider> &quoteProvider
      , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
      , const std::shared_ptr<ApplicationSettings> &appSettings);

   void addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &);

signals:
   void Selected(const bs::network::QuoteReqNotification &, double indicBid, double indicAsk);
   void quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn);

public slots:
   void onQuoteReqNotifReplied(const bs::network::QuoteNotification &);
   void onQuoteReqNotifSelected(const QModelIndex& index);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqCancelled(const QString &reqId);
   void onQuoteRejected(const QString &reqId, const QString &reason);
   void onSecurityMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void onBestQuotePrice(const QString reqId, double price, bool own);

private slots:
   void onSettingChanged(int setting, QVariant val);
   void onQuoteRequest(const bs::network::QuoteReqNotification &qrn);
   void onSecuritiesReceived();
   void onRowsChanged();

private:
   Ui::QuoteRequestsWidget* ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   QuoteRequestsModel *    model_;
   QuoteReqSortModel *     sortModel_;
   bool  dropQN_ = false;
};


class QuoteReqSortModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   QuoteReqSortModel(const std::shared_ptr<AssetManager>& assetMgr, QObject *parent)
      : QSortFilterProxyModel(parent), assetManager_(assetMgr) {}
   void SetFilter(const QStringList &visible);

protected:
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
   bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override;

private:
   std::shared_ptr<AssetManager> assetManager_;
   std::set<QString>             visible_;
};

#endif // __QUOTE_REQUESTS_WIDGET_H__

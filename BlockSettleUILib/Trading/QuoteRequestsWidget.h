#ifndef QUOTE_REQUESTS_WIDGET_H
#define QUOTE_REQUESTS_WIDGET_H

#include "ApplicationSettings.h"
#include "QuoteRequestsModel.h"

#include <QWidget>
#include <QTimer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QProgressBar>

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
      virtual ~StatsCollector() noexcept = default;
      virtual QColor getColorFor(const std::string &key) const = 0;
      virtual unsigned int getGradeFor(const std::string &key) const = 0;
      virtual void saveState();
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
   explicit ProgressDelegate(QWidget *parent = nullptr)
      : QStyledItemDelegate(parent)
   {
      pbar_.setStyleSheet(QLatin1String("QProgressBar { border: 1px solid #1c2835; "
         "border-radius: 4px; background-color: rgba(0, 0, 0, 0); }"));
      pbar_.hide();
   }

   void paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const override;

private:
   QProgressBar pbar_;
};


class AssetManager;
class BaseCelerClient;
class QuoteRequestsModel;
class QuoteReqSortModel;
class RFQBlotterTreeView;
class BaseCelerClient;

class QuoteRequestsWidget : public QWidget
{
Q_OBJECT

public:
   QuoteRequestsWidget(QWidget* parent = nullptr);
   ~QuoteRequestsWidget() override;

   void init(std::shared_ptr<spdlog::logger> logger, const std::shared_ptr<QuoteProvider> &quoteProvider
      , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , std::shared_ptr<BaseCelerClient> celerClient);

   void addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &);

   RFQBlotterTreeView* view() const;

signals:
   void Selected(const bs::network::QuoteReqNotification &, double indicBid, double indicAsk);
   void quoteReqNotifStatusChanged(const bs::network::QuoteReqNotification &qrn);

public slots:
   void onQuoteReqNotifReplied(const bs::network::QuoteNotification &);
   void onQuoteReqNotifSelected(const QModelIndex& index);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqCancelled(const QString &reqId, bool byUser);
   void onQuoteRejected(const QString &reqId, const QString &reason);
   void onSecurityMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void onBestQuotePrice(const QString reqId, double price, bool own);

private slots:
   void onSettingChanged(int setting, QVariant val);
   void onQuoteRequest(const bs::network::QuoteReqNotification &qrn);
   void onRowsChanged();
   void onRowsInserted(const QModelIndex &parent, int first, int last);
   void onRowsRemoved(const QModelIndex &parent, int first, int last);   
   void onCollapsed(const QModelIndex &index);
   void onExpanded(const QModelIndex &index);

private:
   void expandIfNeeded(const QModelIndex &index = QModelIndex());
   void saveCollapsedState();

private:
   std::unique_ptr<Ui::QuoteRequestsWidget> ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   QStringList             collapsed_;
   QuoteRequestsModel *    model_;
   QuoteReqSortModel *     sortModel_;
   bool  dropQN_ = false;
};


class QuoteReqSortModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   QuoteReqSortModel(QuoteRequestsModel *model, QObject *parent);

   void showQuoted(bool on = true);

protected:
   bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
   QuoteRequestsModel * model_;
   bool showQuoted_;
};

#endif // QUOTE_REQUESTS_WIDGET_H

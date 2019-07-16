#include "QuoteRequestsWidget.h"
#include "ui_QuoteRequestsWidget.h"
#include <spdlog/logger.h>

#include "AssetManager.h"
#include "CurrencyPair.h"
#include "NotificationCenter.h"
#include "QuoteProvider.h"
#include "SettlementContainer.h"
#include "UiUtils.h"
#include "RFQBlotterTreeView.h"

#include <QStyle>
#include <QStyleOptionProgressBar>
#include <QProgressBar>
#include <QPainter>


namespace bs {

void StatsCollector::saveState()
{
}

} /* namespace bs */


//
// DoNotDrawSelectionDelegate
//

//! This delegate just clears selection bit and paints item as
//! unselected always.
class DoNotDrawSelectionDelegate final : public QStyledItemDelegate
{
public:
   explicit DoNotDrawSelectionDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

   void paint(QPainter *painter, const QStyleOptionViewItem &opt,
              const QModelIndex &index) const override
   {
      QStyleOptionViewItem changedOpt = opt;
      changedOpt.state &= ~(QStyle::State_Selected);

      QStyledItemDelegate::paint(painter, changedOpt, index);
   }
}; // class DoNotDrawSelectionDelegate


QuoteRequestsWidget::QuoteRequestsWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::QuoteRequestsWidget())
   , model_(nullptr)
   , sortModel_(nullptr)
{
   ui_->setupUi(this);
   ui_->treeViewQuoteRequests->setUniformRowHeights(true);

   connect(ui_->treeViewQuoteRequests, &QTreeView::clicked, this, &QuoteRequestsWidget::onQuoteReqNotifSelected);
   connect(ui_->treeViewQuoteRequests, &QTreeView::doubleClicked, this, &QuoteRequestsWidget::onQuoteReqNotifSelected);
}

QuoteRequestsWidget::~QuoteRequestsWidget() = default;

void QuoteRequestsWidget::init(std::shared_ptr<spdlog::logger> logger, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
   , const std::shared_ptr<ApplicationSettings> &appSettings, std::shared_ptr<BaseCelerClient> celerClient)
{
   logger_ = logger;
   assetManager_ = assetManager;
   appSettings_ = appSettings;
   collapsed_ = appSettings_->get<QStringList>(ApplicationSettings::Filter_MD_QN);
   dropQN_ = appSettings->get<bool>(ApplicationSettings::dropQN);

   model_ = new QuoteRequestsModel(statsCollector, celerClient, appSettings_,
      ui_->treeViewQuoteRequests);
   model_->SetAssetManager(assetManager);

   sortModel_ = new QuoteReqSortModel(model_, this);
   sortModel_->setSourceModel(model_);
   sortModel_->showQuoted(appSettings_->get<bool>(ApplicationSettings::ShowQuoted));

   ui_->treeViewQuoteRequests->setModel(sortModel_);
   ui_->treeViewQuoteRequests->setRfqModel(model_);
   ui_->treeViewQuoteRequests->setSortModel(sortModel_);
   ui_->treeViewQuoteRequests->setAppSettings(appSettings_);
   ui_->treeViewQuoteRequests->header()->setSectionResizeMode(
      static_cast<int>(QuoteRequestsModel::Column::SecurityID),
      QHeaderView::ResizeToContents);

   connect(ui_->treeViewQuoteRequests, &QTreeView::collapsed,
           this, &QuoteRequestsWidget::onCollapsed);
   connect(ui_->treeViewQuoteRequests, &QTreeView::expanded,
           this, &QuoteRequestsWidget::onExpanded);
   connect(ui_->treeViewQuoteRequests, &RFQBlotterTreeView::enterKeyPressed,
           this, &QuoteRequestsWidget::onEnterKeyInQuoteRequestsPressed);
   connect(model_, &QuoteRequestsModel::quoteReqNotifStatusChanged, [this](const bs::network::QuoteReqNotification &qrn) {
      emit quoteReqNotifStatusChanged(qrn);
   });
   connect(model_, &QAbstractItemModel::rowsInserted, this, &QuoteRequestsWidget::onRowsInserted);
   connect(model_, &QAbstractItemModel::rowsRemoved, this, &QuoteRequestsWidget::onRowsRemoved);
   connect(sortModel_, &QSortFilterProxyModel::rowsInserted, this, &QuoteRequestsWidget::onRowsChanged);
   connect(sortModel_, &QSortFilterProxyModel::rowsRemoved, this, &QuoteRequestsWidget::onRowsChanged);

   connect(quoteProvider.get(), &QuoteProvider::quoteReqNotifReceived, this, &QuoteRequestsWidget::onQuoteRequest);
   connect(appSettings.get(), &ApplicationSettings::settingChanged, this, &QuoteRequestsWidget::onSettingChanged);

   ui_->treeViewQuoteRequests->setItemDelegateForColumn(
      static_cast<int>(QuoteRequestsModel::Column::Status), new ProgressDelegate(ui_->treeViewQuoteRequests));

   auto *doNotDrawSelectionDelegate = new DoNotDrawSelectionDelegate(ui_->treeViewQuoteRequests);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(
      static_cast<int>(QuoteRequestsModel::Column::QuotedPx),
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(
      static_cast<int>(QuoteRequestsModel::Column::IndicPx),
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(
      static_cast<int>(QuoteRequestsModel::Column::BestPx),
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(
      static_cast<int>(QuoteRequestsModel::Column::Empty),
      doNotDrawSelectionDelegate);

   const auto opt = ui_->treeViewQuoteRequests->viewOptions();
   const int width = opt.fontMetrics.boundingRect(tr("No quote received")).width() + 10;
   ui_->treeViewQuoteRequests->header()->resizeSection(
      static_cast<int>(QuoteRequestsModel::Column::Status),
      width);
}

void QuoteRequestsWidget::onQuoteReqNotifSelected(const QModelIndex& index)
{
   const auto quoteIndex = sortModel_->index(index.row(), 0, index.parent());
   std::string qId = sortModel_->data(quoteIndex,
      static_cast<int>(QuoteRequestsModel::Role::ReqId)).toString().toStdString();
   const bs::network::QuoteReqNotification &qrn = model_->getQuoteReqNotification(qId);

   double bidPx = model_->getPrice(qrn.security, QuoteRequestsModel::Role::BidPrice);
   double offerPx = model_->getPrice(qrn.security, QuoteRequestsModel::Role::OfferPrice);
   const double bestQPx = sortModel_->data(quoteIndex,
      static_cast<int>(QuoteRequestsModel::Role::BestQPrice)).toDouble();
   if (!qFuzzyIsNull(bestQPx)) {
      CurrencyPair cp(qrn.security);
      bool isBuy = (qrn.side == bs::network::Side::Buy) ^ (cp.NumCurrency() == qrn.product);
      const double quotedPx = sortModel_->data(quoteIndex,
         static_cast<int>(QuoteRequestsModel::Role::QuotedPrice)).toDouble();
      auto assetType = assetManager_->GetAssetTypeForSecurity(qrn.security);
      const auto pip = qFuzzyCompare(bestQPx, quotedPx) ? 0.0 : std::pow(10, -UiUtils::GetPricePrecisionForAssetType(assetType));
      if (isBuy) {
         bidPx = bestQPx + pip;
      }
      else {
         offerPx = bestQPx - pip;
      }
   }
   emit Selected(qrn, bidPx, offerPx);
}

void QuoteRequestsWidget::addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &container)
{
   if (model_) {
      model_->addSettlementContainer(container);
   }
}

RFQBlotterTreeView* QuoteRequestsWidget::view() const
{
   return ui_->treeViewQuoteRequests;
}

void QuoteRequestsWidget::onQuoteReqNotifReplied(const bs::network::QuoteNotification &qn)
{
   if (model_) {
      model_->onQuoteReqNotifReplied(qn);
   }
}

void QuoteRequestsWidget::onQuoteNotifCancelled(const QString &reqId)
{
   if (model_) {
      model_->onQuoteNotifCancelled(reqId);
   }
}

void QuoteRequestsWidget::onQuoteReqCancelled(const QString &reqId, bool byUser)
{
   if (model_) {
      model_->onQuoteReqCancelled(reqId, byUser);
   }
}

void QuoteRequestsWidget::onQuoteRejected(const QString &reqId, const QString &reason)
{
   if (model_) {
      model_->onQuoteRejected(reqId, reason);
   }
}

void QuoteRequestsWidget::onBestQuotePrice(const QString reqId, double price, bool own)
{
   if (model_) {
      model_->onBestQuotePrice(reqId, price, own);
   }
}

void QuoteRequestsWidget::onSecurityMDUpdated(bs::network::Asset::Type assetType, const QString &security, bs::network::MDFields mdFields)
{
   Q_UNUSED(assetType);
   if (model_ && !mdFields.empty()) {
      model_->onSecurityMDUpdated(security, mdFields);
   }
}

void QuoteRequestsWidget::onQuoteRequest(const bs::network::QuoteReqNotification &qrn)
{
   if (dropQN_) {
      bool checkResult = true;
      if (qrn.side == bs::network::Side::Buy) {
         checkResult = assetManager_->checkBalance(qrn.product, qrn.quantity);
      }
      else {
         CurrencyPair cp(qrn.security);
         checkResult = assetManager_->checkBalance(cp.ContraCurrency(qrn.product), 0.01);
      }
      if (!checkResult) {
         return;
      }
   }
   if (model_ != nullptr) {
      model_->onQuoteReqNotifReceived(qrn);
   }
}

void QuoteRequestsWidget::onSettingChanged(int setting, QVariant val)
{
   switch (static_cast<ApplicationSettings::Setting>(setting))
   {
      case ApplicationSettings::dropQN:
         dropQN_ = val.toBool();
         break;

      case ApplicationSettings::FxRfqLimit :
         ui_->treeViewQuoteRequests->setLimit(ApplicationSettings::FxRfqLimit, val.toInt());
         break;

      case ApplicationSettings::XbtRfqLimit :
         ui_->treeViewQuoteRequests->setLimit(ApplicationSettings::XbtRfqLimit, val.toInt());
         break;

      case ApplicationSettings::PmRfqLimit :
         ui_->treeViewQuoteRequests->setLimit(ApplicationSettings::PmRfqLimit, val.toInt());
         break;

      case ApplicationSettings::PriceUpdateInterval :
         model_->setPriceUpdateInterval(val.toInt());
         break;

      case ApplicationSettings::ShowQuoted :
         sortModel_->showQuoted(val.toBool());
         break;

      default:
         break;
   }
}

void QuoteRequestsWidget::onRowsChanged()
{
   unsigned int cntChildren = 0;
   for (int row = 0; row < sortModel_->rowCount(); row++) {
      cntChildren += sortModel_->rowCount(sortModel_->index(row, 0));
   }
   NotificationCenter::notify(bs::ui::NotifyType::DealerQuotes, { cntChildren });
}

void QuoteRequestsWidget::onRowsInserted(const QModelIndex &parent, int first, int last)
{
   for (int row = first; row <= last; row++) {
      const auto &index = model_->index(row, 0, parent);
      if (index.data(static_cast<int>(QuoteRequestsModel::Role::ReqId)).isNull()) {
         expandIfNeeded();
      }
      else {
         for (int i = 1; i < sortModel_->columnCount(); ++i) {
            if (i != static_cast<int>(QuoteRequestsModel::Column::Status)) {
               ui_->treeViewQuoteRequests->resizeColumnToContents(i);
            }
         }
      }
   }
}

void QuoteRequestsWidget::onRowsRemoved(const QModelIndex &, int, int)
{
   const auto &indices = ui_->treeViewQuoteRequests->selectionModel()->selectedIndexes();
   if (indices.isEmpty()) {
      emit Selected(bs::network::QuoteReqNotification(), 0, 0);
   }
   else {
      onQuoteReqNotifSelected(indices.first());
   }
}

void QuoteRequestsWidget::onCollapsed(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.append(UiUtils::modelPath(sortModel_->mapToSource(index), model_));
      saveCollapsedState();
   }
}

void QuoteRequestsWidget::onExpanded(const QModelIndex &index)
{
   if (index.isValid()) {
      collapsed_.removeOne(UiUtils::modelPath(sortModel_->mapToSource(index), model_));
      saveCollapsedState();
   }
}

void QuoteRequestsWidget::saveCollapsedState()
{
   appSettings_->set(ApplicationSettings::Filter_MD_QN, collapsed_);
}

void QuoteRequestsWidget::onEnterKeyInQuoteRequestsPressed(const QModelIndex &index)
{
   onQuoteReqNotifSelected(index);
}

void QuoteRequestsWidget::expandIfNeeded(const QModelIndex &index)
{
   if (!collapsed_.contains(UiUtils::modelPath(sortModel_->mapToSource(index), model_)))
      ui_->treeViewQuoteRequests->expand(index);

   for (int i = 0; i < sortModel_->rowCount(index); ++i)
      expandIfNeeded(sortModel_->index(i, 0, index));
}

bs::SecurityStatsCollector::SecurityStatsCollector(const std::shared_ptr<ApplicationSettings> appSettings, ApplicationSettings::Setting param)
   : appSettings_(appSettings), param_(param)
{
   const auto map = appSettings_->get<QVariantMap>(param);
   for (auto it = map.begin(); it != map.end(); ++it) {
      counters_[it.key().toStdString()] = it.value().toUInt();
   }
   connect(appSettings.get(), &ApplicationSettings::settingChanged, this, &bs::SecurityStatsCollector::onSettingChanged);

   gradeColors_ = { QColor(Qt::white), QColor(Qt::lightGray), QColor(Qt::gray), QColor(Qt::darkGray) };
   gradeBoundary_.resize(gradeColors_.size(), 0);

   timer_.setInterval(60 * 1000);  // once in a minute
   connect(&timer_, &QTimer::timeout, this, &bs::SecurityStatsCollector::saveState);
   timer_.start();
}

void bs::SecurityStatsCollector::saveState()
{
   if (!modified_) {
      return;
   }
   QVariantMap map;
   for (const auto counter : counters_) {
      map[QString::fromStdString(counter.first)] = counter.second;
   }
   appSettings_->set(param_, map);
   modified_ = false;
}

void bs::SecurityStatsCollector::onSettingChanged(int setting, QVariant val)
{
   if ((static_cast<ApplicationSettings::Setting>(setting) == param_) && val.toMap().empty()) {
      resetCounters();
   }
}

void bs::SecurityStatsCollector::onQuoteSubmitted(const bs::network::QuoteNotification &qn)
{
   counters_[qn.security]++;
   modified_ = true;
   recalculate();
}

unsigned int bs::SecurityStatsCollector::getGradeFor(const std::string &security) const
{
   const auto itSec = counters_.find(security);
   if (itSec == counters_.end()) {
      return gradeBoundary_.size() - 1;
   }
   for (size_t i = 0; i < gradeBoundary_.size(); i++) {
      if (itSec->second <= gradeBoundary_[i]) {
         return gradeBoundary_.size() - 1 - i;
      }
   }
   return 0;
}

QColor bs::SecurityStatsCollector::getColorFor(const std::string &security) const
{
   return gradeColors_[getGradeFor(security)];
}

void bs::SecurityStatsCollector::resetCounters()
{
   counters_.clear();
   modified_ = true;
   recalculate();
}

void bs::SecurityStatsCollector::recalculate()
{
   std::vector<unsigned int> counts;
   for (const auto &counter : counters_) {
      counts.push_back(counter.second);
   }
   std::sort(counts.begin(), counts.end());

   if (counts.size() < gradeBoundary_.size()) {
      for (unsigned int i = 0; i < counts.size(); i++) {
         gradeBoundary_[gradeBoundary_.size() - counts.size() + i] = counts[i];
      }
   }
   else {
      const unsigned int step = counts.size() / gradeBoundary_.size();
      for (unsigned int i = 0; i < gradeBoundary_.size(); i++) {
         gradeBoundary_[i] = counts[qMin<unsigned int>((i + 1) * step, counts.size() - 1)];
      }
   }
}


QColor bs::SettlementStatsCollector::getColorFor(const std::string &) const
{
   return QColor(Qt::cyan);
}

unsigned int bs::SettlementStatsCollector::getGradeFor(const std::string &) const
{
   return container_->timeLeftMs();
}


QuoteReqSortModel::QuoteReqSortModel(QuoteRequestsModel *model, QObject *parent)
   : QSortFilterProxyModel(parent)
   , model_(model)
   , showQuoted_(true)
{
   connect(model_, &QuoteRequestsModel::invalidateFilterModel,
      this, &QuoteReqSortModel::invalidate);
}

bool QuoteReqSortModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
   const auto index = sourceModel()->index(row, 0, parent);

   if (index.isValid() ) {
      if(index.data(static_cast<int>(QuoteRequestsModel::Role::Type)).toInt() ==
         static_cast<int>(QuoteRequestsModel::DataType::RFQ)) {
            if (parent.data(static_cast<int>(QuoteRequestsModel::Role::LimitOfRfqs)).toInt() > 0) {
               if (index.data(static_cast<int>(QuoteRequestsModel::Role::Visible)).toBool()) {
                  return true;
               } else if (index.data(static_cast<int>(QuoteRequestsModel::Role::Quoted)).toBool() &&
                     showQuoted_) {
                        return true;
               } else {
                  return false;
               }
            } else {
               return true;
            }
      } else {
         return true;
      }
   } else {
      return false;
   }
}

void QuoteReqSortModel::showQuoted(bool on)
{
   if (showQuoted_ != on) {
      showQuoted_ = on;

      model_->showQuotedRfqs(on);

      invalidateFilter();
   }
}

bool QuoteReqSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   const auto leftGrade = sourceModel()->data(left,
      static_cast<int>(QuoteRequestsModel::Role::Grade));
   const auto rightGrade = sourceModel()->data(right,
      static_cast<int>(QuoteRequestsModel::Role::Grade));
   if (leftGrade != rightGrade) {
      return (leftGrade < rightGrade);
   }
   const auto leftTL = sourceModel()->data(left,
      static_cast<int>(QuoteRequestsModel::Role::TimeLeft));
   const auto rightTL = sourceModel()->data(right,
      static_cast<int>(QuoteRequestsModel::Role::TimeLeft));
   return (leftTL < rightTL);
}


void ProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const
{
   if (index.data(static_cast<int>(QuoteRequestsModel::Role::ShowProgress)).toBool()) {
      QStyleOptionProgressBar pOpt;
      pOpt.maximum = index.data(static_cast<int>(QuoteRequestsModel::Role::Timeout)).toInt();
      pOpt.minimum = 0;
      pOpt.progress = index.data(static_cast<int>(QuoteRequestsModel::Role::TimeLeft)).toInt();
      pOpt.rect = opt.rect;

      QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pOpt, painter, &pbar_);
   } else {
      QStyleOptionViewItem changedOpt = opt;
      changedOpt.state &= ~(QStyle::State_Selected);

      QStyledItemDelegate::paint(painter, changedOpt, index);
   }
}

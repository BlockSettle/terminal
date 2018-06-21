#include "QuoteRequestsWidget.h"
#include "ui_QuoteRequestsWidget.h"
#include <spdlog/logger.h>

#include "AssetManager.h"
#include "CurrencyPair.h"
#include "NotificationCenter.h"
#include "QuoteProvider.h"
#include "SettlementContainer.h"
#include "UiUtils.h"


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

void QuoteRequestsWidget::init(std::shared_ptr<spdlog::logger> logger, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<AssetManager>& assetManager, const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
   , const std::shared_ptr<ApplicationSettings> &appSettings)
{
   logger_ = logger;
   assetManager_ = assetManager;
   appSettings_ = appSettings;
   dropQN_ = appSettings->get<bool>(ApplicationSettings::dropQN);

   model_ = new QuoteRequestsModel(statsCollector, ui_->treeViewQuoteRequests);
   model_->SetAssetManager(assetManager);

   sortModel_ = new QuoteReqSortModel(assetManager, this);
   sortModel_->setSourceModel(model_);

   ui_->treeViewQuoteRequests->setModel(sortModel_);

   connect(model_, &QAbstractItemModel::rowsInserted, [this]() {
      ui_->treeViewQuoteRequests->expandAll();
      for (int i = 0; i < sortModel_->columnCount(); i++) {
         ui_->treeViewQuoteRequests->resizeColumnToContents(i);
      }
   });
   connect(model_, &QAbstractItemModel::rowsRemoved, [this] {
      const auto &indices = ui_->treeViewQuoteRequests->selectionModel()->selectedIndexes();
      if (indices.isEmpty()) {
         emit Selected(bs::network::QuoteReqNotification(), 0, 0);
      }
      else {
         onQuoteReqNotifSelected(indices.first());
      }
   });
   connect(model_, &QuoteRequestsModel::quoteReqNotifStatusChanged, [this](const bs::network::QuoteReqNotification &qrn) {
      emit quoteReqNotifStatusChanged(qrn);
   });
   connect(sortModel_, &QSortFilterProxyModel::rowsInserted, this, &QuoteRequestsWidget::onRowsChanged);
   connect(sortModel_, &QSortFilterProxyModel::rowsRemoved, this, &QuoteRequestsWidget::onRowsChanged);

   connect(quoteProvider.get(), &QuoteProvider::quoteReqNotifReceived, this, &QuoteRequestsWidget::onQuoteRequest);
   connect(appSettings.get(), &ApplicationSettings::settingChanged, this, &QuoteRequestsWidget::onSettingChanged);
   connect(assetManager_.get(), &AssetManager::securitiesReceived, this, &QuoteRequestsWidget::onSecuritiesReceived);

   ui_->treeViewQuoteRequests->setItemDelegateForColumn(QuoteRequestsModel::Header::Status, new ProgressDelegate());

   auto *doNotDrawSelectionDelegate = new DoNotDrawSelectionDelegate(ui_->treeViewQuoteRequests);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(QuoteRequestsModel::Header::QuotedPx,
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(QuoteRequestsModel::Header::IndicPx,
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(QuoteRequestsModel::Header::BestPx,
      doNotDrawSelectionDelegate);
   ui_->treeViewQuoteRequests->setItemDelegateForColumn(QuoteRequestsModel::Header::Empty,
      doNotDrawSelectionDelegate);
}

void QuoteRequestsWidget::onQuoteReqNotifSelected(const QModelIndex& index)
{
   const auto quoteIndex = sortModel_->index(index.row(), 0, index.parent());
   std::string qId = sortModel_->data(quoteIndex, QuoteRequestsModel::Role::ReqId).toString().toStdString();
   const bs::network::QuoteReqNotification &qrn = model_->getQuoteReqNotification(qId);

   double bidPx = model_->getPrice(qrn.security, QuoteRequestsModel::Role::BidPrice);
   double offerPx = model_->getPrice(qrn.security, QuoteRequestsModel::Role::OfferPrice);
   const double bestQPx = sortModel_->data(quoteIndex, QuoteRequestsModel::Role::BestQPrice).toDouble();
   if (!qFuzzyIsNull(bestQPx)) {
      CurrencyPair cp(qrn.security);
      bool isBuy = (qrn.side == bs::network::Side::Buy) ^ (cp.NumCurrency() == qrn.product);
      const double quotedPx = sortModel_->data(quoteIndex, QuoteRequestsModel::Role::QuotedPrice).toDouble();
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

void QuoteRequestsWidget::onSecuritiesReceived()
{
   sortModel_->SetFilter(appSettings_->get<QStringList>(ApplicationSettings::Filter_MD_QN));
   ui_->treeViewQuoteRequests->expandAll();
}

void QuoteRequestsWidget::onSettingChanged(int setting, QVariant val)
{
   switch (static_cast<ApplicationSettings::Setting>(setting))
   {
   case ApplicationSettings::Filter_MD_QN:
      sortModel_->SetFilter(val.toStringList());
      ui_->treeViewQuoteRequests->expandAll();
      break;

   case ApplicationSettings::dropQN:
      dropQN_ = val.toBool();
      break;

   default:   break;
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


bool QuoteReqSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   const auto leftGrade = sourceModel()->data(left, QuoteRequestsModel::Role::Grade);
   const auto rightGrade = sourceModel()->data(right, QuoteRequestsModel::Role::Grade);
   if (leftGrade != rightGrade) {
      return (leftGrade < rightGrade);
   }
   const auto leftTL = sourceModel()->data(left, QuoteRequestsModel::Role::TimeLeft);
   const auto rightTL = sourceModel()->data(right, QuoteRequestsModel::Role::TimeLeft);
   return (leftTL < rightTL);
}

bool QuoteReqSortModel::filterAcceptsRow(int sourceRow, const QModelIndex &srcParent) const
{
   if (visible_.empty()) {
      return true;
   }

   const auto index = sourceModel()->index(sourceRow, 0, srcParent);
   if (!sourceModel()->data(index, QuoteRequestsModel::Role::AllowFiltering).toBool()) {
      return true;
   }
   const auto security = sourceModel()->data(index).toString();
   if (visible_.find(security) != visible_.end()) {
      return false;
   }
   return true;
}

void QuoteReqSortModel::SetFilter(const QStringList &visible)
{
   visible_.clear();
   for (const auto &item : visible) {
      visible_.insert(item);
   }

   invalidateFilter();
}

#include <QProgressBar>
#include <QPainter>
void ProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const
{
   if (index.data(QuoteRequestsModel::Role::ShowProgress).toBool()) {
      QProgressBar renderer;

      QString style = QString::fromStdString("QProgressBar { border: 1px solid #1c2835; border-radius: 4px; background-color: rgba(0, 0, 0, 0); }");

      renderer.resize(opt.rect.size());
      renderer.setMinimum(0);
      renderer.setMaximum(index.data(QuoteRequestsModel::Role::Timeout).toInt());
      renderer.setValue(index.data(QuoteRequestsModel::Role::TimeLeft).toInt());
      renderer.setTextVisible(false);

      //QApplication::style()->polish(&renderer);
      renderer.setStyleSheet(style);
      painter->save();
      painter->translate(opt.rect.topLeft());
      renderer.render(painter);
      painter->restore();
   } else {
      QStyledItemDelegate::paint(painter, opt, index);
      return;
   }
}

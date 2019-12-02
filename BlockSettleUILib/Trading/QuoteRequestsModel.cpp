/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QuoteRequestsModel.h"

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "CelerClient.h"
#include "CelerSubmitQuoteNotifSequence.h"
#include "Colors.h"
#include "CommonTypes.h"
#include "CurrencyPair.h"
#include "DealerCCSettlementContainer.h"
#include "QuoteRequestsWidget.h"
#include "SettlementContainer.h"
#include "UiUtils.h"

#include <chrono>


QuoteRequestsModel::QuoteRequestsModel(const std::shared_ptr<bs::SecurityStatsCollector> &statsCollector
 , std::shared_ptr<BaseCelerClient> celerClient, std::shared_ptr<ApplicationSettings> appSettings
 , QObject* parent)
   : QAbstractItemModel(parent)
   , secStatsCollector_(statsCollector)
   , celerClient_(celerClient)
   , appSettings_(appSettings)
{
   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &QuoteRequestsModel::ticker);
   timer_.start();

   connect(&priceUpdateTimer_, &QTimer::timeout, this, &QuoteRequestsModel::onPriceUpdateTimer);

   setPriceUpdateInterval(appSettings_->get<int>(ApplicationSettings::PriceUpdateInterval));

   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed,
      this, &QuoteRequestsModel::clearModel);
   connect(this, &QuoteRequestsModel::deferredUpdate,
      this, &QuoteRequestsModel::onDeferredUpdate, Qt::QueuedConnection);
}

QuoteRequestsModel::~QuoteRequestsModel()
{
   secStatsCollector_->saveState();
}

int QuoteRequestsModel::columnCount(const QModelIndex &) const
{
   return 10;
}

QVariant QuoteRequestsModel::data(const QModelIndex &index, int role) const
{
   IndexHelper *idx = static_cast<IndexHelper*>(index.internalPointer());

   switch (idx->type_) {
      case DataType::RFQ : {
         RFQ *r = static_cast<RFQ*>(idx->data_);

         switch (role) {
            case Qt::DisplayRole : {
               switch(static_cast<Column>(index.column())) {
                  case Column::SecurityID : {
                     return r->security_;
                  }

                  case Column::Product : {
                     return r->product_;
                  }
                  case Column::Side : {
                     return r->sideString_;
                  }

                  case Column::Quantity : {
                     return r->quantityString_;
                  }

                  case Column::Party : {
                     return r->party_;
                  }

                  case Column::Status : {
                     return r->status_.status_;
                  }

                  case Column::QuotedPx : {
                     return r->quotedPriceString_;
                  }

                  case Column::IndicPx : {
                     return r->indicativePxString_;
                  }

                  case Column::BestPx : {
                     return r->bestQuotedPxString_;
                  }

                  default:
                     return QVariant();
               }
            }

            case static_cast<int>(Role::Type) : {
               return static_cast<int>(DataType::RFQ);
            }

            case static_cast<int>(Role::LimitOfRfqs) : {
               return static_cast<Group*>(r->idx_.parent_->data_)->limit_;
            }

            case static_cast<int>(Role::QuotedRfqsCount) : {
               return -1;
            }

            case static_cast<int>(Role::Quoted) : {
               return r->quoted_;
            }

            case static_cast<int>(Role::ShowProgress) : {
               return r->status_.showProgress_;
            }

            case static_cast<int>(Role::Timeout) : {
               return r->status_.timeout_;
            }

            case static_cast<int>(Role::TimeLeft) : {
               return r->status_.timeleft_;
            }

            case static_cast<int>(Role::BestQPrice) : {
               return r->bestQuotedPx_;
            }

            case static_cast<int>(Role::QuotedPrice) : {
               return r->quotedPrice_;
            }

            case static_cast<int>(Role::AllowFiltering) : {
               if (index.column() == 0) {
                  return true;
               } else {
                  return false;
               }
            }

            case static_cast<int>(Role::ReqId) : {
               return QString::fromStdString(r->reqId_);
            }

            case Qt::BackgroundRole : {
               switch (static_cast<Column>(index.column())) {
                  case Column::QuotedPx :
                     return r->quotedPriceBrush_;

                  case Column::IndicPx :
                     return r->indicativePxBrush_;

                  case Column::Status :
                     return r->stateBrush_;

                  default :
                     return QVariant();
               }
            }

            case Qt::TextColorRole: {
               if (secStatsCollector_ && index.column() < static_cast<int>(Column::Status)) {
                  return secStatsCollector_->getColorFor(r->security_.toStdString());
               }
               else {
                  return QVariant();
               }
            }

            case static_cast<int>(Role::Visible) : {
               return r->visible_;
            }

            case static_cast<int>(Role::SortOrder) : {
               return r->status_.timeleft_;
            }

            default:
               return QVariant();
         }
      }

      case DataType::Group : {
         Group *g = static_cast<Group*>(idx->data_);

         switch (role) {
            case Qt::FontRole : {
               if (index.column() == static_cast<int>(Column::SecurityID)) {
                  return g->font_;
               } else {
                  return QVariant();
               }
            }

            case static_cast<int>(Role::Type) : {
               return static_cast<int>(DataType::Group);
            }

            case static_cast<int>(Role::HasHiddenChildren) : {
               return (static_cast<std::size_t>(g->visibleCount_ + g->quotedRfqsCount_) <
                  g->rfqs_.size());
            }

            case static_cast<int>(Role::LimitOfRfqs) : {
               return g->limit_;
            }

            case static_cast<int>(Role::QuotedRfqsCount) : {
               return g->quotedRfqsCount_;
            }

            case Qt::DisplayRole : {
               switch (index.column()) {
                  case static_cast<int>(Column::SecurityID) :
                     return g->security_;

                  default :
                     return QVariant();
               }
            }

            case static_cast<int>(Role::StatText) : {
               return (g->limit_ > 0 ? tr("%1 of %2")
                     .arg(QString::number(g->visibleCount_ +
                        (showQuoted_ ? g->quotedRfqsCount_ : 0)))
                     .arg(QString::number(g->rfqs_.size())) :
                  tr("%1 RFQ").arg(QString::number(g->rfqs_.size())));
            }

            case static_cast<int>(Role::CountOfRfqs) : {
               return static_cast<int>(g->rfqs_.size());
            }

            case Qt::TextColorRole : {
               switch (index.column()) {
                  case static_cast<int>(Column::SecurityID) : {
                     if (secStatsCollector_) {
                        return secStatsCollector_->getColorFor(g->security_.toStdString());
                     } else {
                        return QVariant();
                     }
                  }

                  default :
                     return QVariant();
               }
            }

            case static_cast<int>(Role::Grade) : {
               if (secStatsCollector_) {
                  return secStatsCollector_->getGradeFor(g->security_.toStdString());
               } else {
                  return QVariant();
               }
            }

            case static_cast<int>(Role::AllowFiltering) : {
               return true;
            }

            case static_cast<int>(Role::SortOrder) : {
               return g->security_;
            }

            default :
               return QVariant();
         }
      }

      case DataType::Market : {
         Market *m = static_cast<Market*>(idx->data_);

         switch (role) {
            case static_cast<int>(Role::Grade) : {
               return 1;
            }

            case static_cast<int>(Role::Type) : {
               return static_cast<int>(DataType::Market);
            }

            case static_cast<int>(Role::LimitOfRfqs) : {
               return m->limit_;
            }

            case static_cast<int>(Role::QuotedRfqsCount) : {
               return -1;
            }

            case Qt::DisplayRole : {
               switch (static_cast<Column>(index.column())) {
                  case Column::SecurityID : {
                     return m->security_;
                  }

                  default :
                     return QVariant();
               }
            }

            case Qt::TextColorRole : {
               if (index.column() == static_cast<int>(Column::Product)) {
                  return c_greenColor;
               } else if (index.column() == static_cast<int>(Column::Side)) {
                  return c_redColor;
               } else {
                  return QVariant();
               }
            }

            case Qt::TextAlignmentRole : {
               if (index.column() == static_cast<int>(Column::Product)
                   || index.column() == static_cast<int>(Column::Side)) {
                     return Qt::AlignRight;
               } else {
                  return QVariant();
               }
            }

            case static_cast<int>(Role::SortOrder) : {
               if (m->security_ == QLatin1String(bs::network::Asset::toString(bs::network::Asset::Type::PrivateMarket))) {
                  return 3;
               }
               else if (m->security_ == QLatin1String(bs::network::Asset::toString(bs::network::Asset::Type::SpotXBT))) {
                  return 2;
               }
               else if (m->security_ == QLatin1String(bs::network::Asset::toString(bs::network::Asset::Type::SpotFX))) {
                  return 1;
               }
               else if (m->security_ == groupNameSettlements_) {
                  return 0;
               }
            }

            default :
               return QVariant();
         }
      }

      default :
         return QVariant();
   }
}

QModelIndex QuoteRequestsModel::index(int r, int column, const QModelIndex &parent) const
{
   std::size_t row = static_cast<std::size_t>(r);

   if (parent.isValid()) {
      IndexHelper *idx = static_cast<IndexHelper*>(parent.internalPointer());

      switch (idx->type_) {
         case DataType::Market : {
            Market *m = static_cast<Market*>(idx->data_);

            if (row < m->groups_.size()) {
               return createIndex(r, column, &m->groups_.at(row)->idx_);
            } else if (row < m->groups_.size() + m->settl_.rfqs_.size()){
               return createIndex(r, column, &m->settl_.rfqs_.at(row - m->groups_.size())->idx_);
            } else {
               return QModelIndex();
            }
         }

         case DataType::Group : {
            Group *g = static_cast<Group*>(idx->data_);

            if (row < g->rfqs_.size()) {
               return createIndex(r, column, &g->rfqs_.at(row)->idx_);
            } else {
               return QModelIndex();
            }
         }

         default :
            return QModelIndex();
      }
   } else if (row < data_.size()) {
      return createIndex(r, column, &data_.at(row)->idx_);
   } else {
      return QModelIndex();
   }
}

int QuoteRequestsModel::findGroup(IndexHelper *idx) const
{
   if (idx->parent_) {
      auto *market = static_cast<Market*>(idx->parent_->data_);

      auto it = std::find_if(market->groups_.cbegin(), market->groups_.cend(),
         [&idx](const std::unique_ptr<Group> &g) { return (&g->idx_ == idx); });

      if (it != market->groups_.cend()) {
         return static_cast<int>(std::distance(market->groups_.cbegin(), it));
      } else {
         return -1;
      }
   } else if (idx->type_ == DataType::Market) {
      return findMarket(idx);
   } else {
      return -1;
   }
}

QuoteRequestsModel::Group* QuoteRequestsModel::findGroup(Market *market, const QString &security) const
{
   auto it = std::find_if(market->groups_.cbegin(), market->groups_.cend(),
      [&] (const std::unique_ptr<Group> &g) { return (g->security_ == security); } );

   if (it != market->groups_.cend()) {
      return it->get();
   } else {
      return nullptr;
   }
}

int QuoteRequestsModel::findMarket(IndexHelper *idx) const
{
   auto it = std::find_if(data_.cbegin(), data_.cend(),
      [&idx](const std::unique_ptr<Market> &m) { return (&m->idx_ == idx); });

   if (it != data_.cend()) {
      return static_cast<int>(std::distance(data_.cbegin(), it));
   } else {
      return -1;
   }
}

QuoteRequestsModel::Market* QuoteRequestsModel::findMarket(const QString &name) const
{
   for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[i]->security_ == name) {
         return data_[i].get();
      }
   }

   return nullptr;
}

QModelIndex QuoteRequestsModel::parent(const QModelIndex &index) const
{
   if (index.isValid()) {
      IndexHelper *idx = static_cast<IndexHelper*>(index.internalPointer());

      if (idx->parent_) {
         switch (idx->parent_->type_) {
            case DataType::Group : {
               return createIndex(findGroup(idx->parent_), 0, idx->parent_);
            }

            case DataType::Market : {
               return createIndex(findMarket(idx->parent_), 0, idx->parent_);
            }

            default :
               return QModelIndex();
         }
      } else {
         return QModelIndex();
      }
   } else {
      return QModelIndex();
   }
}

int QuoteRequestsModel::rowCount(const QModelIndex &parent) const
{
   if (parent.isValid()) {
      IndexHelper *idx = static_cast<IndexHelper*>(parent.internalPointer());

      switch (idx->type_) {
         case DataType::Group : {
            return static_cast<int>(static_cast<Group*>(idx->data_)->rfqs_.size());
         }

         case DataType::Market : {
            auto *market = static_cast<Market*>(idx->data_);
            return static_cast<int>(market->groups_.size() + market->settl_.rfqs_.size());
         }

         default :
            return 0;
      }
   } else {
      return static_cast<int>(data_.size());
   }
}

QVariant QuoteRequestsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role != Qt::DisplayRole) {
      return QVariant();
   }

   switch (static_cast<Column>(section)) {
      case Column::SecurityID:  return tr("SecurityID");
      case Column::Product:     return tr("Product");
      case Column::Side:        return tr("Side");
      case Column::Quantity:    return tr("Quantity");
      case Column::Party:       return tr("Party");
      case Column::Status:      return tr("Status");
      case Column::QuotedPx:    return tr("Quoted Price");
      case Column::IndicPx:     return tr("Indicative Px");
      case Column::BestPx:      return tr("Best Quoted Px");
      default:                  return QString();
   }
}

void QuoteRequestsModel::limitRfqs(const QModelIndex &index, int limit)
{
   if (!index.parent().isValid() && index.row() < static_cast<int>(data_.size())) {
      IndexHelper *idx = static_cast<IndexHelper*>(index.internalPointer());

      auto *m = static_cast<Market*>(idx->data_);

      m->limit_ = limit;

      for (auto it = m->groups_.begin(), last = m->groups_.end(); it != last; ++it) {
         (*it)->limit_ = limit;
         clearVisibleFlag(it->get());
         showRfqsFromFront(it->get());
      }

      emit invalidateFilterModel();
   }
}

QModelIndex QuoteRequestsModel::findMarketIndex(const QString &name) const
{
   auto *m = findMarket(name);

   if (m) {
      return createIndex(findMarket(&m->idx_), 0, &m->idx_);
   } else {
      return QModelIndex();
   }
}

void QuoteRequestsModel::setPriceUpdateInterval(int interval)
{
   priceUpdateInterval_ = interval;
   priceUpdateTimer_.stop();

   onPriceUpdateTimer();

   if (priceUpdateInterval_ > 0) {
      priceUpdateTimer_.start(priceUpdateInterval_);
   }
}

void QuoteRequestsModel::showQuotedRfqs(bool on)
{
   if (showQuoted_ != on) {
      showQuoted_ = on;

      for (auto mit = data_.cbegin(), mlast = data_.cend(); mit != mlast; ++mit) {
         for (auto it = (*mit)->groups_.cbegin(), last = (*mit)->groups_.cend();
            it != last; ++it) {
               const auto idx = createIndex(
                  static_cast<int>(std::distance((*mit)->groups_.cbegin(), it)),
                  static_cast<int>(Column::Product), &(*it)->idx_);
               emit dataChanged(idx, idx);
         }
      }
   }
}

void QuoteRequestsModel::SetAssetManager(const std::shared_ptr<AssetManager>& assetManager)
{
   assetManager_ = assetManager;
}

void QuoteRequestsModel::ticker() {
   std::unordered_set<std::string>  deletedRows;
   const auto timeNow = QDateTime::currentDateTime();

   for (const auto &id : pendingDeleteIds_) {
      forSpecificId(id, [this](Group *g, int idxItem) {
         beginRemoveRows(createIndex(findGroup(&g->idx_), 0, &g->idx_), idxItem, idxItem);
         if (g->rfqs_[static_cast<std::size_t>(idxItem)]->quoted_) {
            --g->quotedRfqsCount_;
         }
         if (g->rfqs_[static_cast<std::size_t>(idxItem)]->visible_) {
            --g->visibleCount_;
            showRfqsFromBack(g);
         }
         g->rfqs_.erase(g->rfqs_.begin() + idxItem);
         endRemoveRows();

         emit invalidateFilterModel();
      });
      deletedRows.insert(id);
   }
   pendingDeleteIds_.clear();

   for (auto qrn : notifications_) {
      const auto timeDiff = timeNow.msecsTo(qrn.second.expirationTime.addMSecs(qrn.second.timeSkewMs));
      if ((timeDiff < 0) || (qrn.second.status == bs::network::QuoteReqNotification::Withdrawn)) {
         forSpecificId(qrn.second.quoteRequestId, [this](Group *grp, int itemIndex) {
            const auto row = findGroup(&grp->idx_);
            beginRemoveRows(createIndex(row, 0, &grp->idx_), itemIndex, itemIndex);
            if (grp->rfqs_[static_cast<std::size_t>(itemIndex)]->quoted_) {
               --grp->quotedRfqsCount_;
            }
            if (grp->rfqs_[static_cast<std::size_t>(itemIndex)]->visible_) {
               --grp->visibleCount_;
               showRfqsFromBack(grp);
            }
            grp->rfqs_.erase(grp->rfqs_.begin() + itemIndex);
            endRemoveRows();

            if ((grp->rfqs_.size() == 0) && (row >= 0)) {
               const auto m = findMarket(grp->idx_.parent_);
               beginRemoveRows(createIndex(m, 0, grp->idx_.parent_), row, row);
               data_[m]->groups_.erase(data_[m]->groups_.begin() + row);
               endRemoveRows();
            } else {
               emit invalidateFilterModel();
            }
         });
         deletedRows.insert(qrn.second.quoteRequestId);
      }
      else if ((qrn.second.status == bs::network::QuoteReqNotification::PendingAck)
         || (qrn.second.status == bs::network::QuoteReqNotification::Replied)) {
         forSpecificId(qrn.second.quoteRequestId, [timeDiff](Group *grp, int itemIndex) {
            grp->rfqs_[static_cast<std::size_t>(itemIndex)]->status_.timeleft_ =
               static_cast<int>(timeDiff);
         });
      }
   }

   for (auto delRow : deletedRows) {
      notifications_.erase(delRow);
   }

   for (const auto &settlContainer : settlContainers_) {
      forSpecificId(settlContainer.second->id(),
         [timeLeft = settlContainer.second->timeLeftMs()](Group *grp, int itemIndex) {
         grp->rfqs_[static_cast<std::size_t>(itemIndex)]->status_.timeleft_ =
            static_cast<int>(timeLeft);
      });
   }

   if (!data_.empty()) {
      for (size_t i = 0; i < data_.size(); ++i) {
         for (size_t j = 0; j < data_[i]->groups_.size(); ++j) {
            if (data_[i]->groups_[j]->rfqs_.empty()) {
               continue;
            }
            emit dataChanged(createIndex(0, static_cast<int>(Column::Status),
                  &data_[i]->groups_[j]->rfqs_.front()->idx_),
               createIndex(static_cast<int>(data_[i]->groups_[j]->rfqs_.size() - 1),
                  static_cast<int>(Column::Status),
                  &data_[i]->groups_[j]->rfqs_.back()->idx_));
         }

         if (!data_[i]->settl_.rfqs_.empty()) {
            const int r = static_cast<int>(data_[i]->groups_.size());
            emit dataChanged(createIndex(r, static_cast<int>(Column::Status),
                  &data_[i]->settl_.rfqs_.front()->idx_),
               createIndex(r + static_cast<int>(data_[i]->settl_.rfqs_.size()) - 1,
                  static_cast<int>(Column::Status),
                  &data_[i]->settl_.rfqs_.back()->idx_));
         }
      }
   }
}

void QuoteRequestsModel::onQuoteNotifCancelled(const QString &reqId)
{
   int row = -1;
   RFQ *rfq = nullptr;

   forSpecificId(reqId.toStdString(), [&](Group *group, int i) {
      row = i;
      rfq = group->rfqs_[static_cast<std::size_t>(i)].get();
      rfq->quotedPriceString_ = tr("pulled");
   });

   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::PendingAck);

   if (row >= 0 && rfq) {
      static const QVector<int> roles({Qt::DisplayRole});
      const QModelIndex idx = createIndex(row, static_cast<int>(Column::QuotedPx), &rfq->idx_);
      emit dataChanged(idx, idx, roles);
   }
}

void QuoteRequestsModel::onQuoteReqCancelled(const QString &reqId, bool byUser)
{
   if (!byUser) {
      return;
   }

   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::Withdrawn);
}

void QuoteRequestsModel::onQuoteRejected(const QString &reqId, const QString &reason)
{
   setStatus(reqId.toStdString(), bs::network::QuoteReqNotification::Rejected, reason);
}

void QuoteRequestsModel::updateBestQuotePrice(const QString &reqId, double price, bool own,
   std::vector<std::pair<QModelIndex, QModelIndex>> *idxs)
{
   int row = -1;
   Group *g = nullptr;

   forSpecificId(reqId.toStdString(), [&](Group *grp, int index) {
      row = index;
      g = grp;
      const auto assetType = grp->rfqs_[static_cast<std::size_t>(index)]->assetType_;

      grp->rfqs_[static_cast<std::size_t>(index)]->bestQuotedPxString_ =
         UiUtils::displayPriceForAssetType(price, assetType);
      grp->rfqs_[static_cast<std::size_t>(index)]->bestQuotedPx_ = price;
      grp->rfqs_[static_cast<std::size_t>(index)]->quotedPriceBrush_ = colorForQuotedPrice(
         grp->rfqs_[static_cast<std::size_t>(index)]->quotedPrice_, price, own);
   });

   if (row >= 0 && g) {
      static const QVector<int> roles({static_cast<int>(Qt::DisplayRole),
         static_cast<int>(Qt::BackgroundRole)});
      const auto idx1 = createIndex(row, static_cast<int>(Column::QuotedPx),
         &g->rfqs_[static_cast<std::size_t>(row)]->idx_);
      const auto idx2 = createIndex(row, static_cast<int>(Column::BestPx),
         &g->rfqs_[static_cast<std::size_t>(row)]->idx_);

      if (!idxs) {
         emit dataChanged(idx1, idx2, roles);
      } else {
         idxs->push_back(std::make_pair(idx1, idx2));
      }
   }
}

void QuoteRequestsModel::onBestQuotePrice(const QString reqId, double price, bool own)
{
   if (priceUpdateInterval_ < 1) {
      updateBestQuotePrice(reqId, price, own);
   } else {
      bestQuotePrices_[reqId] = {price, own};
   }
}

void QuoteRequestsModel::onQuoteReqNotifReplied(const bs::network::QuoteNotification &qn)
{
   int row = -1;
   Group *g = nullptr;

   forSpecificId(qn.quoteRequestId, [&](Group *group, int i) {
      row = i;
      g = group;
      const auto assetType = group->rfqs_[static_cast<std::size_t>(i)]->assetType_;
      const double quotedPrice = (qn.side == bs::network::Side::Buy) ? qn.bidPx : qn.offerPx;

      group->rfqs_[static_cast<std::size_t>(i)]->quotedPriceString_ =
         UiUtils::displayPriceForAssetType(quotedPrice, assetType);
      group->rfqs_[static_cast<std::size_t>(i)]->quotedPrice_ = quotedPrice;
      group->rfqs_[static_cast<std::size_t>(i)]->quotedPriceBrush_ =
         colorForQuotedPrice(quotedPrice, group->rfqs_[static_cast<std::size_t>(i)]->bestQuotedPx_);
   });

   if (row >= 0 && g) {
      static const QVector<int> roles({static_cast<int>(Qt::DisplayRole),
         static_cast<int>(Qt::BackgroundRole)});
      const QModelIndex idx = createIndex(row, static_cast<int>(Column::QuotedPx),
         &g->rfqs_[static_cast<std::size_t>(row)]->idx_);
      emit dataChanged(idx, idx, roles);
   }

   setStatus(qn.quoteRequestId, bs::network::QuoteReqNotification::Replied);
}

QBrush QuoteRequestsModel::colorForQuotedPrice(double quotedPrice, double bestQPrice, bool own)
{
   if (qFuzzyIsNull(quotedPrice) || qFuzzyIsNull(bestQPrice)) {
      return {};
   }

   if (own && qFuzzyCompare(quotedPrice, bestQPrice)) {
      return c_greenColor;
   }

   return c_redColor;
}

void QuoteRequestsModel::onQuoteReqNotifReceived(const bs::network::QuoteReqNotification &qrn)
{
   QString marketName = tr(bs::network::Asset::toString(qrn.assetType));
   auto *market = findMarket(marketName);

   if (!market) {
      beginInsertRows(QModelIndex(), static_cast<int>(data_.size()),
         static_cast<int>(data_.size()));
      data_.push_back(std::unique_ptr<Market>(new Market(marketName,
         appSettings_->get<int>(UiUtils::limitRfqSetting(qrn.assetType)))));
      market = data_.back().get();
      endInsertRows();
   }

   QString groupNameSec = QString::fromStdString(qrn.security);
   auto *group = findGroup(market, groupNameSec);

   if (!group) {
      beginInsertRows(createIndex(findMarket(&market->idx_), 0, &market->idx_),
         static_cast<int>(market->groups_.size()),
         static_cast<int>(market->groups_.size()));
      QFont font;
      font.setBold(true);
      market->groups_.push_back(std::unique_ptr<Group>(new Group(groupNameSec,
         market->limit_, font)));
      market->groups_.back()->idx_.parent_ = &market->idx_;
      group = market->groups_.back().get();
      endInsertRows();
   }

   insertRfq(group, qrn);
}

void QuoteRequestsModel::insertRfq(Group *group, const bs::network::QuoteReqNotification &qrn)
{
   auto itQRN = notifications_.find(qrn.quoteRequestId);

   if (itQRN == notifications_.end()) {
      const auto assetType = assetManager_->GetAssetTypeForSecurity(qrn.security);
      const CurrencyPair cp(qrn.security);
      const bool isBid = (qrn.side == bs::network::Side::Buy) ^ (cp.NumCurrency() == qrn.product);
      const double indicPrice = isBid ? mdPrices_[qrn.security][Role::BidPrice] :
         mdPrices_[qrn.security][Role::OfferPrice];

      beginInsertRows(createIndex(findGroup(&group->idx_), 0, &group->idx_),
         static_cast<int>(group->rfqs_.size()),
         static_cast<int>(group->rfqs_.size()));

      group->rfqs_.push_back(std::unique_ptr<RFQ>(new RFQ(QString::fromStdString(qrn.security),
         QString::fromStdString(qrn.product),
         tr(bs::network::Side::toString(qrn.side)),
         QString(),
         (qrn.assetType == bs::network::Asset::Type::PrivateMarket) ?
            UiUtils::displayCCAmount(qrn.quantity) : UiUtils::displayQty(qrn.quantity, qrn.product),
         QString(),
         (!qFuzzyIsNull(indicPrice) ? UiUtils::displayPriceForAssetType(indicPrice, assetType)
            : QString()),
         QString(),
         {
            quoteReqStatusDesc(qrn.status),
            ((qrn.status == bs::network::QuoteReqNotification::Status::PendingAck)
               || (qrn.status == bs::network::QuoteReqNotification::Status::Replied)),
           30000
         },
         indicPrice, 0.0, 0.0,
         qrn.side,
         assetType,
         qrn.quoteRequestId)));

      group->rfqs_.back()->idx_.parent_ = &group->idx_;

      endInsertRows();

      notifications_[qrn.quoteRequestId] = qrn;

      if (group->limit_ > 0 && group->limit_ > group->visibleCount_) {
         group->rfqs_.back()->visible_ = true;
         ++group->visibleCount_;

         emit invalidateFilterModel();
      }
   }
   else {
      setStatus(qrn.quoteRequestId, qrn.status);
   }
}

bool QuoteRequestsModel::StartCCSignOnOrder(const QString& orderId)
{
   auto it = settlContainers_.find(orderId.toStdString());
   if (it == settlContainers_.end()) {
      return false;
   }

   std::shared_ptr< DealerCCSettlementContainer> container = std::dynamic_pointer_cast<DealerCCSettlementContainer>(it->second);
   if (container != nullptr) {
      return container->startSigning();
   }

   return false;
}

void QuoteRequestsModel::addSettlementContainer(const std::shared_ptr<bs::SettlementContainer> &container)
{
   const auto &id = container->id();
   settlContainers_[id] = container;

   // Use queued connections to not destroy SettlementContainer inside callbacks
   connect(container.get(), &bs::SettlementContainer::failed, this, [this, id] {
      deleteSettlement(id);
   }, Qt::QueuedConnection);
   connect(container.get(), &bs::SettlementContainer::completed, this, [this, id] {
      deleteSettlement(id);
   }, Qt::QueuedConnection);
   connect(container.get(), &bs::SettlementContainer::timerExpired, this, [this, id] {
      deleteSettlement(id);
   }, Qt::QueuedConnection);

   auto *market = findMarket(groupNameSettlements_);

   if (!market) {
      beginInsertRows(QModelIndex(), static_cast<int>(data_.size()),
         static_cast<int>(data_.size()));
      data_.push_back(std::unique_ptr<Market>(new Market(groupNameSettlements_, -1)));
      market = data_.back().get();
      endInsertRows();
   }

   const auto collector = std::make_shared<bs::SettlementStatsCollector>(container);
   const auto assetType = assetManager_->GetAssetTypeForSecurity(container->security());
   const auto amountStr = (container->assetType() == bs::network::Asset::Type::PrivateMarket)
      ? UiUtils::displayCCAmount(container->quantity())
      : UiUtils::displayQty(container->quantity(), container->product());

   beginInsertRows(createIndex(findMarket(&market->idx_), 0, &market->idx_),
      static_cast<int>(market->groups_.size() + market->settl_.rfqs_.size()),
      static_cast<int>(market->groups_.size() + market->settl_.rfqs_.size()));

   market->settl_.rfqs_.push_back(std::unique_ptr<RFQ>(new RFQ(
      QString::fromStdString(container->security()),
      QString::fromStdString(container->product()),
      tr(bs::network::Side::toString(container->side())),
      QString(),
      amountStr,
      QString(),
      UiUtils::displayPriceForAssetType(container->price(), assetType),
      QString(),
      {
         QString(),
         true
      },
      container->price(), 0.0, 0.0,
      container->side(),
      assetType,
      container->id())));

   market->settl_.rfqs_.back()->idx_.parent_ = &market->idx_;

   connect(container.get(), &bs::SettlementContainer::timerStarted,
      [s = market->settl_.rfqs_.back().get(), this,
       row = static_cast<int>(market->groups_.size() + market->settl_.rfqs_.size() - 1)](int msDuration) {
         s->status_.timeout_ = msDuration;
         const QModelIndex idx = createIndex(row, 0, &s->idx_);
         static const QVector<int> roles({static_cast<int>(Role::Timeout)});
         emit dataChanged(idx, idx, roles);
      }
   );

   endInsertRows();
}

void QuoteRequestsModel::clearModel()
{
   beginResetModel();
   data_.clear();
   endResetModel();
}

void QuoteRequestsModel::onDeferredUpdate(const QPersistentModelIndex &index)
{
   if (index.isValid()) {
      emit dataChanged(index, index);
   }
}

void QuoteRequestsModel::onPriceUpdateTimer()
{
   std::vector<std::pair<QModelIndex, QModelIndex>> idxs;

   for (auto it = prices_.cbegin(), last = prices_.cend(); it != last; ++it) {
      updatePrices(it->first, it->second.first, it->second.second, &idxs);
   }

   prices_.clear();

   for (auto it = bestQuotePrices_.cbegin(), last = bestQuotePrices_.cend(); it != last; ++it) {
      updateBestQuotePrice(it->first, it->second.price_, it->second.own_, &idxs);
   }

   bestQuotePrices_.clear();

   if (!idxs.empty()) {
      struct Index {
         int row_;
         int column_;
      };

      std::map<QModelIndex, std::pair<Index, Index>> mapOfIdxs;

      for (const auto &idx: idxs) {
         auto it = mapOfIdxs.find(idx.first.parent());

         if (it != mapOfIdxs.end()) {
            if (idx.first.row() < it->second.first.row_) {
               it->second.first.row_ = idx.first.row();
            }

            if (idx.first.column() < it->second.first.column_) {
               it->second.first.column_ = idx.first.column();
            }

            if (idx.second.row() > it->second.second.row_) {
               it->second.second.row_ = idx.second.row();
            }

            if (idx.second.column() > it->second.second.column_) {
               it->second.second.column_ = idx.second.column();
            }
         } else {
            mapOfIdxs[idx.first.parent()] = std::make_pair<Index, Index>(
               {idx.first.row(), idx.first.column()},
               {idx.second.row(), idx.second.column()});
         }
      }

      for (auto it = mapOfIdxs.cbegin(), last = mapOfIdxs.cend(); it != last; ++it) {
         emit dataChanged(index(it->second.first.row_, it->second.first.column_, it->first),
            index(it->second.second.row_, it->second.second.column_, it->first));
      }
   }
}

void QuoteRequestsModel::deleteSettlement(const std::string &id)
{
   updateSettlementCounters();

   auto it = settlContainers_.find(id);
   if (it != settlContainers_.end()) {
      pendingDeleteIds_.insert(id);
      it->second->deactivate();
      settlContainers_.erase(it);
   }
}

void QuoteRequestsModel::updateSettlementCounters()
{
   auto * market = findMarket(groupNameSettlements_);

   if (market) {
      const int row = findMarket(&market->idx_);

      emit dataChanged(createIndex(row, static_cast<int>(Column::Product), &market->idx_),
         createIndex(row, static_cast<int>(Column::Side), &market->idx_));
   }
}

void QuoteRequestsModel::forSpecificId(const std::string &reqId, const cbItem &cb)
{
   for (size_t i = 0; i < data_.size(); ++i) {
      if (!data_[i]->settl_.rfqs_.empty()) {
         for (size_t k = 0; k < data_[i]->settl_.rfqs_.size(); ++k) {
            if (data_[i]->settl_.rfqs_[k]->reqId_ == reqId) {
               cb(&data_[i]->settl_, static_cast<int>(k));
               return;
            }
         }
      }

      for (size_t j = 0; j < data_[i]->groups_.size(); ++j) {
         for (size_t k = 0; k < data_[i]->groups_[j]->rfqs_.size(); ++k) {
            if (data_[i]->groups_[j]->rfqs_[k]->reqId_ == reqId) {
               cb(data_[i]->groups_[j].get(), static_cast<int>(k));
               return;
            }
         }
      }
   }
}

void QuoteRequestsModel::forEachSecurity(const QString &security, const cbItem &cb)
{
   for (size_t i = 0; i < data_.size(); ++i) {
      for (size_t j = 0; j < data_[i]->groups_.size(); ++j) {
         if (data_[i]->groups_[j]->security_ != security)
            continue;
         for (size_t k = 0; k < data_[i]->groups_[j]->rfqs_.size(); ++k) {
            cb(data_[i]->groups_[j].get(), static_cast<int>(k));
         }
      }
   }
}

const bs::network::QuoteReqNotification &QuoteRequestsModel::getQuoteReqNotification(const std::string &id) const
{
   static bs::network::QuoteReqNotification   emptyQRN;

   auto itQRN = notifications_.find(id);
   if (itQRN == notifications_.end())  return emptyQRN;
   return itQRN->second;
}

double QuoteRequestsModel::getPrice(const std::string &security, Role role) const
{
   const auto itMDP = mdPrices_.find(security);
   if (itMDP != mdPrices_.end()) {
      const auto itP = itMDP->second.find(role);
      if (itP != itMDP->second.end()) {
         return itP->second;
      }
   }
   return 0;
}

QString QuoteRequestsModel::getMarketSecurity(const QModelIndex &index)
{
   for (auto parent = index; parent.isValid(); parent = parent.parent()) {
         IndexHelper *idx = static_cast<IndexHelper*>(parent.internalPointer());

         if (idx->type_ != DataType::Market) {
            continue;
         }

         Market *marketInfo = static_cast<Market*>(idx->data_);
         if (marketInfo) {
            return marketInfo->security_;
         }
   }

   return {};
}

QString QuoteRequestsModel::quoteReqStatusDesc(bs::network::QuoteReqNotification::Status status)
{
   switch (status) {
      case bs::network::QuoteReqNotification::Withdrawn:    return tr("Withdrawn");
      case bs::network::QuoteReqNotification::PendingAck:   return tr("PendingAck");
      case bs::network::QuoteReqNotification::Replied:      return tr("Replied");
      case bs::network::QuoteReqNotification::TimedOut:     return tr("TimedOut");
      case bs::network::QuoteReqNotification::Rejected:     return tr("Rejected");
      default:       return QString();
   }
}

QBrush QuoteRequestsModel::bgColorForStatus(bs::network::QuoteReqNotification::Status status)
{
   switch (status) {
      case bs::network::QuoteReqNotification::Withdrawn:    return Qt::magenta;
      case bs::network::QuoteReqNotification::Rejected:     return c_redColor;
      case bs::network::QuoteReqNotification::Replied:      return c_greenColor;
      case bs::network::QuoteReqNotification::TimedOut:     return c_yellowColor;
      case bs::network::QuoteReqNotification::PendingAck:
      default:       return QBrush();
   }
}

void QuoteRequestsModel::setStatus(const std::string &reqId, bs::network::QuoteReqNotification::Status status
   , const QString &details)
{
   auto itQRN = notifications_.find(reqId);
   if (itQRN != notifications_.end()) {
      itQRN->second.status = status;

      forSpecificId(reqId, [this, status, details](Group *grp, int index) {
         if (!details.isEmpty()) {
            grp->rfqs_[static_cast<std::size_t>(index)]->status_.status_ = details;
         }
         else {
            grp->rfqs_[static_cast<std::size_t>(index)]->status_.status_ =
               quoteReqStatusDesc(status);
         }

         bool emitUpdate = false;

         if (status == bs::network::QuoteReqNotification::Replied) {

            if (!grp->rfqs_[static_cast<std::size_t>(index)]->quoted_) {
               grp->rfqs_[static_cast<std::size_t>(index)]->quoted_ = true;
               ++grp->quotedRfqsCount_;
               emitUpdate = true;
            }

            if (grp->rfqs_[static_cast<std::size_t>(index)]->visible_) {
               grp->rfqs_[static_cast<std::size_t>(index)]->visible_ = false;
               --grp->visibleCount_;
               showRfqsFromBack(grp);
               emitUpdate = true;
            }

            emit invalidateFilterModel();
         }

         if (status == bs::network::QuoteReqNotification::Withdrawn) {
            if (grp->rfqs_[static_cast<std::size_t>(index)]->quoted_) {
               grp->rfqs_[static_cast<std::size_t>(index)]->quoted_ = false;
               --grp->quotedRfqsCount_;
               emit invalidateFilterModel();
               emitUpdate = true;
            }
         }

         grp->rfqs_[static_cast<std::size_t>(index)]->stateBrush_ = bgColorForStatus(status);

         const bool showProgress = ((status == bs::network::QuoteReqNotification::Status::PendingAck)
            || (status == bs::network::QuoteReqNotification::Status::Replied));
         grp->rfqs_[index]->status_.showProgress_ = showProgress;

         const QModelIndex idx = createIndex(index, static_cast<int>(Column::Status),
            &grp->rfqs_[index]->idx_);
         emit dataChanged(idx, idx);

         if (emitUpdate) {
            const QModelIndex gidx = createIndex(findGroup(&grp->idx_), 0, &grp->idx_);
            emit dataChanged(gidx, gidx);
         }
      });

      emit quoteReqNotifStatusChanged(itQRN->second);
   }
}

void QuoteRequestsModel::updatePrices(const QString &security, const bs::network::MDField &pxBid,
   const bs::network::MDField &pxOffer, std::vector<std::pair<QModelIndex, QModelIndex>> *idxs)
{
   forEachSecurity(security, [security, pxBid, pxOffer, this, idxs](Group *grp, int index) {
      const CurrencyPair cp(security.toStdString());
      const bool isBuy = (grp->rfqs_[static_cast<std::size_t>(index)]->side_ == bs::network::Side::Buy)
         ^ (cp.NumCurrency() == grp->rfqs_[static_cast<std::size_t>(index)]->product_.toStdString());
      double indicPrice = 0;

      if (isBuy && (pxBid.type != bs::network::MDField::Unknown)) {
         indicPrice = pxBid.value;
      } else if (!isBuy && (pxOffer.type != bs::network::MDField::Unknown)) {
         indicPrice = pxOffer.value;
      }

      if (indicPrice > 0) {
         const auto prevPrice = grp->rfqs_[static_cast<std::size_t>(index)]->indicativePx_;
         const auto assetType = grp->rfqs_[static_cast<std::size_t>(index)]->assetType_;
         grp->rfqs_[static_cast<std::size_t>(index)]->indicativePxString_ =
            UiUtils::displayPriceForAssetType(indicPrice, assetType);
         grp->rfqs_[static_cast<std::size_t>(index)]->indicativePx_ = indicPrice;

         if (!qFuzzyIsNull(prevPrice)) {
            if (indicPrice > prevPrice) {
               grp->rfqs_[static_cast<std::size_t>(index)]->indicativePxBrush_ = c_greenColor;
            }
            else if (indicPrice < prevPrice) {
               grp->rfqs_[static_cast<std::size_t>(index)]->indicativePxBrush_ = c_redColor;
            }
         }

         const QModelIndex idx = createIndex(index, static_cast<int>(Column::IndicPx),
            &grp->rfqs_[static_cast<std::size_t>(index)]->idx_);

         if (!idxs) {
            emit dataChanged(idx, idx);
         } else {
            idxs->push_back(std::make_pair(idx, idx));
         }
      }
   });
}

void QuoteRequestsModel::showRfqsFromBack(Group *g)
{
   if (g->limit_ > 0) {
      for (auto it = g->rfqs_.rbegin(), last = g->rfqs_.rend(); it != last; ++it) {
         if (g->visibleCount_ < g->limit_) {
            if (!(*it)->quoted_ && !(*it)->visible_) {
               (*it)->visible_ = true;
               ++g->visibleCount_;
            }
         } else {
            break;
         }
      }
   }
}

void QuoteRequestsModel::showRfqsFromFront(Group *g)
{
   if (g->limit_ > 0) {
      for (auto it = g->rfqs_.begin(), last = g->rfqs_.end(); it != last; ++it) {
         if (g->visibleCount_ < g->limit_) {
            if (!(*it)->quoted_ && !(*it)->visible_) {
               (*it)->visible_ = true;
               ++g->visibleCount_;
            }
         } else {
            break;
         }
      }
   }
}

void QuoteRequestsModel::clearVisibleFlag(Group *g)
{
   if (g->limit_ > 0) {
      for (auto it = g->rfqs_.begin(), last = g->rfqs_.end(); it != last; ++it) {
         (*it)->visible_ = false;
      }

      g->visibleCount_ = 0;
   }
}

void QuoteRequestsModel::onSecurityMDUpdated(const QString &security, const bs::network::MDFields &mdFields)
{
   const auto pxBid = bs::network::MDField::get(mdFields, bs::network::MDField::PriceBid);
   const auto pxOffer = bs::network::MDField::get(mdFields, bs::network::MDField::PriceOffer);
   if (pxBid.type != bs::network::MDField::Unknown) {
      mdPrices_[security.toStdString()][Role::BidPrice] = pxBid.value;
   }
   if (pxOffer.type != bs::network::MDField::Unknown) {
      mdPrices_[security.toStdString()][Role::OfferPrice] = pxOffer.value;
   }

   if (priceUpdateInterval_ < 1) {
      updatePrices(security, pxBid, pxOffer);
   } else {
      prices_[security] = std::make_pair(pxBid, pxOffer);
   }
}

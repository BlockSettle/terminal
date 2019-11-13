#include "OrderListModel.h"

#include "AssetManager.h"
#include "QuoteProvider.h"
#include "UiUtils.h"

#include "bs_proxy_terminal_pb.pb.h"

namespace {

   const auto kNewOrderColor = QColor{0xFF, 0x7F, 0};
   const auto kPendingColor = QColor{0x63, 0xB0, 0xB2};
   const auto kRevokedColor = QColor{0xf6, 0xa7, 0x24};
   const auto kSettledColor = QColor{0x22, 0xC0, 0x64};
   const auto kFailedColor = QColor{0xEC, 0x0A, 0x35};

} // namespace

QString OrderListModel::Header::toString(OrderListModel::Header::Index h)
{
   switch (h) {
   case Time:        return tr("Time");
   case Product:     return tr("Product");
   case Side:        return tr("Side");
   case Quantity:    return tr("Quantity");
   case Price:       return tr("Price");
   case Value:       return tr("Value");
   case Status:      return tr("Status");
   default:          return tr("Undefined");
   }
}

QString OrderListModel::StatusGroup::toString(OrderListModel::StatusGroup::Type sg)
{
   switch (sg) {
   case StatusGroup::UnSettled:  return tr("UnSettled");
   case StatusGroup::Settled:    return tr("Settled");
   default:    return tr("Unknown");
   }
}


OrderListModel::OrderListModel(const std::shared_ptr<AssetManager>& assetManager, QObject* parent)
   : QAbstractItemModel(parent)
   , assetManager_(assetManager)
{
   reset();
}

int OrderListModel::columnCount(const QModelIndex &) const
{
   return Header::last;
}

QVariant OrderListModel::data(const QModelIndex &index, int role) const
{
   if (index.isValid()) {
      auto idx = static_cast<IndexHelper*>(index.internalPointer());

      switch (idx->type_) {
         case DataType::Data : {
            auto d = static_cast<Data*>(idx->data_);

            switch (role) {
               case Qt::DisplayRole : {
                  switch (index.column()) {
                     case Header::Time :
                        return d->time_;

                     case Header::Product :
                        return d->product_;

                     case Header::Side :
                        return d->side_;

                     case Header::Quantity :
                        return d->quantity_;

                     case Header::Price :
                        return d->price_;

                     case Header::Value :
                        return d->value_;

                     case Header::Status :
                        return d->status_;
                  }
               }

               case Qt::TextAlignmentRole : {
                  switch (index.column()) {
                     case Header::Quantity :
                     case Header::Price :
                     case Header::Value :
                        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

                     default :
                        return QVariant();
                  }
               }

               case Qt::ForegroundRole : {
                  if (index.column() == Header::Status) {
                     return d->statusColor_;
                  } else {
                     return QVariant();
                  }
               }

               default :
                  return QVariant();
            }
         }

         case DataType::Group : {
            auto g = static_cast<Group*>(idx->data_);

            switch (role) {
               case Qt::DisplayRole : {
                  if (index.column() == 0) {
                     return g->security_;
                  } else {
                     return QVariant();
                  }
               }

               default :
                  return QVariant();
            }
         }

         case DataType::Market : {
            auto g = static_cast<Market*>(idx->data_);

            switch (role) {
               case Qt::DisplayRole : {
                  if (index.column() == 0) {
                     return g->name_;
                  } else {
                     return QVariant();
                  }
               }

               case Qt::FontRole : {
                  return g->font_;
               }

               default :
                  return QVariant();
            }
         }

         case DataType::StatusGroup : {
            auto g = static_cast<StatusGroup*>(idx->data_);

            switch (role) {
               case Qt::DisplayRole : {
                  if (index.column() == 0) {
                     return g->name_;
                  } else {
                     return QVariant();
                  }
               }

               default :
                  return QVariant();
            }
         }

         default :
            return QVariant();
      }
   } else {
      return QVariant();
   }
}

QModelIndex OrderListModel::index(int row, int column, const QModelIndex &parent) const
{
   if (parent.isValid()) {
      auto idx = static_cast<IndexHelper*>(parent.internalPointer());

      switch (idx->type_) {
         case DataType::Market : {
            auto m = static_cast<Market*>(idx->data_);

            if (row < static_cast<int>(m->rows_.size())) {
               return createIndex(row, column, &m->rows_[static_cast<std::size_t>(row)]->idx_);
            } else {
               return QModelIndex();
            }
         }

         case DataType::Group : {
            auto g = static_cast<Group*>(idx->data_);

            if (row < static_cast<int>(g->rows_.size())) {
               return createIndex(row, column, &g->rows_[static_cast<std::size_t>(row)]->idx_);
            } else {
               return QModelIndex();
            }
         }

         case DataType::StatusGroup : {
            auto g = static_cast<StatusGroup*>(idx->data_);

            if (row < static_cast<int>(g->rows_.size())) {
               return createIndex(row, column, &g->rows_[static_cast<std::size_t>(row)]->idx_);
            } else {
               return QModelIndex();
            }
         }

         default :
            return QModelIndex();
      }
   } else {
      switch (row) {
         case StatusGroup::UnSettled :
            return createIndex(row, column, &unsettled_->idx_);

         case StatusGroup::Settled :
            return createIndex(row, column, &settled_->idx_);

         default :
            return QModelIndex();
      }
   }
}

int OrderListModel::findGroup(Market *market, Group *group) const
{
   const auto it = std::find_if(market->rows_.cbegin(), market->rows_.cend(),
      [group] (const std::unique_ptr<Group> &g) { return (&group->idx_ == &g->idx_); });

   if (it != market->rows_.cend()) {
      return static_cast<int> (std::distance(market->rows_.cbegin(), it));
   } else {
      return -1;
   }
}

int OrderListModel::findMarket(StatusGroup *statusGroup, Market *market) const
{
   const auto it = std::find_if(statusGroup->rows_.cbegin(), statusGroup->rows_.cend(),
      [market] (const std::unique_ptr<Market> &m) { return (&market->idx_ == &m->idx_); });

   if (it != statusGroup->rows_.cend()) {
      return static_cast<int> (std::distance(statusGroup->rows_.cbegin(), it));
   } else {
      return -1;
   }
}

QModelIndex OrderListModel::parent(const QModelIndex &index) const
{
   if (index.isValid()) {
      auto idx = static_cast<IndexHelper*>(index.internalPointer());

      if (idx->parent_) {
         switch (idx->parent_->type_) {
            case DataType::Market : {
               auto m = static_cast<Market*>(idx->parent_->data_);

               return createIndex(findMarket(static_cast<StatusGroup*>(idx->parent_->parent_->data_),
                  m), 0, &m->idx_);
            }

            case DataType::Group : {
               auto g = static_cast<Group*>(idx->parent_->data_);

               return createIndex(findGroup(static_cast<Market*>(idx->parent_->parent_->data_),
                  g), 0, &g->idx_);
            }

            case DataType::StatusGroup : {
               auto g = static_cast<StatusGroup*>(idx->parent_->data_);

               return createIndex(g->row_, 0, &g->idx_);
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

int OrderListModel::rowCount(const QModelIndex &parent) const
{
   if (!parent.isValid()) {
      return 2;
   }

   auto idx = static_cast<IndexHelper*>(parent.internalPointer());

   switch (idx->type_) {
      case DataType::Market : {
         auto m = static_cast<Market*>(idx->data_);

         return static_cast<int>(m->rows_.size());
      }

      case DataType::Group : {
         auto g = static_cast<Group*>(idx->data_);

         return static_cast<int>(g->rows_.size());
      }

      case DataType::StatusGroup : {
         auto g = static_cast<StatusGroup*>(idx->data_);

         return static_cast<int>(g->rows_.size());
      }

      default :
         return 0;
   }
}

QVariant OrderListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role != Qt::DisplayRole) {
      return QVariant();
   }

   return Header::toString(static_cast<Header::Index>(section));
}

void OrderListModel::onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
{
   switch (response.data_case()) {
      case Blocksettle::Communication::ProxyTerminalPb::Response::kUpdateOrders:
         processUpdateOrders(response.update_orders());
         break;
      default:
         break;
   }
}

void OrderListModel::setOrderStatus(Group *group, int index, const bs::network::Order& order,
   bool emitUpdate)
{
   assert(index >= 0 && index < group->rows_.size());
   auto &rowData = group->rows_[static_cast<std::size_t>(index)];
   bool isStatusChanged = false;

   auto setNewStatusIfNeeded = [&](QString &&newStatus, QColor newColor) {
      isStatusChanged = rowData->status_ != newStatus;
      if (isStatusChanged) {
         rowData->status_ = std::move(newStatus);
         rowData->statusColor_ = newColor;
      }
   };

   switch (order.status)
   {
      case bs::network::Order::New:
         // New is not used currently
      case bs::network::Order::Pending: {
         auto status = order.pendingStatus.empty() ? tr("Pending") : QString::fromStdString(order.pendingStatus);

         auto color = kPendingColor;
         if (status.toLower().startsWith(QStringLiteral("revoke"))) {
             color = kRevokedColor;
         } else if (status.toLower().startsWith(QStringLiteral("pay-in")) || order.pendingStatus.empty()) {
            // try to use different color for new orders
            color = kNewOrderColor;
         }

         setNewStatusIfNeeded(std::move(status), color);
         break;
      }

      case bs::network::Order::Filled:
         setNewStatusIfNeeded(tr("Settled"), kSettledColor);
         break;

      case bs::network::Order::Failed:
         setNewStatusIfNeeded(tr("Failed"), kFailedColor);
         break;
   }

   const auto idx = createIndex(index, Header::Status,
      &group->rows_[static_cast<std::size_t>(index)]->idx_);
   const auto persistentIdx = QPersistentModelIndex(idx);

   if (isStatusChanged && emitUpdate) {
      emit dataChanged(idx, idx);
   }

   if (!latestOrderTimestamp_.isValid() || order.dateTime > latestOrderTimestamp_) {
      latestOrderTimestamp_ = order.dateTime;
      emit newOrder(persistentIdx);
   }

   // We should highlight latest changed if there any
   // and if not let's highlight the most recent timestamp
   if (latestChangedTimestamp_.isValid() && order.dateTime == latestChangedTimestamp_) {
      emit selectRow(idx);
   }
   else if (!latestChangedTimestamp_.isValid() && order.dateTime == latestOrderTimestamp_) {
      emit selectRow(idx);
   }
}

OrderListModel::StatusGroup::Type OrderListModel::getStatusGroup(const bs::network::Order& order)
{
   switch (order.status) {
      case bs::network::Order::New:
      case bs::network::Order::Pending:
         return StatusGroup::UnSettled;

      case bs::network::Order::Filled:
      case bs::network::Order::Failed:
         return StatusGroup::Settled;
   }

   return StatusGroup::last;
}

std::pair<OrderListModel::Group*, int> OrderListModel::findItem(const bs::network::Order &order)
{
   const auto itGroups = groups_.find(order.exchOrderId.toStdString());
   const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));
   StatusGroup *sg = nullptr;

   if (itGroups != groups_.end()) {
      sg = (itGroups->second == StatusGroup::UnSettled ? unsettled_.get() : settled_.get());

      auto it = std::find_if(sg->rows_.cbegin(), sg->rows_.cend(),
         [assetGrpName] (const std::unique_ptr<Market> & m) { return (m->name_ == assetGrpName); });

      if (it != sg->rows_.cend()) {
         auto git = std::find_if((*it)->rows_.cbegin(), (*it)->rows_.cend(),
            [&order] (const std::unique_ptr<Group> &g)
               { return (order.security == g->security_.toStdString()); } );

         if (git != (*it)->rows_.cend()) {
            auto dit = std::find_if((*git)->rows_.cbegin(), (*git)->rows_.cend(),
               [&order] (const std::unique_ptr<Data> &d) { return (d->id_ == order.exchOrderId); });

            if (dit != (*git)->rows_.cend()) {
               return std::make_pair(git->get(),
                  static_cast<int>(std::distance((*git)->rows_.cbegin(), dit)));
            } else {
               return std::make_pair(git->get(), -1);
            }
         } else {
            return std::make_pair(nullptr, -1);
         }
      } else {
         return std::make_pair(nullptr, -1);
      }
   }

   sg = (getStatusGroup(order) == StatusGroup::UnSettled ? unsettled_.get() : settled_.get());

   auto it = std::find_if(sg->rows_.cbegin(), sg->rows_.cend(),
      [assetGrpName] (const std::unique_ptr<Market> & g) { return (g->name_ == assetGrpName); });

   if (it != sg->rows_.cend()) {
      auto git = std::find_if((*it)->rows_.cbegin(), (*it)->rows_.cend(),
         [&order] (const std::unique_ptr<Group> &g)
            { return (order.security == g->security_.toStdString()); } );

      if (git != (*it)->rows_.cend()) {
         return std::make_pair(git->get(), -1);
      } else {
         return std::make_pair(nullptr, -1);
      }
   } else {
      return std::make_pair(nullptr, -1);
   }
}

void OrderListModel::removeRowIfContainerChanged(const bs::network::Order &order,
   int &oldOrderRow)
{
   // Remove row if container (settled/unsettled) changed.
   auto git = groups_.find(order.exchOrderId.toStdString());

   if (git != groups_.end() && git->second != getStatusGroup(order) && oldOrderRow >= 0) {
      StatusGroup *tmpsg = (git->second == StatusGroup::UnSettled ? unsettled_.get() :
         settled_.get());

      const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));

      auto mit = std::find_if(tmpsg->rows_.cbegin(), tmpsg->rows_.cend(),
         [assetGrpName] (const std::unique_ptr<Market> & m) { return (m->name_ == assetGrpName); });

      if (mit != tmpsg->rows_.cend()) {
         auto git = std::find_if((*mit)->rows_.cbegin(), (*mit)->rows_.cend(),
            [&order] (const std::unique_ptr<Group> &g)
               { return (order.security == g->security_.toStdString()); });

         if (git != (*mit)->rows_.cend()) {
            const auto groupRow = findGroup(mit->get(), git->get());
            const auto didx = createIndex(groupRow, 0, &(*git)->idx_);

            beginRemoveRows(didx, oldOrderRow, oldOrderRow);
            (*git)->rows_.erase((*git)->rows_.begin() + oldOrderRow);
            oldOrderRow = -1;
            endRemoveRows();

            const auto marketRow = findMarket(tmpsg, mit->get());

            if (!(*git)->rows_.size()) {
               beginRemoveRows(createIndex(marketRow, 0, &(*mit)->idx_),
                  groupRow, groupRow);
               (*mit)->rows_.erase(git);
               endRemoveRows();
            }

            if (!(*mit)->rows_.size()) {
               beginRemoveRows(createIndex(tmpsg->row_, 0, &tmpsg->idx_), marketRow, marketRow);
               tmpsg->rows_.erase(mit);
               endRemoveRows();
            }
         }
      }
   }
}

void OrderListModel::findMarketAndGroup(const bs::network::Order &order, Market *&market,
   Group *&group)
{
   StatusGroup *sg = (getStatusGroup(order) == StatusGroup::UnSettled ? unsettled_.get() :
      settled_.get());

   const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));

   auto mit = std::find_if(sg->rows_.cbegin(), sg->rows_.cend(),
      [assetGrpName] (const std::unique_ptr<Market> & m) { return (m->name_ == assetGrpName); });

   if (mit != sg->rows_.cend()) {
      auto git = std::find_if((*mit)->rows_.cbegin(), (*mit)->rows_.cend(),
         [&order] (const std::unique_ptr<Group> & g)
            { return (g->security_.toStdString() == order.security); });

      if (git != (*mit)->rows_.cend()) {
         group = git->get();
         market = mit->get();
      } else {
         group = nullptr;
         market = mit->get();
      }
   } else {
      group = nullptr;
      market = nullptr;
   }
}

void OrderListModel::createGroupsIfNeeded(const bs::network::Order &order, Market *&marketItem,
   Group *&groupItem)
{
   QModelIndex sidx = (getStatusGroup(order) == StatusGroup::UnSettled ?
      createIndex(0, 0, &unsettled_->idx_) : createIndex(1, 0, &settled_->idx_));
   StatusGroup *sg = (getStatusGroup(order) == StatusGroup::UnSettled ? unsettled_.get() :
      settled_.get());

   // Create market if it doesn't exist.
   if (!marketItem) {
      beginInsertRows(sidx, static_cast<int>(sg->rows_.size()), static_cast<int>(sg->rows_.size()));
      sg->rows_.push_back(make_unique<Market>(
         tr(bs::network::Asset::toString(order.assetType)), &sg->idx_));
      marketItem = sg->rows_.back().get();
      endInsertRows();
   }

   // Create group if it doesn't exist.
   if (!groupItem) {
      beginInsertRows(createIndex(findMarket(sg, marketItem), 0, &marketItem->idx_),
         static_cast<int>(marketItem->rows_.size()), static_cast<int>(marketItem->rows_.size()));
      marketItem->rows_.push_back(make_unique<Group>(
         QString::fromStdString(order.security), &marketItem->idx_));
      groupItem = marketItem->rows_.back().get();
      endInsertRows();
   }
}

void OrderListModel::reset()
{
   beginResetModel();
   groups_.clear();
   unsettled_ = std::make_unique<StatusGroup>(StatusGroup::toString(StatusGroup::UnSettled), 0);
   settled_ = std::make_unique<StatusGroup>(StatusGroup::toString(StatusGroup::Settled), 1);
   endResetModel();
}

void OrderListModel::processUpdateOrders(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrders &message)
{
   // Save latest selected index first
   resetLatestChangedStatus(message);
   // OrderListModel supposed to work correctly when orders states updated one by one.
   // We don't use this anymore (server sends all active orders every time) so just clear old caches.
   // Remove this if old behavior is needed
   reset();
   // Use some fake orderId so old code works correctly
   int orderId = 0;

   for (const auto &data : message.orders()) {
      bs::network::Order order;

      switch (data.status()) {
         case bs::types::ORDER_STATUS_PENDING:
            order.status = bs::network::Order::Pending;
            break;
         case bs::types::ORDER_STATUS_FILLED:
            order.status = bs::network::Order::Filled;
            break;
         case bs::types::ORDER_STATUS_VOID:
            order.status = bs::network::Order::Failed;
            break;
         default:
            break;
      }

      const bool isXBT = (data.product() == "XBT") || (data.product_against() == "XBT");
      const bool isCC = (assetManager_->getCCLotSize(data.product())) > 0
         || (assetManager_->getCCLotSize(data.product_against()) > 0);

      if (isCC) {
         order.assetType = bs::network::Asset::PrivateMarket;
      } else if (isXBT) {
         order.assetType = bs::network::Asset::SpotXBT;
      } else {
         order.assetType = bs::network::Asset::SpotFX;
      }

      orderId += 1;
      order.exchOrderId = QString::number(orderId);
      order.side = bs::network::Side::Type(data.side());
      order.pendingStatus = data.status_text();
      order.dateTime = QDateTime::fromMSecsSinceEpoch(data.timestamp_ms());
      order.product = data.product();
      order.quantity = data.quantity();
      order.security = data.product() + "/" + data.product_against();
      order.price = data.price();

      onOrderUpdated(order);
   }
}

void OrderListModel::resetLatestChangedStatus(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrders &message)
{
   latestChangedTimestamp_ = {};

   std::vector<std::pair<int64_t, int>> newOrderStatuses(message.orders_size());
   for (const auto &data : message.orders()) {
      newOrderStatuses.push_back({ data.timestamp_ms(), static_cast<int>(data.status()) });
   }
   std::sort(newOrderStatuses.begin(), newOrderStatuses.end(), [&](const auto &left, const auto &right) {
      return left.first < right.first;
   });

   // if length the same -> we could relay that only row status is changed
   if (sortedPeviousOrderStatuses_.size() == newOrderStatuses.size()) {
      for (int i = 0; i < newOrderStatuses.size(); ++i) {
         // Order changed nothing to do
         if (sortedPeviousOrderStatuses_[i].first != newOrderStatuses[i].first) {
            break;
         }

         if (sortedPeviousOrderStatuses_[i].second != newOrderStatuses[i].second) {
            latestChangedTimestamp_ = QDateTime::fromMSecsSinceEpoch(newOrderStatuses[i].first);
            break;
         }
      }
   }

   sortedPeviousOrderStatuses_ = std::move(newOrderStatuses);
}

void OrderListModel::onOrderUpdated(const bs::network::Order& order)
{
   auto found = findItem(order);

   Group *groupItem = nullptr;
   Market *marketItem = nullptr;

   removeRowIfContainerChanged(order, found.second);

   findMarketAndGroup(order, marketItem, groupItem);

   createGroupsIfNeeded(order, marketItem, groupItem);

   groups_[order.exchOrderId.toStdString()] = getStatusGroup(order);

   const auto parentIndex = createIndex(findGroup(marketItem, groupItem), 0, &groupItem->idx_);

   if (found.second < 0) {
      beginInsertRows(parentIndex, 0, 0);

      // As quantity is now could be negative need to invert value
      double value = - order.quantity * order.price;
      if (order.security.substr(0, order.security.find('/')) != order.product) {
         value = order.quantity / order.price;
      }
      const auto priceAssetType = assetManager_->GetAssetTypeForSecurity(order.security);

      groupItem->rows_.push_front(make_unique<Data>(
         UiUtils::displayTimeMs(order.dateTime),
         QString::fromStdString(order.product),
         tr(bs::network::Side::toString(order.side)),
         UiUtils::displayQty(order.quantity, order.security, order.product, order.assetType),
         UiUtils::displayPriceForAssetType(order.price, priceAssetType),
         UiUtils::displayValue(value, order.security, order.product, order.assetType),
         QString(),
         order.exchOrderId,
         &groupItem->idx_));

      setOrderStatus(groupItem, 0, order);

      endInsertRows();
   }
   else {
      setOrderStatus(groupItem, found.second, order, true);
   }
}

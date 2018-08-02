#include "OrderListModel.h"
#include "AssetManager.h"
#include "QuoteProvider.h"
#include "UiUtils.h"

QString OrderListModel::Header::toString(OrderListModel::Header::Index h)
{
   switch (h) {
   case Time:        return tr("Time");
   case SecurityID:  return tr("SecurityID");
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


OrderListModel::OrderListModel(std::shared_ptr<QuoteProvider> quoteProvider, const std::shared_ptr<AssetManager>& assetManager, QObject* parent)
   : QAbstractItemModel(parent)
   , quoteProvider_(quoteProvider)
   , assetManager_(assetManager)
   , unsettled_(new StatusGroup(StatusGroup::toString(StatusGroup::UnSettled), 0))
   , settled_(new StatusGroup(StatusGroup::toString(StatusGroup::Settled), 1))
{
   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &OrderListModel::onOrderUpdated);
}

int OrderListModel::columnCount(const QModelIndex &) const
{
   return 8;
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

                     case Header::SecurityID :
                        return d->security_;

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
         case 0 :
            return createIndex(row, column, &unsettled_->idx_);

         case 1 :
            return createIndex(row, column, &settled_->idx_);

         default :
            return QModelIndex();
      }
   }
}

int OrderListModel::findGroup(StatusGroup *statusGroup, Group *group) const
{
   const auto it = std::find_if(statusGroup->rows_.cbegin(), statusGroup->rows_.cend(),
      [group] (const std::unique_ptr<Group> &g) { return (&group->idx_ == &g->idx_); });

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
            case DataType::Group : {
               auto g = static_cast<Group*>(idx->parent_->data_);

               return createIndex(findGroup(static_cast<StatusGroup*>(idx->parent_->parent_->data_),
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

void OrderListModel::setOrderStatus(Group *group, int index, const bs::network::Order& order,
   bool emitUpdate)
{
   switch (order.status)
   {
      case bs::network::Order::New:
         group->rows_[static_cast<std::size_t>(index)]->status_ = tr("New");
      case bs::network::Order::Pending:
      {
         if (!order.pendingStatus.empty()) {
            auto statusString = QString::fromStdString(order.pendingStatus);
            group->rows_[static_cast<std::size_t>(index)]->status_ = statusString;
            if (statusString.startsWith(QLatin1String("Revoke"))) {
               group->rows_[static_cast<std::size_t>(index)]->statusColor_ =
                  QColor{0xf6, 0xa7, 0x24};
               break;
            }
         }
         group->rows_[static_cast<std::size_t>(index)]->statusColor_ = QColor{0x63, 0xB0, 0xB2};
      }
         break;

      case bs::network::Order::Filled:
         group->rows_[static_cast<std::size_t>(index)]->status_ = tr("Settled");
         group->rows_[static_cast<std::size_t>(index)]->statusColor_ = QColor{0x22, 0xC0, 0x64};
         break;

      case bs::network::Order::Failed:
         group->rows_[static_cast<std::size_t>(index)]->status_ = tr("Failed");
         group->rows_[static_cast<std::size_t>(index)]->statusColor_ = QColor{0xEC, 0x0A, 0x35};
         break;

      default:
         group->rows_[static_cast<std::size_t>(index)]->status_ = tr("Unknown");
         break;
   }

   if (emitUpdate) {
      const auto idx = createIndex(index, Header::Status,
         &group->rows_[static_cast<std::size_t>(index)]->idx_);

      emit dataChanged(idx, idx);
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

      default: return StatusGroup::last;
   }
}

std::pair<OrderListModel::Group*, int> OrderListModel::findItem(const bs::network::Order &order)
{
   const auto itGroups = groups_.find(order.exchOrderId.toStdString());
   const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));
   StatusGroup *sg = nullptr;

   if (itGroups != groups_.end()) {
      sg = (itGroups->second == StatusGroup::UnSettled ? unsettled_.get() : settled_.get());

      auto it = std::find_if(sg->rows_.cbegin(), sg->rows_.cend(),
         [assetGrpName] (const std::unique_ptr<Group> & g) { return (g->name_ == assetGrpName); });

      if (it != sg->rows_.cend()) {
         auto dit = std::find_if((*it)->rows_.cbegin(), (*it)->rows_.cend(),
            [order] (const std::unique_ptr<Data> &d) { return (d->id_ == order.exchOrderId); });

         if (dit != (*it)->rows_.cend()) {
            std::make_pair(it->get(), static_cast<int>(std::distance((*it)->rows_.cbegin(), dit)));
         } else {
            return std::make_pair(it->get(), -1);
         }
      } else {
         return std::make_pair(nullptr, -1);
      }
   }

   sg = (getStatusGroup(order) == StatusGroup::UnSettled ? unsettled_.get() : settled_.get());

   auto it = std::find_if(sg->rows_.cbegin(), sg->rows_.cend(),
      [assetGrpName] (const std::unique_ptr<Group> & g) { return (g->name_ == assetGrpName); });

   return std::make_pair((it != sg->rows_.cend() ? it->get() : nullptr), -1);
}

void OrderListModel::onOrderUpdated(const bs::network::Order& order)
{
   const auto found = findItem(order);
   auto groupItem = found.first;
   auto index = found.second;
   StatusGroup *sg = (getStatusGroup(order) == StatusGroup::UnSettled ? unsettled_.get() :
      settled_.get());
   QModelIndex sidx = (getStatusGroup(order) == StatusGroup::UnSettled ?
      createIndex(0, 0, &unsettled_->idx_) : createIndex(1, 0, &settled_->idx_));

   // Remove row if container (settled/unsettled) changed.
   auto git = groups_.find(order.exchOrderId.toStdString());

   if (git != groups_.end() && git->second != getStatusGroup(order) && index >= 0) {
      StatusGroup *tmpsg = (git->second == StatusGroup::UnSettled ? unsettled_.get() :
         settled_.get());

      const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));

      auto it = std::find_if(tmpsg->rows_.cbegin(), tmpsg->rows_.cend(),
         [assetGrpName] (const std::unique_ptr<Group> & g) { return (g->name_ == assetGrpName); });

      if (it != tmpsg->rows_.cend()) {
         const auto didx = createIndex(findGroup(tmpsg, it->get()), 0, &(*it)->idx_);

         beginRemoveRows(didx, index, index);
         (*it)->rows_.erase((*it)->rows_.begin() + index);
         index = -1;
         endRemoveRows();

         if (!(*it)->rows_.size()) {
            const int gdist = static_cast<int>(std::distance(tmpsg->rows_.cbegin(), it));
            beginRemoveRows(createIndex(tmpsg->row_, 0, &tmpsg->idx_), gdist, gdist);
            tmpsg->rows_.erase(it);
            groupItem = nullptr;
            endRemoveRows();
         }
      }
   }

   // Create group if it doesn't exist.
   if (!groupItem) {
      beginInsertRows(sidx, static_cast<int>(sg->rows_.size()), static_cast<int>(sg->rows_.size()));
      sg->rows_.push_back(std::unique_ptr<Group>(new Group(
         tr(bs::network::Asset::toString(order.assetType)), &sg->idx_)));
      groupItem = sg->rows_.back().get();
      endInsertRows();
   }

   groups_[order.exchOrderId.toStdString()] = getStatusGroup(order);

   if (index < 0) {
      beginInsertRows(createIndex(findGroup(sg, groupItem), 0, &groupItem->idx_),
         static_cast<int>(groupItem->rows_.size()), static_cast<int>(groupItem->rows_.size()));

      double value = order.quantity * order.price;
      if (order.security.substr(0, order.security.find('/')) != order.product) {
         value = order.quantity / order.price;
      }
      const auto priceAssetType = assetManager_->GetAssetTypeForSecurity(order.security);

      groupItem->rows_.push_back(std::unique_ptr<Data>(new Data(
         UiUtils::displayTimeMs(order.dateTime),
         QString::fromStdString(order.security),
         QString::fromStdString(order.product),
         tr(bs::network::Side::toString(order.side)),
         UiUtils::displayQty(order.quantity, order.security, order.product, order.assetType),
         UiUtils::displayPriceForAssetType(order.price, priceAssetType),
         UiUtils::displayValue(value, order.security, order.product, order.assetType),
         QString(),
         order.exchOrderId,
         &groupItem->idx_)));

      setOrderStatus(groupItem, static_cast<int>(groupItem->rows_.size()) - 1, order);

      endInsertRows();
   }
   else {
      setOrderStatus(groupItem, index, order, true);
   }
}

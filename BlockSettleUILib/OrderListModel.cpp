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

QStringList OrderListModel::Header::labels()
{
   QStringList rv;
   for (int i = Header::first; i < Header::last; i++) {
      rv << Header::toString(static_cast<Header::Index>(i));
   }
   return rv;
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
   : QStandardItemModel(parent)
   , quoteProvider_(quoteProvider)
   , assetManager_(assetManager)
{
   for (int i = static_cast<int>(StatusGroup::first); i < static_cast<int>(StatusGroup::last); i++) {
      const auto group = static_cast<StatusGroup::Type>(i);
      const auto groupItem = new QStandardItem(StatusGroup::toString(group));
      appendRow(QList<QStandardItem*>() << groupItem);
   }

   setHorizontalHeaderLabels(Header::labels());

   connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &OrderListModel::onOrderUpdated);
}

void OrderListModel::setOrderStatus(QStandardItem* item, const bs::network::Order& order)
{
   switch (order.status)
   {
      case bs::network::Order::New:
         item->setText(tr("New"));
      case bs::network::Order::Pending:
      {
         if (!order.pendingStatus.empty()) {
            auto statusString = QString::fromStdString(order.pendingStatus);
            item->setText(statusString);
            if (statusString.startsWith(QLatin1String("Revoke"))) {
               item->setForeground(QColor{0xf6, 0xa7, 0x24});
               break;
            }
         }
         item->setForeground(QColor{0x63, 0xB0, 0xB2});
      }
         break;
      case bs::network::Order::Filled:
         item->setText(tr("Settled"));
         item->setForeground(QColor{0x22, 0xC0, 0x64});
         break;
      case bs::network::Order::Failed:
         item->setText(tr("Failed"));
         item->setForeground(QColor{0xEC, 0x0A, 0x35});
         break;
      default:
         item->setText(tr("Unknown"));
         break;
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

std::pair<QStandardItem *, int> OrderListModel::findItem(const bs::network::Order &order)
{
   const auto statusGrp = getStatusGroup(order);
   const auto assetGrpName = tr(bs::network::Asset::toString(order.assetType));
   const auto statusGI = findItems(StatusGroup::toString(statusGrp));
   if (statusGI.isEmpty()) {
      return { nullptr, -1 };
   }
   const auto itGroups = groups_.find(order.exchOrderId.toStdString());
   if ((itGroups != groups_.end()) && (itGroups->second != statusGrp)) {
      const auto oldStatusGI = findItems(StatusGroup::toString(itGroups->second));
      if (!oldStatusGI.isEmpty()) {
         for (int i = 0; i < oldStatusGI.first()->rowCount(); i++) {
            auto group = oldStatusGI.first()->child(i, 0);
            if (group->text() == assetGrpName) {
               for (int j = 0; j < group->rowCount(); j++) {
                  if (group->child(j, 0)->data() == order.exchOrderId) {
                     group->removeRow(j);
                     break;
                  }
               }
               if (!group->rowCount()) {
                  oldStatusGI.first()->removeRow(i);
               }
               break;
            }
         }
      }
   }
   QStandardItem *group = nullptr;
   for (int i = 0; i < statusGI.first()->rowCount(); i++) {
      group = statusGI.first()->child(i, 0);
      if (group->text() == assetGrpName) {
         break;
      }
      else {
         group = nullptr;
      }
   }
   if (!group) {
      group = new QStandardItem(assetGrpName);
      QFont font;
      font.setBold(true);
      group->setData(font, Qt::FontRole);
      statusGI.first()->appendRow(QList<QStandardItem*>() << group);
   }

   int index = -1;
   for (int i = 0; i < group->rowCount(); i++) {
      if (group->child(i, 0)->data() == order.exchOrderId) {
         index = i;
         break;
      }
   }
   if (index < 0) {
      groups_[order.exchOrderId.toStdString()] = statusGrp;
   }
   return { group, index };
}

void OrderListModel::onOrderUpdated(const bs::network::Order& order)
{
   const auto found = findItem(order);
   auto groupItem = found.first;
   const auto index = found.second;

   if (index < 0) {
      QList<QStandardItem*> items;

      auto siTime = new QStandardItem(UiUtils::displayTimeMs(order.dateTime));
      siTime->setData(order.exchOrderId);
      items << siTime;

      const auto product = QString::fromStdString(order.product);
      items << new QStandardItem(QString::fromStdString(order.security));
      items << new QStandardItem(product);
      items << new QStandardItem(tr(bs::network::Side::toString(order.side)));

      double value = order.quantity * order.price;
      if (order.security.substr(0, order.security.find('/')) != order.product) {
         value = order.quantity / order.price;
      }

      auto siQuantity = new QStandardItem(UiUtils::displayQty(order.quantity, order.security, order.product, order.assetType));
      siQuantity->setTextAlignment(Qt::AlignRight);
      items << siQuantity;

      const auto priceAssetType = assetManager_->GetAssetTypeForSecurity(order.security);
      auto siPrice = new QStandardItem(UiUtils::displayPriceForAssetType(order.price, priceAssetType));
      siPrice->setTextAlignment(Qt::AlignRight);

      const QString sValue = UiUtils::displayValue(value, order.security, order.product, order.assetType);
      auto siValue = new QStandardItem(sValue);
      siValue->setTextAlignment(Qt::AlignRight);

      items << siPrice << siValue << new QStandardItem();

      setOrderStatus(items.last(), order);

      groupItem->insertRow(0, items);
   }
   else {
      setOrderStatus(groupItem->child(index, Header::Status), order);
   }
}

#include "MarketDataModel.h"
#include "CommonTypes.h"
#include "Colors.h"
#include <QLocale>

#include "UiUtils.h"

MarketDataModel::MarketDataModel(const QStringList &showSettings, QObject* parent)
   : QStandardItemModel(parent)
{
   QStringList headerLabels;
   for (int col = static_cast<int>(MarketDataColumns::First); col < static_cast<int>(MarketDataColumns::ColumnsCount); col++) {
      headerLabels << columnName(static_cast<MarketDataColumns>(col));
   }
   setHorizontalHeaderLabels(headerLabels);

   for (int col = static_cast<int>(MarketDataColumns::First) + 1; col < static_cast<int>(MarketDataColumns::ColumnsCount); col++) {
      horizontalHeaderItem(col)->setTextAlignment(Qt::AlignCenter);
   }

   for (const auto &setting : showSettings) {
      instrVisible_.insert(setting);
   }

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &MarketDataModel::ticker);
   timer_.start();
}

QString MarketDataModel::columnName(MarketDataColumns col) const
{
   switch (col)
   {
   case MarketDataColumns::Product:    return tr("Security");
   case MarketDataColumns::BidPrice:   return tr("Bid");
   case MarketDataColumns::OfferPrice: return tr("Ask");
   case MarketDataColumns::LastPrice:  return tr("Last");
   case MarketDataColumns::DailyVol:   return tr("24h Volume");
   case MarketDataColumns::EmptyColumn: return QString();
   default:          return tr("Unknown");
   }
}

static double toDoubleFromPriceStr(const QString &priceStr)
{
   if (priceStr.isEmpty()) {
      return std::numeric_limits<double>::infinity();
   }
   bool ok;
   double rv = priceStr.toDouble(&ok);
   if (!ok) {
      rv = QLocale().toDouble(priceStr, &ok);
   }
   if (!ok) {
      return std::numeric_limits<double>::infinity();
   }
   return rv;
}

QToggleItem* priceItem(const QString &price)
{
   auto item = new QToggleItem(price);
   item->setData(Qt::AlignRight, Qt::TextAlignmentRole);
   return item;
}

QToggleItem *MarketDataModel::getGroup(bs::network::Asset::Type assetType)
{
   QString productGroup;
   if (assetType == bs::network::Asset::Undefined) {
      productGroup = tr("Rejected");
   }
   else {
      productGroup = tr(bs::network::Asset::toString(assetType));
   }
   auto groupItems = findItems(productGroup);
   QToggleItem* groupItem = nullptr;
   if (groupItems.isEmpty()) {
      groupItem = new QToggleItem(productGroup, isVisible(productGroup));
      groupItem->setData(-1);
      appendRow(QList<QStandardItem*>() << groupItem);
   }
   else {
      groupItem = static_cast<QToggleItem *>(groupItems.first());
   }
   return groupItem;
}

struct PriceItem {
   QString  str;
   double   value;
};
using PriceMap = std::map<MarketDataModel::MarketDataColumns, PriceItem>;

static QString getVolumeString(double value, bs::network::Asset::Type at)
{
   switch(at) {
   case bs::network::Asset::SpotFX:
      return UiUtils::displayCurrencyAmount(value);
   case bs::network::Asset::SpotXBT:
      return UiUtils::displayAmount(value);
   case bs::network::Asset::PrivateMarket:
      return UiUtils::displayCCAmount(value);
   }

   return QString();
}

static void FieldsToMap(bs::network::Asset::Type at, const bs::network::MDFields &fields, PriceMap &map)
{
   for (const auto &field : fields) {
      switch (field.type) {
      case bs::network::MDField::PriceBid:
         map[MarketDataModel::MarketDataColumns::BidPrice] = { UiUtils::displayPriceForAssetType(field.value, at), UiUtils::truncatePriceForAsset(field.value, at) };
         break;
      case bs::network::MDField::PriceOffer:
         map[MarketDataModel::MarketDataColumns::OfferPrice] = { UiUtils::displayPriceForAssetType(field.value, at), UiUtils::truncatePriceForAsset(field.value, at) };
         break;
      case bs::network::MDField::PriceLast:
         map[MarketDataModel::MarketDataColumns::LastPrice] = { UiUtils::displayPriceForAssetType(field.value, at), UiUtils::truncatePriceForAsset(field.value, at) };
         break;
      case bs::network::MDField::DailyVolume:
         map[MarketDataModel::MarketDataColumns::DailyVol] = { getVolumeString(field.value, at), field.value };
         break;
      case bs::network::MDField::Reject:
         map[MarketDataModel::MarketDataColumns::ColumnsCount] = { field.desc, 0 };
         break;
      default:  break;
      }
   }
}

bool MarketDataModel::isVisible(const QString &id) const
{
   if (instrVisible_.empty()) {
      return true;
   }
   const auto itVisible = instrVisible_.find(id);
   if (itVisible != instrVisible_.end()) {
      return true;
   }
   return false;
}

void MarketDataModel::onMDUpdated(bs::network::Asset::Type assetType, const QString &security, bs::network::MDFields mdFields)
{
   if ((assetType == bs::network::Asset::Undefined) && security.isEmpty()) {  // Celer disconnected
      priceUpdates_.clear();
      removeRows(0, rowCount());
      return;
   }

   PriceMap fieldsMap;
   FieldsToMap(assetType, mdFields, fieldsMap);
   auto groupItem = getGroup(assetType);
   auto childRow = groupItem->findRowWithText(security);
   const auto timeNow = QDateTime::currentDateTime();
   if (!childRow.empty()) {
      for (const auto &price : fieldsMap) {
         childRow[static_cast<int>(price.first)]->setText(price.second.str);
         childRow[static_cast<int>(price.first)]->setBackground(bgColorForCol(security, price.first, price.second.value, timeNow, childRow));
      }
      return;
   }

   // If we reach here, the product wasn't found, so we make a new row for it
   QToggleItem::QToggleRow items;
   if (assetType == bs::network::Asset::Type::Undefined) {
      const auto rejItem = new QToggleItem(fieldsMap[MarketDataColumns::ColumnsCount].str);
      rejItem->setForeground(Qt::red);
      items << rejItem;
   }
   else {
      items << new QToggleItem(security, isVisible(security) | groupItem->isVisible());
      for (int col = static_cast<int>(MarketDataColumns::First) + 1; col < static_cast<int>(MarketDataColumns::ColumnsCount); col++) {
         const auto price = fieldsMap[static_cast<MarketDataColumns>(col)];
         auto item = priceItem(price.str);
         items << item;
      }
   }
   groupItem->addRow(items);
}

QBrush MarketDataModel::bgColorForCol(const QString &security, MarketDataModel::MarketDataColumns col, double price
   , const QDateTime &updTime, const QList<QToggleItem *> &row)
{
   switch (col) {
   case MarketDataModel::MarketDataColumns::BidPrice:
   case MarketDataModel::MarketDataColumns::OfferPrice:
   case MarketDataModel::MarketDataColumns::LastPrice:
   {
      const auto prev = priceUpdates_[security][col].price;
      priceUpdates_[security][col] = { price, updTime, {} };
      if (!qFuzzyIsNull(prev)) {
         if (price > prev) {
            priceUpdates_[security][col].row = row;
            return c_greenColor;
         }
         else if (price < prev) {
            priceUpdates_[security][col].row = row;
            return c_redColor;
         }
      }
      break;
   }
   default:    break;
   }
   return {};
}

QStringList MarketDataModel::getVisibilitySettings() const
{
   QStringList rv;
   for (int i = 0; i < rowCount(); i++) {
      const auto toggleGrp = static_cast<QToggleItem *>(item(i));
      if (toggleGrp == nullptr) {
         continue;
      }
      if (toggleGrp->isVisible()) {
         rv << toggleGrp->text();
         continue;
      }
      for (int i = 0; i < toggleGrp->rowCount(); i++) {
         const auto child = static_cast<QToggleItem *>(toggleGrp->child(i, 0));
         if (child == nullptr) {
            continue;
         }
         if (child->isVisible()) {
            rv << child->text();
         }
      }
   }
   return rv;
}

void MarketDataModel::onVisibilityToggled(bool filtered)
{
   for (int i = 0; i < rowCount(); i++) {
      auto toggleGrp = static_cast<QToggleItem*>(item(i));
      if (toggleGrp == nullptr) {
         continue;
      }
      toggleGrp->showCheckBox(!filtered);
   }
   emit needResize();
}

void MarketDataModel::ticker()
{
   const auto timeNow = QDateTime::currentDateTime();
   for (const auto &priceUpd : priceUpdates_) {
      for (auto price : priceUpd.second) {
         if (price.second.row.empty()) {
            continue;
         }
         if (price.second.updated.msecsTo(timeNow) > 3000) {
            price.second.row[static_cast<int>(price.first)]->setBackground(QBrush());
            price.second.row.clear();
         }
      }
   }
}


QToggleItem::QToggleRow QToggleItem::findRowWithText(const QString &text, int column)
{
   for (int i = 0; i < rowCount(); i++) {
      if (child(i, column)->text() == text) {
         QToggleRow rv;
         for (int j = 0; j < columnCount(); j++) {
            rv << static_cast<QToggleItem*>(child(i, j));
         }
         return rv;
      }
   }
   for (const auto invChild : invisibleChildren_) {
      if (invChild.at(column)->text() == text) {
         return invChild;
      }
   }
   return {};
}

void QToggleItem::showCheckBox(bool state, int column)
{
   if (state) {
      setCheckState(isVisible_ ? Qt::Checked : Qt::Unchecked);
      for (const auto &row : invisibleChildren_) {
         appendRow(row);
      }
   }

   setCheckable(state);
   for (int i = 0; i < rowCount(); i++) {
      auto tgChild = static_cast<QToggleItem*>(child(i, column));
      tgChild->showCheckBox(state, column);
   }

   if (!state) {
      setData(QVariant(), Qt::CheckStateRole);

      QList<QList<QStandardItem*> > takenRows;
      int i = 0;
      while (rowCount() > 0) {
         if (i >= rowCount()) {
            break;
         }
         auto tgChild = static_cast<QToggleItem*>(child(i, column));
         if (tgChild->isVisible()) {
            i++;
         }
         else {
            takenRows << takeRow(i);
         }
      }
      invisibleChildren_.clear();
      for (const auto row : takenRows) {
         QToggleRow tRow;
         for (const auto item : row) {
            tRow << static_cast<QToggleItem *>(item);
         }
         invisibleChildren_ << tRow;
      }
   }
}

void QToggleItem::setData(const QVariant &value, int role)
{
   QStandardItem::setData(value, role);

   if ((role == Qt::CheckStateRole) && value.isValid()) {
      const auto state = static_cast<Qt::CheckState>(value.toInt());
      setVisible(state == Qt::Checked);
      auto tParent = dynamic_cast<QToggleItem *>(parent());
      if (tParent != nullptr) {
         tParent->updateCheckMark();
      } else {
         for (int  i=0; i < rowCount(); i++) {
            auto childItem = dynamic_cast<QToggleItem *>(child(i, 0));
            childItem->setVisible(state == Qt::Checked);
            childItem->QStandardItem::setData(state, Qt::CheckStateRole);
         }
      }
   }
}

void QToggleItem::updateCheckMark(int column)
{
   int nbVisibleChildren = 0;
   for (int i = 0; i < rowCount(); i++) {
      const auto tgChild = static_cast<QToggleItem*>(child(i, column));
      if (tgChild->isVisible()) {
         nbVisibleChildren++;
      }
   }
   if (!nbVisibleChildren) {
      QStandardItem::setData(Qt::Unchecked, Qt::CheckStateRole);
   }
   else {
      setVisible(nbVisibleChildren != 0);
      QStandardItem::setData((nbVisibleChildren == rowCount()) ? Qt::Checked : Qt::PartiallyChecked, Qt::CheckStateRole);
   }
}

void QToggleItem::addRow(const QToggleItem::QToggleRow &row, int visColumn)
{
   if (row[visColumn]->isVisible()) {
      appendRow(row);
   }
   else {
      invisibleChildren_ << row;
   }
}

void QToggleItem::appendRow(const QToggleItem::QToggleRow &row)
{
   QList<QStandardItem *> list;
   for (const auto &item : row) {
      list << item;
   }
   QStandardItem::appendRow(list);
}


MDSortFilterProxyModel::MDSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{ }

bool MDSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   QVariant leftData = sourceModel()->data(left);
   QVariant rightData = sourceModel()->data(right);

   static const std::map<QString, int> groups = {
      {tr(bs::network::Asset::toString(bs::network::Asset::PrivateMarket)), 0},
      {tr(bs::network::Asset::toString(bs::network::Asset::SpotXBT)), 1},
      {tr(bs::network::Asset::toString(bs::network::Asset::SpotFX)), 2},
   };

   if (!left.parent().isValid() && !right.parent().isValid()) {
      try {
         return (groups.at(leftData.toString()) < groups.at(rightData.toString()));
      } catch (const std::out_of_range &) {
         return true;
      }
   }

   if ((leftData.type() == QVariant::String) && (rightData.type() == QVariant::String)) {
      if ((left.column() > 0) && (right.column() > 0)) {
         double priceLeft = toDoubleFromPriceStr(leftData.toString());
         double priceRight = toDoubleFromPriceStr(rightData.toString());

         if ((priceLeft > 0) && (priceRight > 0))
            return (priceLeft < priceRight);
      }
      return (leftData.toString() < rightData.toString());
   }
   return (leftData < rightData);
}

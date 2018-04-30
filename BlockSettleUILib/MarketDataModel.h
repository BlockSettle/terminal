#ifndef __MARKET_DATA_MODEL_H__
#define __MARKET_DATA_MODEL_H__

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <QBrush>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include "CommonTypes.h"


class QToggleItem : public QStandardItem
{
public:
   QToggleItem(const QString &text, bool visible = true)
      : QStandardItem(text), isVisible_(visible) {}

   typedef QList<QToggleItem *>  QToggleRow;
   QToggleRow findRowWithText(const QString &text, int column = 0);

   void addRow(const QToggleRow &, int visColumn = 0);
   void setVisible(bool visible) { isVisible_ = visible; }
   bool isVisible() const { return isVisible_; }
   void showCheckBox(bool state, int column = 0);
   void updateCheckMark(int column = 0);

   void setData(const QVariant &value, int role = Qt::UserRole + 1) override;

private:
   bool              isVisible_ = true;
   QList<QToggleRow> invisibleChildren_;

   void appendRow(const QToggleRow &);
};

class MarketDataModel : public QStandardItemModel
{
Q_OBJECT
public:
   MarketDataModel(const QStringList &showSettings = {}, QObject *parent = nullptr);
   ~MarketDataModel() noexcept = default;

   MarketDataModel(const MarketDataModel&) = delete;
   MarketDataModel& operator = (const MarketDataModel&) = delete;
   MarketDataModel(MarketDataModel&&) = delete;
   MarketDataModel& operator = (MarketDataModel&&) = delete;

   QStringList getVisibilitySettings() const;

public slots:
   void onMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void onVisibilityToggled(bool filtered);

signals:
   void needResize();

private slots:
   void ticker();

public:
   enum class MarketDataColumns : int
   {
      First,
      Product = First,
      BidPrice,
      OfferPrice,
      LastPrice,
      DailyVol,
      EmptyColumn,
      ColumnsCount
   };

private:
   struct PriceUpdate {
      double      price;
      QDateTime   updated;
      QList<QToggleItem *> row;
   };
   typedef std::map<MarketDataModel::MarketDataColumns, PriceUpdate> PriceByCol;
   typedef std::map<QString, PriceByCol>     PriceUpdates;

   std::set<QString>    instrVisible_;
   PriceUpdates         priceUpdates_;
   QTimer               timer_;

private:
   QToggleItem *getGroup(bs::network::Asset::Type);
   QString columnName(MarketDataColumns) const;
   bool isVisible(const QString &id) const;
   QBrush bgColorForCol(const QString &security, MarketDataModel::MarketDataColumns, double price
      , const QDateTime &, const QList<QToggleItem *> &row);
};


class MDSortFilterProxyModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit MDSortFilterProxyModel(QObject *parent = nullptr);

protected:
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

#endif // __MARKET_DATA_MODEL_H__

#ifndef __MARKET_DATA_WIDGET_H__
#define __MARKET_DATA_WIDGET_H__

#include <memory>
#include <QWidget>
#include "ApplicationSettings.h"


namespace Ui {
    class MarketDataWidget;
};

class MarketDataModel;
class MDSortFilterProxyModel;
class MarketDataProvider;
class ApplicationSettings;
class MDHeader;


class MarketDataWidget : public QWidget
{
Q_OBJECT

public:
   MarketDataWidget(QWidget* parent = nullptr );
   virtual ~MarketDataWidget();

   void init(const std::shared_ptr<ApplicationSettings> &appSettings, ApplicationSettings::Setting paramVis
      , const std::shared_ptr<MarketDataProvider>& mdProvider);

signals:
   void CurrencySelected(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void SellClicked(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void BuyClicked(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);

private slots:
   void resizeAndExpand();
   void onHeaderStateChanged(bool state);
   void onRowClicked(const QModelIndex& index);
   void onMDRejected(const std::string &security, const std::string &reason);

private:
   Ui::MarketDataWidget    *              ui;
   MarketDataModel         *              marketDataModel_;
   MDSortFilterProxyModel  *              mdSortFilterModel_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   ApplicationSettings::Setting           settingVisibility_;
   std::shared_ptr<MDHeader>              mdHeader_;
   bool  filteredView_ = true;
};

#include <QPainter>
#include <QHeaderView>
#include <QStyleOptionButton>
#include <QMouseEvent>

class MDHeader : public QHeaderView
{
   Q_OBJECT
public:
   MDHeader(Qt::Orientation orient, QWidget *parent = nullptr) : QHeaderView(orient, parent) {}

protected:
   void paintSection(QPainter *painter, const QRect &rect, int logIndex) const {
      painter->save();
      QHeaderView::paintSection(painter, rect, logIndex);
      painter->restore();
      if (logIndex == 0) {
         QStyleOptionButton option;
         option.rect = QRect(2, 2, height() - 4, height() - 4);
         option.state = QStyle::State_Enabled;
         option.state |= state_ ? QStyle::State_On : QStyle::State_Off;
         style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter);
      }
   }

   void mousePressEvent(QMouseEvent *event) {
      if (QRect(0, 0, height(), height()).contains(event->x(), event->y())) {
         state_ = !state_;
         emit stateChanged(state_);
         update();
      }
      else {
         QHeaderView::mousePressEvent(event);
      }
   }

private:
   bool state_ = true;

signals:
   void stateChanged(bool);
};


#endif // __MARKET_DATA_WIDGET_H__

/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __MARKET_DATA_WIDGET_H__
#define __MARKET_DATA_WIDGET_H__

#include <memory>
#include <QWidget>
#include <QItemSelection>
#include "ApplicationSettings.h"


namespace Ui {
    class MarketDataWidget;
};

class ApplicationSettings;
class MarketDataModel;
class MarketDataProvider;
class MDCallbacksQt;
class MDSortFilterProxyModel;
class MDHeader;
class TreeViewWithEnterKey;

struct MarketSelectedInfo {
   QString productGroup_;
   QString currencyPair_;
   QString bidPrice_;
   QString offerPrice_;

   bool isValid() const;
};

Q_DECLARE_METATYPE(MarketSelectedInfo);

class MarketDataWidget : public QWidget
{
Q_OBJECT

public:
   MarketDataWidget(QWidget* parent = nullptr );
   ~MarketDataWidget() override;

   void init(const std::shared_ptr<ApplicationSettings> &appSettings, ApplicationSettings::Setting paramVis
      , const std::shared_ptr<MarketDataProvider> &, const std::shared_ptr<MDCallbacksQt> &);

   TreeViewWithEnterKey* view() const;

   void setAuthorized(bool authorized);
   MarketSelectedInfo getCurrentlySelectedInfo() const;

signals:
   void CurrencySelected(const MarketSelectedInfo& selectedInfo);
   void AskClicked(const MarketSelectedInfo& selectedInfo);
   void BidClicked(const MarketSelectedInfo& selectedInfo);
   void MDHeaderClicked();
   void clicked();

private slots:
   void resizeAndExpand();
   void onHeaderStateChanged(bool state);
   void onRowClicked(const QModelIndex& index);
   void onSelectionChanged(const QModelIndex &, const QModelIndex &);
   void onMDRejected(const std::string &security, const std::string &reason);

   void onLoadingNetworkSettings();

   void OnMDConnecting();
   void OnMDConnected();
   void OnMDDisconnecting();
   void OnMDDisconnected();

   void ChangeMDSubscriptionState();

protected:
   MarketSelectedInfo getRowInfo(const QModelIndex& index) const;

private:
   std::unique_ptr<Ui::MarketDataWidget> ui_;
   MarketDataModel         *              marketDataModel_;
   MDSortFilterProxyModel  *              mdSortFilterModel_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   ApplicationSettings::Setting           settingVisibility_;
   std::shared_ptr<MDHeader>              mdHeader_;
   bool  filteredView_ = true;
   std::shared_ptr<MarketDataProvider>    mdProvider_;
   bool authorized_{ false };
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
   void paintSection(QPainter *painter, const QRect &rect, int logIndex) const override {
      painter->save();
      QHeaderView::paintSection(painter, rect, logIndex);
      painter->restore();

      if (logIndex == 0) { 
         QStyleOptionButton option;
         const QSize ch = checkboxSizeHint();
         option.rect = QRect(2, (height() - ch.height()) / 2, ch.width(), ch.height());
         option.state = QStyle::State_Enabled;
         option.state |= state_ ? QStyle::State_On : QStyle::State_Off;

         style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter);
      }
   }

   QSize sectionSizeFromContents(int logicalIndex) const override
   {
      if (logicalIndex == 0) {
         const QSize orig = QHeaderView::sectionSizeFromContents(logicalIndex);
         const QSize checkbox = checkboxSizeHint();

         return QSize(orig.width() + checkbox.width() + 4,
                      qMax(orig.height(), checkbox.height() + 4));
      } else {
         return QHeaderView::sectionSizeFromContents(logicalIndex);
      }
   }

   void mousePressEvent(QMouseEvent *event) override {
      if (QRect(0, 0, checkboxSizeHint().width() + 4, height()).contains(event->x(), event->y())) {
         state_ = !state_;
         emit stateChanged(state_);
         update();
      }
      else {
         QHeaderView::mousePressEvent(event);
      }
   }

private:
   QSize checkboxSizeHint() const
   {
      QStyleOptionButton opt;
      return style()->subElementRect(QStyle::SE_CheckBoxIndicator, &opt).size();
   }

private:
   bool state_ = true;

signals:
   void stateChanged(bool);
};


#endif // __MARKET_DATA_WIDGET_H__

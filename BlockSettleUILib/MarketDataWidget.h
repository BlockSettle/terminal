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
class TreeViewWithEnterKey;

struct MarkeSelectedInfo {
   QString productGroup_;
   QString currencyPair_;
   QString bidPrice_;
   QString offerPrice_;

   bool isValid() const;
};

Q_DECLARE_METATYPE(MarkeSelectedInfo);

class MarketDataWidget : public QWidget
{
Q_OBJECT

public:
   MarketDataWidget(QWidget* parent = nullptr );
   ~MarketDataWidget() override;

   void init(const std::shared_ptr<ApplicationSettings> &appSettings, ApplicationSettings::Setting paramVis
      , const std::shared_ptr<MarketDataProvider>& mdProvider);

   TreeViewWithEnterKey* view() const;

   void setAuthorized(bool authorized);
   MarkeSelectedInfo getCurrentlySelectedInfo() const;

signals:
   void CurrencySelected(const MarkeSelectedInfo& selectedInfo);
   void SellClicked(const MarkeSelectedInfo& selectedInfo);
   void BuyClicked(const MarkeSelectedInfo& selectedInfo);
   void MDHeaderClicked();

private slots:
   void resizeAndExpand();
   void onHeaderStateChanged(bool state);
   void onRowClicked(const QModelIndex& index);
   void onMDRejected(const std::string &security, const std::string &reason);
   void onEnterKeyPressed(const QModelIndex &index);

   void onLoadingNetworkSettings();

   void OnMDConnecting();
   void OnMDConnected();
   void OnMDDisconnecting();
   void OnMDDisconnected();

   void ChangeMDSubscriptionState();

protected:
   MarkeSelectedInfo getRowInfo(const QModelIndex& index) const;

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

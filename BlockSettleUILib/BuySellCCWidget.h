#ifndef __BUY_SELL_CC_WIDGET_H__
#define __BUY_SELL_CC_WIDGET_H__

#include <QWidget>

namespace Ui {
    class BuySellCCWidget;
};


class BuySellCCWidget : public QWidget
{
Q_OBJECT

public:
   BuySellCCWidget(QWidget* parent = nullptr );
   virtual ~BuySellCCWidget();

   void resetView(bool sell);
private:
   Ui::BuySellCCWidget* ui_;
};

#endif // __BUY_SELL_CC_WIDGET_H__
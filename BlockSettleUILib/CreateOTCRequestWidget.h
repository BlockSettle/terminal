#ifndef __CREATE_OTC_REQUEST_WIDGET_H__
#define __CREATE_OTC_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

#include "OtcTypes.h"

namespace Ui {
   class CreateOTCRequestWidget;
};


class CreateOTCRequestWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCRequestWidget(QWidget* parent = nullptr);
   ~CreateOTCRequestWidget() override;

private slots:
   void onSellClicked();
   void onBuyClicked();

private:
   std::unique_ptr<Ui::CreateOTCRequestWidget> ui_;
};

#endif // __CREATE_OTC_REQUEST_WIDGET_H__

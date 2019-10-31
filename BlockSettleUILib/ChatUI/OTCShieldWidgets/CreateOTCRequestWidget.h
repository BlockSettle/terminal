#ifndef __CREATE_OTC_REQUEST_WIDGET_H__
#define __CREATE_OTC_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

#include "OtcTypes.h"
#include "OTCWindowsAdapterBase.h"

namespace Ui {
   class CreateOTCRequestWidget;
};

class CreateOTCRequestWidget : public OTCWindowsAdapterBase
{
   Q_OBJECT

public:
   CreateOTCRequestWidget(QWidget* parent = nullptr);
   ~CreateOTCRequestWidget() override;

   void init(bs::network::otc::Env env);

   bs::network::otc::QuoteRequest request() const;

signals:
   void requestCreated();

protected slots:
   void onUpdateBalances() override;

private slots:
   void onSellClicked();
   void onBuyClicked();
   void onNumCcySelected();

private:
   void updateXBTRange(bool isSell, BTCNumericTypes::balance_type balance = 0.0);

   std::unique_ptr<Ui::CreateOTCRequestWidget> ui_;

   QString buyProduct_{ QLatin1String("EUR") };
   int env_{};
};

#endif // __CREATE_OTC_REQUEST_WIDGET_H__

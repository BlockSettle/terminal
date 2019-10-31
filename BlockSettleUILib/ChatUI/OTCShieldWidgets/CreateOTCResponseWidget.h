#ifndef __CREATE_OTC_RESPONSE_WIDGET_H__
#define __CREATE_OTC_RESPONSE_WIDGET_H__

#include <memory>

#include "OTCWindowsAdapterBase.h"
#include "OtcTypes.h"

namespace Ui {
   class CreateOTCResponseWidget;
};

class CreateOTCResponseWidget : public OTCWindowsAdapterBase
{
   Q_OBJECT
public:
   CreateOTCResponseWidget(QWidget* parent = nullptr);
   ~CreateOTCResponseWidget() override;

   void setRequest(const bs::network::otc::QuoteRequest &request);

   bs::network::otc::QuoteResponse response() const;
   void setPeer(const bs::network::otc::Peer &peer) override;

protected slots:
   void onUpdateBalances() override;

private slots:
   void updateAcceptButton();

signals:
   void responseCreated();

private:
   std::unique_ptr<Ui::CreateOTCResponseWidget> ui_;

   bs::network::otc::Side ourSide_{};
   QString buyProduct_{ QLatin1String("EUR") };
};

#endif // __CREATE_OTC_RESPONSE_WIDGET_H__

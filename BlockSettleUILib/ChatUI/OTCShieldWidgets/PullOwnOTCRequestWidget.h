#ifndef __PULL_OWN_OTC_REQUEST_WIDGET_H__
#define __PULL_OWN_OTC_REQUEST_WIDGET_H__

#include <memory>
#include <QWidget>

#include "OtcTypes.h"
#include "OTCWindowsAdapterBase.h"

namespace Ui {
    class PullOwnOTCRequestWidget;
};

class PullOwnOTCRequestWidget : public OTCWindowsAdapterBase
{
Q_OBJECT

public:
   explicit PullOwnOTCRequestWidget(QWidget* parent = nullptr);
   ~PullOwnOTCRequestWidget() override;

   void setOffer(const bs::network::otc::Offer &offer);
   void setRequest(const bs::network::otc::QuoteRequest &request);
   void setResponse(const bs::network::otc::QuoteResponse &response);

signals:
   void requestPulled();

private:
   std::unique_ptr<Ui::PullOwnOTCRequestWidget> ui_;

};

#endif // __PULL_OWN_OTC_REQUEST_WIDGET_H__

#ifndef __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
#define __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

#include <memory>
#include <QWidget>

#include "OtcTypes.h"
#include "OTCWindowsAdapterBase.h"

namespace Ui {
   class OTCNegotiationCommonWidget;
};

class OTCNegotiationResponseWidget : public OTCWindowsAdapterBase
{
Q_OBJECT
Q_DISABLE_COPY(OTCNegotiationResponseWidget)

public:
   explicit OTCNegotiationResponseWidget(QWidget* parent = nullptr);
   ~OTCNegotiationResponseWidget() override;

   void setOffer(const bs::network::otc::Offer &offer);

   bs::network::otc::Offer offer() const;

signals:
   void responseAccepted();
   void responseUpdated();
   void responseRejected();

protected:
   void syncInterface() override;

private slots:
   void onChanged();
   void onAcceptOrUpdateClicked();

   void onCurrentWalletChanged();

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
   bs::network::otc::Offer receivedOffer_;
};

#endif // __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

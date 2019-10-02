#ifndef __OTC_NEGOTIATION_RESPONSE_WIDGET_H__
#define __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

#include <memory>
#include <QWidget>

#include "OtcTypes.h"
#include "OTCWindowsAdapterBase.h"
#include "CommonTypes.h"

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
   void setPeer(const bs::network::otc::Peer &peer) override;

signals:
   void responseAccepted();
   void responseUpdated();
   void responseRejected();

public slots:
   void onAboutToApply() override;

protected slots:
   void onSyncInterface() override;
   void onMDUpdated() override;
   void onUpdateBalances() override;

private slots:
   void onChanged();
   void onAcceptOrUpdateClicked();
   void onUpdateIndicativePrice();
   void onShowXBTInputsClicked();
   void onXbtInputsProcessed();

   void onCurrentWalletChanged();

protected:
   void updateIndicativePriceValue();

private:
   std::unique_ptr<Ui::OTCNegotiationCommonWidget> ui_;
   bs::network::otc::Offer receivedOffer_;
};

#endif // __OTC_NEGOTIATION_RESPONSE_WIDGET_H__

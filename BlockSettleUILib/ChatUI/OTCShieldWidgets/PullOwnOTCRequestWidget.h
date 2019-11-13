#ifndef __PULL_OWN_OTC_REQUEST_WIDGET_H__
#define __PULL_OWN_OTC_REQUEST_WIDGET_H__

#include <memory>
#include <QWidget>
#include <QTimer>
#include <QDateTime>

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
   void setPendingBuyerSign(const bs::network::otc::Offer &offer);
   void setPendingSellerSign(const bs::network::otc::Offer &offer);

   void setPeer(const bs::network::otc::Peer &peer) override;

signals:
   void currentRequestPulled();

   void saveOfflineClicked();
   void loadOfflineClicked();
   void broadcastOfflineClicked();

protected:
   void setupNegotiationInterface(const QString& headerText, bool isResponse = false);
   void setupSignAwaitingInterface(const QString& headerText);
   void setupOfferInfo(const bs::network::otc::Offer &offer);

private:
   std::unique_ptr<Ui::PullOwnOTCRequestWidget> ui_;
   bs::network::otc::Side ourSide_ = bs::network::otc::Side::Unknown;
};

#endif // __PULL_OWN_OTC_REQUEST_WIDGET_H__

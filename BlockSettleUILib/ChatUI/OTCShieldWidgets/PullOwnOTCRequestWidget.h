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

   void setOffer(const std::string& contactId, const bs::network::otc::Offer &offer);
   void setRequest(const std::string& contactId, const bs::network::otc::QuoteRequest &request);
   void setResponse(const std::string& contactId, const bs::network::otc::QuoteResponse &response);
   void setPendingBuyerSign(const bs::network::otc::Offer &offer);
   void setPendingSellerSign(const bs::network::otc::Offer &offer);

   void registerOTCUpdatedTime(const bs::network::otc::Peer* peer, QDateTime timestamp);

signals:
   void currentRequestPulled();
   void requestPulled(const std::string& contactId, bs::network::otc::PeerType peerType);

public slots:
   void onLogout();

protected slots:
   void onUpdateTimerData();

protected:
   void setupTimer(const std::string& contactId);
   void setupNegotiationInterface(const QString& headerText);
   void setupSignAwaitingInterface(const QString& headerText);
   void setupOfferInfo(const bs::network::otc::Offer &offer);

private:
   std::unique_ptr<Ui::PullOwnOTCRequestWidget> ui_;

   QTimer pullTimer_;
   QDateTime currentOfferEndTimestamp_;

   struct PeerData
   {
      QDateTime arrivedTime_;
      bs::network::otc::PeerType peerType_;
   };
   std::unordered_map<std::string, PeerData> timestamps_;
};

#endif // __PULL_OWN_OTC_REQUEST_WIDGET_H__

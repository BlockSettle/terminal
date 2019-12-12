/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __REQUESTING_QUOTE_WIDGET_H__
#define __REQUESTING_QUOTE_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <chrono>
#include "CommonTypes.h"


namespace Ui {
    class RequestingQuoteWidget;
}
class AssetManager;
class BaseCelerClient;

class RequestingQuoteWidget : public QWidget
{
Q_OBJECT

public:
   RequestingQuoteWidget(QWidget* parent = nullptr );
   ~RequestingQuoteWidget() override;

   void SetAssetManager(const std::shared_ptr<AssetManager> &assetManager) {
      assetManager_ = assetManager;
   }

   void SetCelerClient(std::shared_ptr<BaseCelerClient> celerClient);

   void populateDetails(const bs::network::RFQ& rfq);

public slots:
   void ticker();
   bool onQuoteReceived(const bs::network::Quote& quote);
   void onQuoteCancelled(const QString &reqId, bool byUser);
   void onOrderFilled(const std::string &quoteId);
   void onOrderFailed(const std::string& quoteId, const std::string& reason);
   void onReject(const QString &reqId, const QString &reason);
   void onCelerDisconnected();

signals:
   void cancelRFQ();
   void requestTimedOut();
   void quoteAccepted(const QString &reqId, const bs::network::Quote& quote);
   void quoteFinished();
   void quoteFailed();

private:
   enum Status {
      Indicative,
      Tradeable
   };

   enum QuoteDetailsState
   {
      Waiting,
      Rejected,
      Replied
   };

   void SetQuoteDetailsState(QuoteDetailsState state);

private:
   std::unique_ptr<Ui::RequestingQuoteWidget> ui_;
   QTimer                     requestTimer_;
   QDateTime                  timeoutReply_;
   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   std::shared_ptr<AssetManager> assetManager_;
   bool                       balanceOk_ = true;
   std::shared_ptr<BaseCelerClient>                    celerClient_;

private:
   void setupTimer(Status status, const QDateTime &expTime);
   void onCancel();
   void onAccept();
};

#endif // __REQUESTING_QUOTE_WIDGET_H__

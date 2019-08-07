#include "RFQDialog.h"
#include "ui_RFQDialog.h"

#include <spdlog/logger.h>

#include "AssetManager.h"
#include "CCSettlementTransactionWidget.h"
#include "QuoteProvider.h"
#include "ReqCCSettlementContainer.h"
#include "ReqXBTSettlementContainer.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "XBTSettlementTransactionWidget.h"

enum StackWidgetId
{
   RequestingQuoteId,
   SettlementTransactionId
};

RFQDialog::RFQDialog(const std::shared_ptr<spdlog::logger> &logger
   , const bs::network::RFQ& rfq
   , const std::shared_ptr<TransactionData>& transactionData
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<AuthAddressManager>& authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<BaseCelerClient> &celerClient
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const bs::Address &authAddr
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::RFQDialog())
   , logger_(logger)
   , rfq_(rfq)
   , transactionData_(transactionData)
   , quoteProvider_(quoteProvider)
   , authAddressManager_(authAddressManager)
   , walletsManager_(walletsManager)
   , signContainer_(signContainer)
   , assetMgr_(assetManager)
   , armory_(armory)
   , celerClient_(celerClient)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , authAddr_(authAddr)
{
   ui_->setupUi(this);

   ui_->pageRequestingQuote->SetAssetManager(assetMgr_);
   ui_->pageRequestingQuote->SetCelerClient(celerClient_);

   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::cancelRFQ, this, &RFQDialog::reject);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::requestTimedOut, this, &RFQDialog::close);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteAccepted, this, &RFQDialog::onRFQResponseAccepted, Qt::QueuedConnection);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFinished, this, &RFQDialog::close);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFailed, this, &RFQDialog::close);

   connect(quoteProvider_.get(), &QuoteProvider::quoteReceived, this, &RFQDialog::onQuoteReceived);
   connect(quoteProvider_.get(), &QuoteProvider::quoteRejected, ui_->pageRequestingQuote, &RequestingQuoteWidget::onReject);
   connect(quoteProvider_.get(), &QuoteProvider::orderRejected, ui_->pageRequestingQuote, &RequestingQuoteWidget::onReject);
   connect(quoteProvider_.get(), &QuoteProvider::quoteCancelled, ui_->pageRequestingQuote, &RequestingQuoteWidget::onQuoteCancelled);

   connect(quoteProvider_.get(), &QuoteProvider::orderFailed, this, &RFQDialog::onOrderFailed);
   connect(quoteProvider_.get(), &QuoteProvider::quoteOrderFilled, this, &RFQDialog::onOrderFilled);
   connect(quoteProvider_.get(), &QuoteProvider::signTxRequested, this, &RFQDialog::onSignTxRequested);

   if (rfq_.assetType == bs::network::Asset::SpotXBT) {
      connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQDialog::onOrderUpdated, Qt::QueuedConnection);
   }

   ui_->pageRequestingQuote->populateDetails(rfq_, transactionData_);

   quoteProvider_->SubmitRFQ(rfq_);

   ui_->stackedWidgetRFQ->setCurrentIndex(RequestingQuoteId);
}

RFQDialog::~RFQDialog() = default;

void RFQDialog::onOrderFilled(const std::string &quoteId)
{
   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      ui_->pageRequestingQuote->onOrderFilled(quoteId);
   }
}

void RFQDialog::onOrderFailed(const std::string& quoteId, const std::string& reason)
{
   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      ui_->pageRequestingQuote->onOrderFailed(quoteId, reason);
   }
}

void RFQDialog::onRFQResponseAccepted(const QString &reqId, const bs::network::Quote &quote)
{
   quote_ = quote;

   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      quoteProvider_->AcceptQuoteFX(reqId, quote);
   }
   else {
      if (rfq_.assetType == bs::network::Asset::SpotXBT) {
         curContainer_ = newXBTcontainer();
      }
      else {
         curContainer_ = newCCcontainer();
      }
      curContainer_->activate();
   }
}

std::shared_ptr<bs::SettlementContainer> RFQDialog::newXBTcontainer()
{
   xbtSettlContainer_ = std::make_shared<ReqXBTSettlementContainer>(logger_
      , authAddressManager_, assetMgr_, signContainer_, armory_, walletsManager_
      , rfq_, quote_, transactionData_, authAddr_);

   connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::settlementAccepted
      , this, &RFQDialog::onSettlementAccepted);
   connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::settlementCancelled
      , this, &QDialog::close);
   connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::acceptQuote
      , this, &RFQDialog::onXBTQuoteAccept);

//   const auto xbtSettlementWidget = new XBTSettlementTransactionWidget(logger_
//      , celerClient_, appSettings_, xbtSettlContainer_, connectionManager_, this);

//   auto settlementIndex = ui_->stackedWidgetRFQ->addWidget(xbtSettlementWidget);
//   ui_->stackedWidgetRFQ->setCurrentIndex(settlementIndex);

   return xbtSettlContainer_;
}

std::shared_ptr<bs::SettlementContainer> RFQDialog::newCCcontainer()
{
   ccSettlContainer_ = std::make_shared<ReqCCSettlementContainer>(logger_
      , signContainer_, armory_, assetMgr_, walletsManager_, rfq_, quote_, transactionData_);

   connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::settlementAccepted
      , this, &RFQDialog::onSettlementAccepted);
   connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::sendOrder
      , this, &RFQDialog::onSettlementOrder);
   connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::settlementCancelled
      , this, &QDialog::close);

//   const auto ccSettlementWidget = new CCSettlementTransactionWidget(logger_
//      , celerClient_, appSettings_, ccSettlContainer_, connectionManager_, this);

//   auto settlementIndex = ui_->stackedWidgetRFQ->addWidget(ccSettlementWidget);
//   ui_->stackedWidgetRFQ->setCurrentIndex(settlementIndex);

   return ccSettlContainer_;
}

void RFQDialog::reject()
{
   // curContainer_->cancel call could emit settlementCancelled which will result in RFQDialog::reject re-enter.
   // This will result in duplicated finished signals. Let's add a workaround for this.
   if (isRejectStarted_) {
      return;
   }
   isRejectStarted_ = true;

   if (cancelOnClose_ && curContainer_) {
      if (!curContainer_->cancel()) {
         logger_->warn("[RFQDialog::reject] settlement container failed to cancel");
      }
   }

   if (cancelOnClose_) {
      quoteProvider_->CancelQuote(QString::fromStdString(rfq_.requestId));
   }

   QDialog::reject();
}

bool RFQDialog::close()
{
   cancelOnClose_ = false;
   return QDialog::close();
}

void RFQDialog::onQuoteReceived(const bs::network::Quote& quote)
{
   ui_->pageRequestingQuote->onQuoteReceived(quote);
}

void RFQDialog::onSettlementAccepted()
{
   if (ccSettlContainer_) {
      const auto itCCOrder = ccReqIdToOrder_.find(QString::fromStdString(rfq_.requestId));
      if (itCCOrder != ccReqIdToOrder_.end()) {
         quoteProvider_->SignTxRequest(itCCOrder->second, ccSettlContainer_->txSignedData());
         ccReqIdToOrder_.erase(QString::fromStdString(rfq_.requestId));
         close();
      } else {
         ccTxMap_[rfq_.requestId] = ccSettlContainer_->txSignedData();
      }
   } else if (xbtSettlContainer_) {
      if (XBTOrder_.settlementId != quote_.settlementId) {
         logger_->debug("[RFQDialog::onSettlementAccepted] did not receive proper order");
      }
      close();
   } else {
      // spotFX
      close();
   }
}

void RFQDialog::onSettlementOrder()
{
   logger_->debug("onSettlementOrder");
   if (ccSettlContainer_) {
      quoteProvider_->AcceptQuote(QString::fromStdString(rfq_.requestId), quote_
         , ccSettlContainer_->txData());
   }
}

void RFQDialog::onSignTxRequested(QString orderId, QString reqId)
{
   const auto itCCtx = ccTxMap_.find(reqId.toStdString());
   if (itCCtx == ccTxMap_.end()) {
      logger_->debug("[RFQDialog] signTX for reqId={} requested before signing", reqId.toStdString());
      ccReqIdToOrder_[reqId] = orderId;
      return;
   }
   quoteProvider_->SignTxRequest(orderId, itCCtx->second);
   ccTxMap_.erase(reqId.toStdString());
   close();
}

void RFQDialog::onOrderUpdated(const bs::network::Order& order)
{
   if (xbtSettlContainer_ && (order.settlementId == quote_.settlementId)
      && (rfq_.assetType == bs::network::Asset::SpotXBT) && (order.status == bs::network::Order::Pending)) {
         XBTOrder_ = order;
         xbtSettlContainer_->OrderReceived();
   }
}

void RFQDialog::onXBTQuoteAccept(std::string reqId, std::string hexPayoutTx)
{
   quoteProvider_->AcceptQuote(QString::fromStdString(reqId), quote_, hexPayoutTx);
}

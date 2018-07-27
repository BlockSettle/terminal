#include "RFQDialog.h"
#include "ui_RFQDialog.h"

#include <QtConcurrent/QtConcurrentRun>
#include <spdlog/logger.h>

#include "AssetManager.h"
#include "CCSettlementTransactionWidget.h"
#include "QuoteProvider.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "WalletsManager.h"
#include "XBTSettlementTransactionWidget.h"

enum StackWidgetId
{
   RequestingQuoteId,
   SettlementTransactionId
};

RFQDialog::RFQDialog(const std::shared_ptr<spdlog::logger> &logger, const bs::network::RFQ& rfq
   , const std::shared_ptr<TransactionData>& transactionData, const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<AuthAddressManager>& authAddressManager, const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<WalletsManager> &walletsManager, const std::shared_ptr<SignContainer> &container
   , std::shared_ptr<CelerClient> celerClient, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::RFQDialog())
   , logger_(logger)
   , rfq_(rfq)
   , transactionData_(transactionData)
   , quoteProvider_(quoteProvider)
   , authAddressManager_(authAddressManager)
   , walletsManager_(walletsManager)
   , container_(container)
   , assetMgr_(assetManager)
   , celerClient_(celerClient)
{
   ui_->setupUi(this);

   ui_->pageRequestingQuote->SetAssetManager(assetMgr_);
   ui_->pageRequestingQuote->SetCelerClient(celerClient_);

   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::cancelRFQ, this, &RFQDialog::onRFQCancelled);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::requestCancelled, this, &RFQDialog::reject);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::requestTimedOut, [this] {
      QMetaObject::invokeMethod(this, "close");
   });
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteAccepted, this, &RFQDialog::onRFQResponseAccepted);
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
      connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, this, &RFQDialog::onOrderUpdated);
   }

   ui_->pageRequestingQuote->populateDetails(rfq_, transactionData_);

   QtConcurrent::run([this] { quoteProvider_->SubmitRFQ(rfq_); });

   ui_->stackedWidgetRFQ->setCurrentIndex(RequestingQuoteId);
}

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

void RFQDialog::onRFQResponseAccepted(const QString &reqId, const bs::network::Quote& quote)
{
   quote_ = quote;

   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      quoteProvider_->AcceptQuoteFX(reqId, quote);
   } else {
      if (rfq_.assetType == bs::network::Asset::SpotXBT) {
         xbtSettlementWidget_ = new XBTSettlementTransactionWidget(this);
         xbtSettlementWidget_->init(logger_, authAddressManager_, assetMgr_
            , quoteProvider_, container_);

         connect(xbtSettlementWidget_, &XBTSettlementTransactionWidget::settlementAccepted
            , this, &RFQDialog::onSettlementAccepted);
         connect(xbtSettlementWidget_, &XBTSettlementTransactionWidget::settlementCancelled
            , this, &QDialog::close);

         xbtSettlementWidget_->reset(walletsManager_);

         xbtSettlementWidget_->populateDetails(rfq_, quote, transactionData_);

         auto settlementIndex = ui_->stackedWidgetRFQ->addWidget(xbtSettlementWidget_);
         ui_->stackedWidgetRFQ->setCurrentIndex(settlementIndex);
      } else {
         ccSettlementWidget_ = new CCSettlementTransactionWidget(this);
         ccSettlementWidget_->init(logger_, assetMgr_, container_);

         connect(ccSettlementWidget_, &CCSettlementTransactionWidget::settlementAccepted
            , this, &RFQDialog::onSettlementAccepted);
         connect(ccSettlementWidget_, &CCSettlementTransactionWidget::sendOrder
            , this, &RFQDialog::onSettlementOrder);
         connect(ccSettlementWidget_, &CCSettlementTransactionWidget::settlementCancelled
            , this, &QDialog::close);

         ccSettlementWidget_->reset(walletsManager_);
         const bs::Address genAddress =  assetMgr_->getCCGenesisAddr(rfq_.product);

         ccSettlementWidget_->populateDetails(rfq_, quote, transactionData_, genAddress);

         auto settlementIndex = ui_->stackedWidgetRFQ->addWidget(ccSettlementWidget_);
         ui_->stackedWidgetRFQ->setCurrentIndex(settlementIndex);
      }

   }
}

void RFQDialog::reject()
{
   if (cancelOnClose_) {
      const auto widget = ui_->stackedWidgetRFQ->currentWidget();
      if (widget) {
         if (!QMetaObject::invokeMethod(widget, "cancel")) {
            logger_->warn("[RFQDialog::reject] failed to find [cancel] method for current stacked widget");
         }
      }
      else {
         logger_->warn("[RFQDialog::reject] failed to get current stacked widget");
      }
   }
   QDialog::reject();
}

bool RFQDialog::close()
{
   cancelOnClose_ = false;
   return QDialog::close();
}

void RFQDialog::onRFQCancelled(const QString &reqId)
{
   quoteProvider_->CancelQuote(reqId);
}

void RFQDialog::onQuoteReceived(const bs::network::Quote& quote)
{
   ui_->pageRequestingQuote->onQuoteReceived(quote);
}

void RFQDialog::onSettlementAccepted()
{
   if (rfq_.assetType == bs::network::Asset::PrivateMarket) {
      const auto itCCOrder = ccReqIdToOrder_.find(QString::fromStdString(rfq_.requestId));
      if (itCCOrder != ccReqIdToOrder_.end()) {
         quoteProvider_->SignTxRequest(itCCOrder->second, ccSettlementWidget_->getTxSignedData());
         ccReqIdToOrder_.erase(QString::fromStdString(rfq_.requestId));
         close();
      } else {
         ccTxMap_[rfq_.requestId] = ccSettlementWidget_->getTxSignedData();
      }
   } else if (rfq_.assetType == bs::network::Asset::SpotXBT) {
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
   if (rfq_.assetType == bs::network::Asset::PrivateMarket) {
      const auto txData = ccSettlementWidget_->getCCTxData();
      quoteProvider_->AcceptQuote(QString::fromStdString(rfq_.requestId), quote_, txData);
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
   if (xbtSettlementWidget_ && (order.settlementId == quote_.settlementId)
      && (rfq_.assetType == bs::network::Asset::SpotXBT) && (order.status == bs::network::Order::Pending)) {
         XBTOrder_ = order;
         xbtSettlementWidget_->OrderReceived();
   }
}

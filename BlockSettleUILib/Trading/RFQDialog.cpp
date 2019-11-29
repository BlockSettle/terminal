/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQDialog.h"
#include "ui_RFQDialog.h"

#include <spdlog/logger.h>

#include "AssetManager.h"
#include "BSMessageBox.h"
#include "QuoteProvider.h"
#include "RFQRequestWidget.h"
#include "ReqCCSettlementContainer.h"
#include "ReqXBTSettlementContainer.h"
#include "RfqStorage.h"
#include "SignContainer.h"
#include "UiUtils.h"

RFQDialog::RFQDialog(const std::shared_ptr<spdlog::logger> &logger
   , const bs::network::RFQ& rfq
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<AuthAddressManager>& authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<BaseCelerClient> &celerClient
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<RfqStorage> &rfqStorage
   , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
   , const bs::Address &recvXbtAddr
   , const bs::Address &authAddr
   , const std::map<UTXO, std::string> &fixedXbtInputs
   , bs::UtxoReservationToken utxoRes
   , RFQRequestWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::RFQDialog())
   , logger_(logger)
   , rfq_(rfq)
   , recvXbtAddr_(recvXbtAddr)
   , quoteProvider_(quoteProvider)
   , authAddressManager_(authAddressManager)
   , walletsManager_(walletsManager)
   , signContainer_(signContainer)
   , assetMgr_(assetManager)
   , armory_(armory)
   , celerClient_(celerClient)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , rfqStorage_(rfqStorage)
   , xbtWallet_(xbtWallet)
   , authAddr_(authAddr)
   , fixedXbtInputs_(fixedXbtInputs)
   , requestWidget_(parent)
   , utxoRes_(std::move(utxoRes))
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

   ui_->pageRequestingQuote->populateDetails(rfq_);

   quoteProvider_->SubmitRFQ(rfq_);
}

RFQDialog::~RFQDialog() = default;

void RFQDialog::onOrderFilled(const std::string &quoteId)
{
   if (quote_.quoteId != quoteId) {
      return;
   }

   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      ui_->pageRequestingQuote->onOrderFilled(quoteId);
   }
}

void RFQDialog::onOrderFailed(const std::string& quoteId, const std::string& reason)
{
   if (quote_.quoteId != quoteId) {
      return;
   }

   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      ui_->pageRequestingQuote->onOrderFailed(quoteId, reason);
   }
   if (xbtSettlContainer_) {
      xbtSettlContainer_->cancel();
   }
   if (ccSettlContainer_) {
      ccSettlContainer_->cancel();
   }
   close();
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
      } else {
         curContainer_ = newCCcontainer();
      }
      if (curContainer_) {
         rfqStorage_->addSettlementContainer(curContainer_);
         curContainer_->activate();
      }
   }
}

void RFQDialog::logError(const QString& errorMessage)
{
   logger_->error("[RFQDialog::logError] {}", errorMessage.toStdString());
}

std::shared_ptr<bs::SettlementContainer> RFQDialog::newXBTcontainer()
{
   if (!xbtWallet_) {
      SPDLOG_LOGGER_ERROR(logger_, "xbt wallet is not set");
      return nullptr;
   }

   try {
      xbtSettlContainer_ = std::make_shared<ReqXBTSettlementContainer>(logger_
         , authAddressManager_, signContainer_, armory_, xbtWallet_, walletsManager_
         , rfq_, quote_, authAddr_, fixedXbtInputs_, recvXbtAddr_);

      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::settlementAccepted
         , this, &RFQDialog::onXBTSettlementAccepted);
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::settlementCancelled
         , this, &QDialog::close);
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::acceptQuote
         , this, &RFQDialog::onXBTQuoteAccept);
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::error
         , this, &RFQDialog::logError);

      // Use requestWidget_ as RFQDialog could be already destroyed before this moment
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::sendUnsignedPayinToPB
         , requestWidget_, &RFQRequestWidget::sendUnsignedPayinToPB);
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::sendSignedPayinToPB
         , requestWidget_, &RFQRequestWidget::sendSignedPayinToPB);
      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::sendSignedPayoutToPB
         , requestWidget_, &RFQRequestWidget::sendSignedPayoutToPB);
   }
   catch (const std::exception &e) {
      logError(tr("Failed to create XBT settlement container: %1")
         .arg(QString::fromLatin1(e.what())));
   }

   return xbtSettlContainer_;
}

void RFQDialog::hideIfNoRemoteSignerMode()
{
   if (signContainer_->opMode() != SignContainer::OpMode::Remote) {
      hide();
   }
}

std::shared_ptr<bs::SettlementContainer> RFQDialog::newCCcontainer()
{
   try {
      ccSettlContainer_ = std::make_shared<ReqCCSettlementContainer>(logger_
         , signContainer_, armory_, assetMgr_, walletsManager_, rfq_, quote_, xbtWallet_, fixedXbtInputs_, std::move(utxoRes_));

      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::txSigned
         , this, &RFQDialog::onCCTxSigned);
      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::sendOrder
         , this, &RFQDialog::onCCQuoteAccepted);
      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::settlementCancelled
         , this, &QDialog::close);
   }
   catch (const std::exception &e) {
      logError(tr("Failed to create CC settlement container: %1")
         .arg(QString::fromLatin1(e.what())));
   }

   return ccSettlContainer_;
}

void RFQDialog::onCCTxSigned()
{
   quoteProvider_->SignTxRequest(ccOrderId_, ccSettlContainer_->txSignedData());
   close();
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

void RFQDialog::onXBTSettlementAccepted()
{
   if (xbtSettlContainer_) {
      close();
   } else {
      logger_->error("[RFQDialog::onXBTSettlementAccepted] XBT settlement accepted with empty container");
   }
}

void RFQDialog::onCCQuoteAccepted()
{
   if (ccSettlContainer_) {
      quoteProvider_->AcceptQuote(QString::fromStdString(rfq_.requestId), quote_
         , ccSettlContainer_->txData());
   }
}

void RFQDialog::onSignTxRequested(QString orderId, QString reqId)
{
   if (QString::fromStdString(rfq_.requestId) != reqId) {
      logger_->debug("[RFQDialog::onSignTxRequested] not our request. ignore");
      return;
   }

   if (ccSettlContainer_ == nullptr) {
      logger_->error("[RFQDialog::onSignTxRequested] could not sign with missing container");
      return;
   }

   hideIfNoRemoteSignerMode();

   ccOrderId_ = orderId;
   ccSettlContainer_->startSigning();
}

void RFQDialog::onXBTQuoteAccept(std::string reqId, std::string hexPayoutTx)
{
   quoteProvider_->AcceptQuote(QString::fromStdString(reqId), quote_, hexPayoutTx);
}

void RFQDialog::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (!xbtSettlContainer_ || (settlementId != quote_.settlementId)) {
      return;
   }

   xbtSettlContainer_->onUnsignedPayinRequested(settlementId);
}

void RFQDialog::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash)
{
   if (!xbtSettlContainer_ || (settlementId != quote_.settlementId)) {
      return;
   }

   hideIfNoRemoteSignerMode();

   xbtSettlContainer_->onSignedPayoutRequested(settlementId, payinHash);
}

void RFQDialog::onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin)
{
   if (!xbtSettlContainer_ || (settlementId != quote_.settlementId)) {
      return;
   }

   hideIfNoRemoteSignerMode();

   xbtSettlContainer_->onSignedPayinRequested(settlementId, unsignedPayin);
}

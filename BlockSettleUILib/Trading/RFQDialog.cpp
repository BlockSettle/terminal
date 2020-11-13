/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQDialog.h"
#include "ui_RFQDialog.h"

#include <spdlog/spdlog.h>

#include "AssetManager.h"
#include "BSMessageBox.h"
#include "QuoteProvider.h"
#include "RFQRequestWidget.h"
#include "ReqCCSettlementContainer.h"
#include "ReqXBTSettlementContainer.h"
#include "RfqStorage.h"
#include "UiUtils.h"
#include "UtxoReservationManager.h"
#include "WalletSignerContainer.h"
#include "Wallets/SyncHDWallet.h"


RFQDialog::RFQDialog(const std::shared_ptr<spdlog::logger> &logger
   , const std::string &id, const bs::network::RFQ& rfq
   , const std::shared_ptr<QuoteProvider>& quoteProvider
   , const std::shared_ptr<AuthAddressManager>& authAddressManager
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<WalletSignerContainer> &signContainer
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<CelerClientQt> &celerClient
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<RfqStorage> &rfqStorage
   , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
   , const bs::Address &recvXbtAddrIfSet
   , const bs::Address &authAddr
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , const std::map<UTXO, std::string> &fixedXbtInputs
   , bs::UtxoReservationToken fixedXbtUtxoRes
   , bs::UtxoReservationToken ccUtxoRes
   , bs::hd::Purpose purpose
   , RFQRequestWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::RFQDialog())
   , logger_(logger)
   , id_(id), rfq_(rfq)
   , recvXbtAddrIfSet_(recvXbtAddrIfSet)
   , quoteProvider_(quoteProvider)
   , authAddressManager_(authAddressManager)
   , walletsManager_(walletsManager)
   , signContainer_(signContainer)
   , assetMgr_(assetManager)
   , armory_(armory)
   , celerClient_(celerClient)
   , appSettings_(appSettings)
   , rfqStorage_(rfqStorage)
   , xbtWallet_(xbtWallet)
   , authAddr_(authAddr)
   , fixedXbtInputs_(fixedXbtInputs)
   , fixedXbtUtxoRes_(std::move(fixedXbtUtxoRes))
   , requestWidget_(parent)
   , utxoReservationManager_(utxoReservationManager)
   , ccUtxoRes_(std::move(ccUtxoRes))
   , walletPurpose_(purpose)
{
   ui_->setupUi(this);

   ui_->pageRequestingQuote->SetAssetManager(assetMgr_);
   ui_->pageRequestingQuote->SetCelerClient(celerClient_);

   // NOTE: RFQDialog could be destroyed before SettlementContainer work is done.
   // Do not make connections that must live after RFQDialog closing.

   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::cancelRFQ, this, &RFQDialog::reject);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::requestTimedOut, this, &RFQDialog::onTimeout);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteAccepted, this
      , &RFQDialog::onRFQResponseAccepted, Qt::QueuedConnection);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFinished, this, &RFQDialog::onQuoteFinished);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFailed, this, &RFQDialog::onQuoteFailed);

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

RFQDialog::RFQDialog(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& id, const bs::network::RFQ& rfq
   , const std::string& xbtWalletId, const bs::Address& recvXbtAddrIfSet
   , const bs::Address& authAddr
   , bs::hd::Purpose purpose
   , RFQRequestWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::RFQDialog())
   , logger_(logger)
   , id_(id), rfq_(rfq)
   , recvXbtAddrIfSet_(recvXbtAddrIfSet)
   , authAddr_(authAddr)
   , requestWidget_(parent)
   , walletPurpose_(purpose)
{
   ui_->setupUi(this);

   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::cancelRFQ, this
      , &RFQDialog::reject);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::requestTimedOut
      , this, &RFQDialog::onTimeout);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteAccepted, this
      , &RFQDialog::onRFQResponseAccepted);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFinished, this
      , &RFQDialog::onQuoteFinished);
   connect(ui_->pageRequestingQuote, &RequestingQuoteWidget::quoteFailed, this
      , &RFQDialog::onQuoteFailed);

   ui_->pageRequestingQuote->populateDetails(rfq_);
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
   close();
}

void RFQDialog::onRFQResponseAccepted(const std::string &reqId
   , const bs::network::Quote &quote)
{
   emit accepted(reqId, quote);
   quote_ = quote;

   if (rfq_.assetType == bs::network::Asset::SpotFX) {
      if (quoteProvider_) {
         quoteProvider_->AcceptQuoteFX(QString::fromStdString(reqId), quote);
      }
   }
   else {
      if (armory_ && walletsManager_) {
         if (rfq_.assetType == bs::network::Asset::SpotXBT) {
            curContainer_ = newXBTcontainer();
         } else {
            curContainer_ = newCCcontainer();
         }

         if (curContainer_) {
            rfqStorage_->addSettlementContainer(curContainer_);
            curContainer_->activate();

            // Do not capture `this` here!
            auto failedCb = [qId = quote_.quoteId, curContainer = curContainer_.get()]
            (const std::string& quoteId, const std::string& reason)
            {
               if (qId == quoteId) {
                  curContainer->cancel();
               }
            };
            connect(quoteProvider_.get(), &QuoteProvider::orderFailed, curContainer_.get(), failedCb);
         }
      }
      else {
         logger_->debug("[{}] non-FX", __func__);
      }
   }
}

void RFQDialog::logError(const std::string &id, bs::error::ErrorCode code, const QString &errorMessage)
{
   logger_->error("[RFQDialog::logError] {}: {}", id, errorMessage.toStdString());

   if (bs::error::ErrorCode::TxCancelled != code) {
      // Do not use this as the parent as it will be destroyed when RFQDialog is closed
      QMetaObject::invokeMethod(qApp, [code, errorMessage] {
         MessageBoxBroadcastError(errorMessage, code).exec();
      }, Qt::QueuedConnection);
   }
}

std::shared_ptr<bs::SettlementContainer> RFQDialog::newXBTcontainer()
{
   if (!xbtWallet_) {
      SPDLOG_LOGGER_ERROR(logger_, "xbt wallet is not set");
      return nullptr;
   }

   const bool expandTxInfo = appSettings_->get<bool>(
      ApplicationSettings::DetailedSettlementTxDialogByDefault);

   const auto tier1XbtLimit = appSettings_->get<uint64_t>(
      ApplicationSettings::SubmittedAddressXbtLimit);

   try {
      xbtSettlContainer_ = std::make_shared<ReqXBTSettlementContainer>(logger_
         , authAddressManager_, signContainer_, armory_, xbtWallet_, walletsManager_
         , rfq_, quote_, authAddr_, fixedXbtInputs_, std::move(fixedXbtUtxoRes_), utxoReservationManager_
         , walletPurpose_, recvXbtAddrIfSet_, expandTxInfo, tier1XbtLimit);

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

      connect(xbtSettlContainer_.get(), &ReqXBTSettlementContainer::cancelTrade
         , requestWidget_, &RFQRequestWidget::cancelXBTTrade);
   }
   catch (const std::exception &e) {
      logError({}, bs::error::ErrorCode::InternalError
         , tr("Failed to create XBT settlement container: %1")
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
   const bool expandTxInfo = appSettings_->get<bool>(
      ApplicationSettings::DetailedSettlementTxDialogByDefault);

   try {
      ccSettlContainer_ = std::make_shared<ReqCCSettlementContainer>(logger_
         , signContainer_, armory_, assetMgr_, walletsManager_, rfq_, quote_
         , xbtWallet_, fixedXbtInputs_, utxoReservationManager_, walletPurpose_
         , std::move(ccUtxoRes_), expandTxInfo);

      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::txSigned
         , this, &RFQDialog::onCCTxSigned);
      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::sendOrder
         , this, &RFQDialog::onCCQuoteAccepted);
      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::settlementCancelled
         , this, &QDialog::close);
      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::error
         , this, &RFQDialog::logError);

      connect(ccSettlContainer_.get(), &ReqCCSettlementContainer::cancelTrade
         , requestWidget_, &RFQRequestWidget::cancelCCTrade);

      // Do not make circular dependency, capture bare pointer
      auto orderUpdatedCb = [qId = quote_.quoteId, ccContainer = ccSettlContainer_.get()] (const bs::network::Order& order) {
         if (order.status == bs::network::Order::Pending && order.quoteId == qId) {
            ccContainer->setClOrdId(order.clOrderId);
         }
      };
      connect(quoteProvider_.get(), &QuoteProvider::orderUpdated, ccSettlContainer_.get(), orderUpdatedCb);
   }
   catch (const std::exception &e) {
      logError({}, bs::error::ErrorCode::InternalError
         , tr("Failed to create CC settlement container: %1")
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
   cancel(false);
   emit cancelled(id_);
   QDialog::reject();
}

void RFQDialog::cancel(bool force)
{
   // curContainer_->cancel call could emit settlementCancelled which will result in RFQDialog::reject re-enter.
   // This will result in duplicated finished signals. Let's add a workaround for this.
   if (isRejectStarted_) {
      return;
   }
   isRejectStarted_ = true;

   if (cancelOnClose_) {
      if (curContainer_) {
         if (curContainer_->cancel()) {
            logger_->debug("[RFQDialog::reject] container cancelled");
         } else {
            logger_->warn("[RFQDialog::reject] settlement container failed to cancel");
         }
      }
      else {
         fixedXbtUtxoRes_.release();
         ccUtxoRes_.release();
      }
   }

   if (cancelOnClose_) {
      if (quoteProvider_) {
         quoteProvider_->CancelQuote(QString::fromStdString(rfq_.requestId));
      }
      else {
         emit cancelled(rfq_.requestId);
      }
   }
   if (force) {
      close();
   }
}

void RFQDialog::onBalance(const std::string& currency, double balance)
{
   ui_->pageRequestingQuote->onBalance(currency, balance);
}

void RFQDialog::onMatchingLogout()
{
   ui_->pageRequestingQuote->onMatchingLogout();
}

void RFQDialog::onSettlementPending(const std::string& quoteId, const BinaryData& settlementId)
{
   //TODO: update UI state
}

void RFQDialog::onSettlementComplete()
{
   accept();
}

void RFQDialog::onTimeout()
{
   emit expired(id_);
   cancelOnClose_ = false;
   hide();
}

void RFQDialog::onQuoteFinished()
{
   if (quoteProvider_) {
      emit accepted(id_, quote_);
   }
   cancelOnClose_ = false;
   hide();
}

void RFQDialog::onQuoteFailed()
{
   emit cancelled(id_);
   close();
}

bool RFQDialog::close()
{
   curContainer_.reset();
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

void RFQDialog::onSignTxRequested(QString orderId, QString reqId, QDateTime timestamp)
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
   ccSettlContainer_->startSigning(timestamp);
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

void RFQDialog::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp)
{
   if (!xbtSettlContainer_ || (settlementId != quote_.settlementId)) {
      return;
   }

   hideIfNoRemoteSignerMode();

   xbtSettlContainer_->onSignedPayoutRequested(settlementId, payinHash, timestamp);
}

void RFQDialog::onSignedPayinRequested(const std::string& settlementId
   , const BinaryData& unsignedPayin, const BinaryData &payinHash, QDateTime timestamp)
{
   if (!xbtSettlContainer_ || (settlementId != quote_.settlementId)) {
      return;
   }

   hideIfNoRemoteSignerMode();

   xbtSettlContainer_->onSignedPayinRequested(settlementId, payinHash, timestamp);
}

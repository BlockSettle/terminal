#include "RequestingQuoteWidget.h"
#include "ui_RequestingQuoteWidget.h"
#include <QStyle>

#include "AssetManager.h"
#include "BlockDataManagerConfig.h"
#include "CurrencyPair.h"
#include "UiUtils.h"
#include "CelerClient.h"

// XXX [AT] : possible concurent change of states - could lead to multiple signals emited
// add atomic flag

static const char* WaitingPropertyName = "statusWarning";
static const char* RepliedPropertyName = "statusSuccess";
static const char* RejectedPropertyName = "statusImportant";

RequestingQuoteWidget::RequestingQuoteWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::RequestingQuoteWidget())
   , requestTimer_(this)
{
   ui_->setupUi(this);

   ui_->labelQuoteValue->setText(tr("Waiting for quote..."));
   ui_->labelDetails->clear();
   ui_->labelDetails->hide();
   ui_->labelQuoteValue->show();

   ui_->pushButtonAccept->hide();
   ui_->labelHint->clear();
   ui_->labelHint->hide();

   setupTimer(Indicative, QDateTime::currentDateTime().addSecs(30));

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &RequestingQuoteWidget::onCancel);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &RequestingQuoteWidget::onAccept);
}

RequestingQuoteWidget::~RequestingQuoteWidget() = default;

void RequestingQuoteWidget::SetCelerClient(std::shared_ptr<BaseCelerClient> celerClient) {
   celerClient_ = celerClient;

   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed,
      this, &RequestingQuoteWidget::onCelerDisconnected);
}

void RequestingQuoteWidget::setupTimer(RequestingQuoteWidget::Status status, const QDateTime &expTime)
{
   ui_->pushButtonAccept->setEnabled(status == Tradeable);

   timeoutReply_ = expTime;

   ui_->progressBar->setMaximum(QDateTime::currentDateTime().msecsTo(timeoutReply_));
   ui_->progressBar->setValue(ui_->progressBar->maximum());
   ticker();

   requestTimer_.setInterval(500);
   connect(&requestTimer_, &QTimer::timeout, this, &RequestingQuoteWidget::ticker);
   requestTimer_.start();
}

void RequestingQuoteWidget::onCancel()
{
   requestTimer_.stop();
   if (quote_.quotingType == bs::network::Quote::Tradeable) {
      emit requestTimedOut();
   } else {
      emit cancelRFQ();
   }
}

void RequestingQuoteWidget::ticker()
{
   auto timeDiff = QDateTime::currentDateTime().msecsTo(timeoutReply_);

   if (timeDiff < -5000) {
      requestTimer_.stop();
      emit requestTimedOut();
   }
   else {
      ui_->progressBar->setValue(timeDiff);
      ui_->progressBar->setFormat(tr("%1 second(s) remaining")
         .arg(QString::number(timeDiff > 0 ? timeDiff/1000 : 0)));

      ui_->labelTimeLeft->setText(tr("%1 second(s) remaining")
                                  .arg(QString::number(timeDiff > 0 ? timeDiff/1000 : 0)));
   }
}

void  RequestingQuoteWidget::onOrderFilled(const std::string &quoteId)
{
   if (quote_.quoteId == quoteId) {
      emit quoteFinished();
   }
}

void  RequestingQuoteWidget::onOrderFailed(const std::string& quoteId, const std::string &reason)
{
   Q_UNUSED(reason);
   if (quote_.quoteId == quoteId) {
      emit quoteFailed();
   }
}

bool RequestingQuoteWidget::onQuoteReceived(const bs::network::Quote& quote)
{
   if (quote.requestId != rfq_.requestId) {
      return false;
   }

   quote_ = quote;
   if (quote_.product.empty()) {
      quote_.product = rfq_.product;
   }

   if (quote_.quotingType == bs::network::Quote::Tradeable) {
      if (!balanceOk_) {
         return false;
      }

      if (quote.assetType == bs::network::Asset::SpotFX) {
         ui_->pushButtonAccept->show();
         setupTimer(Tradeable, quote.expirationTime.addMSecs(quote.timeSkewMs));
      } else {
         onAccept();
      }

      return true;
   }

   if (rfq_.side == bs::network::Side::Buy && rfq_.assetType != bs::network::Asset::SpotFX) {
      double amount = 0;
      if (rfq_.assetType == bs::network::Asset::PrivateMarket) {
         amount = rfq_.quantity * quote_.price;
      }
      else if (rfq_.product != bs::network::XbtCurrency) {
         amount = rfq_.quantity / quote_.price;
      }
   }

   timeoutReply_ = quote.expirationTime.addMSecs(quote.timeSkewMs);

   const auto assetType = assetManager_->GetAssetTypeForSecurity(quote.security);
   ui_->labelQuoteValue->setText(UiUtils::displayPriceForAssetType(quote.price, assetType));
   ui_->labelQuoteValue->show();

   if (quote.assetType == bs::network::Asset::SpotFX) {
      ui_->labelHint->clear();
      ui_->labelHint->hide();
   }

   double value = rfq_.quantity * quote.price;

   QString productString = QString::fromStdString(rfq_.product);
   QString productAmountString = UiUtils::displayAmountForProduct(rfq_.quantity, productString, rfq_.assetType);

   QString contrProductString;
   QString valueString;

   CurrencyPair cp(rfq_.security);

   if (cp.NumCurrency() != rfq_.product) {
      value = rfq_.quantity / quote.price;
      contrProductString = QString::fromStdString(cp.NumCurrency());
   } else {
      contrProductString = QString::fromStdString(cp.DenomCurrency());
   }

   valueString = UiUtils::displayAmountForProduct(value, contrProductString, rfq_.assetType);

   if (rfq_.side == bs::network::Side::Buy) {
      const auto currency = contrProductString.toStdString();
      const auto balance = assetManager_->getBalance(currency);
      balanceOk_ = (value < balance);
      ui_->pushButtonAccept->setEnabled(balanceOk_);
      if (!balanceOk_) {
         ui_->labelHint->setText(tr("Insufficient balance"));
         ui_->labelHint->show();
      }
   }

   if (rfq_.side == bs::network::Side::Buy && !balanceOk_) {
      return true;
   }

   ui_->labelDetails->setText(tr("%1 %2 %3\n%4 %5 %6")
      .arg((rfq_.side == bs::network::Side::Buy) ? tr("Receive") : tr("Deliver"))
      .arg(productAmountString)
      .arg(productString)
      .arg((rfq_.side == bs::network::Side::Buy) ? tr("Deliver") : tr("Receive"))
      .arg(valueString)
      .arg(contrProductString));
   ui_->labelDetails->show();

   return true;
}

void RequestingQuoteWidget::onQuoteCancelled(const QString &reqId, bool byUser)
{
   if (!byUser && (reqId.toStdString() == rfq_.requestId)) {
      quote_ = bs::network::Quote();
      ui_->labelQuoteValue->setText(tr("Waiting for quote..."));
      ui_->labelDetails->clear();
      ui_->labelDetails->hide();
   }
}

void RequestingQuoteWidget::onReject(const QString &reqId, const QString &reason)
{
   if (reqId.toStdString() == rfq_.requestId) {
      ui_->pushButtonAccept->setEnabled(false);
      ui_->labelQuoteValue->setText(tr("Rejected: %1").arg(reason));
      ui_->labelQuoteValue->show();
   }
}

void RequestingQuoteWidget::onCelerDisconnected()
{
   onCancel();
}

void RequestingQuoteWidget::populateDetails(const bs::network::RFQ& rfq)
{
   rfq_ = rfq;

   ui_->labelProductGroup->setText(tr(bs::network::Asset::toString(rfq.assetType)));
   ui_->labelSecurityId->setText(QString::fromStdString(rfq.security));
   ui_->labelProduct->setText(QString::fromStdString(rfq.product));
   ui_->labelSide->setText(tr(bs::network::Side::toString(rfq.side)));

   switch (rfq.assetType) {
   case bs::network::Asset::SpotFX:
      ui_->labelQuantity->setText(UiUtils::displayCurrencyAmount(rfq.quantity));
      break;
   case bs::network::Asset::SpotXBT:
      ui_->labelQuantity->setText(UiUtils::displayQty(rfq.quantity, rfq.product));
      break;
   case bs::network::Asset::PrivateMarket:
      ui_->labelQuantity->setText(UiUtils::displayCCAmount(rfq.quantity));
      break;
   default: break;
   }
}

void RequestingQuoteWidget::onAccept()
{
   requestTimer_.stop();
   ui_->progressBar->hide();
   ui_->pushButtonAccept->setEnabled(false);
   ui_->labelHint->setText(tr("Awaiting Settlement Pay-Out Execution"));
   ui_->labelHint->show();

   emit quoteAccepted(QString::fromStdString(rfq_.requestId), quote_);
}

void RequestingQuoteWidget::SetQuoteDetailsState(QuoteDetailsState state)
{
   ui_->widgetQuoteDetails->setProperty(WaitingPropertyName, false);
   ui_->widgetQuoteDetails->setProperty(RepliedPropertyName, false);
   ui_->widgetQuoteDetails->setProperty(RejectedPropertyName, false);

   switch (state)
   {
   case Waiting:
      ui_->widgetQuoteDetails->setProperty(WaitingPropertyName, true);
      break;
   case Replied:
      ui_->widgetQuoteDetails->setProperty(RepliedPropertyName, true);
      break;
   case Rejected:
      ui_->widgetQuoteDetails->setProperty(RejectedPropertyName, true);
      break;
   }

   ui_->widgetQuoteDetails->style()->unpolish(ui_->widgetQuoteDetails);
   ui_->widgetQuoteDetails->style()->polish(ui_->widgetQuoteDetails);
}

#include "PullOwnOTCRequestWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "ui_PullOwnOTCRequestWidget.h"

using namespace bs::network;

namespace {
   const int kTimerRepeatTimeMSec = 500;
   const QString secondsRemaining = QObject::tr("second(s) remaining");

   const QString headerTextOTCRequest = QObject::tr("OTC REQUEST");
   const QString headerTextOTCResponse = QObject::tr("OTC RESPONSE");
   const QString headerTextOTCPendingBuyerSign = QObject::tr("OTC PENDING SETTLEMENT PAY-IN");
   const QString headerTextOTCPendingSellerSign = QObject::tr("OTC PENDING SETTLEMENT PAY-OUT");

   const QString buttonTextPull = QObject::tr("PULL");
   const QString buttonTextCancel = QObject::tr("CANCEL");

   int getSeconds(std::chrono::milliseconds durationInMillisecs) {
      return int(std::chrono::duration_cast<std::chrono::seconds>(durationInMillisecs).count());
   }
}

PullOwnOTCRequestWidget::PullOwnOTCRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase(parent)
   , ui_{ new Ui::PullOwnOTCRequestWidget() }
{
   ui_->setupUi(this);

   connect(&pullTimer_, &QTimer::timeout, this, &PullOwnOTCRequestWidget::onUpdateTimerData);
   connect(ui_->pullPushButton, &QPushButton::clicked, this, &PullOwnOTCRequestWidget::currentRequestPulled);

   pullTimer_.setInterval(kTimerRepeatTimeMSec);
}

PullOwnOTCRequestWidget::~PullOwnOTCRequestWidget() = default;

void PullOwnOTCRequestWidget::setOffer(const bs::network::otc::Offer &offer)
{
   setupNegotiationInterface(headerTextOTCRequest);
   setupOfferInfo(offer);
   timeoutSec_ = getSeconds(bs::network::otc::negotiationTimeout());
}

void PullOwnOTCRequestWidget::setRequest(const bs::network::otc::QuoteRequest &request)
{
   setupNegotiationInterface(headerTextOTCRequest);

   ourSide_ = request.ourSide;
   ui_->sideValue->setText(QString::fromStdString(otc::toString(request.ourSide)));
   ui_->quantityValue->setText(QString::fromStdString(otc::toString(request.rangeType)));
   ui_->priceValue->clear();
   ui_->priceWidget->hide();

   timeoutSec_ = getSeconds(bs::network::otc::publicRequestTimeout());
}

void PullOwnOTCRequestWidget::setResponse(const otc::QuoteResponse &response)
{
   setupNegotiationInterface(headerTextOTCResponse, true /* isResponse */);

   ourSide_ = response.ourSide;
   ui_->sideValue->setText(QString::fromStdString(otc::toString(response.ourSide)));
   ui_->quantityValue->setText(getXBTRange(response.amount));
   ui_->priceValue->setText(getCCRange(response.price));
   ui_->priceWidget->show();

   timeoutSec_ = 0;
}

void PullOwnOTCRequestWidget::setPendingBuyerSign(const bs::network::otc::Offer &offer)
{
   setupSignAwaitingInterface(headerTextOTCPendingSellerSign);
   setupOfferInfo(offer);
   timeoutSec_ = getSeconds(bs::network::otc::payoutTimeout());
}

void PullOwnOTCRequestWidget::setPendingSellerSign(const bs::network::otc::Offer &offer)
{
   setupSignAwaitingInterface(headerTextOTCPendingBuyerSign);
   setupOfferInfo(offer);
   timeoutSec_ = getSeconds(bs::network::otc::payinTimeout());
}

void PullOwnOTCRequestWidget::setPeer(const bs::network::otc::Peer &peer)
{
   using namespace bs::network::otc;
   if ((peer.state == State::WaitBuyerSign && ourSide_ == otc::Side::Buy) ||
      (peer.state == State::WaitSellerSeal && ourSide_ == otc::Side::Sell)) {
      timeoutSec_ = 0;
      ui_->progressBarTimeLeft->hide();
      ui_->labelTimeLeft->hide();
      ui_->horizontalWidgetSubmit->hide();
   }
  
   if (peer.state != State::Idle) {
      ui_->sideValue->setText(getSide(ourSide_, peer.isOurSideSentOffer));
   }

   if (timeoutSec_) {
      setupTimer(peer.stateTimestamp);
   }
}

void PullOwnOTCRequestWidget::onUpdateTimerData()
{
   const auto diff = currentOfferEndTimestamp_ - std::chrono::steady_clock::now();
   const auto diffSeconds = std::chrono::duration_cast<std::chrono::seconds>(diff);

   ui_->labelTimeLeft->setText(QString(QLatin1String("%1 %2")).arg(diffSeconds.count()).arg(secondsRemaining));
   ui_->progressBarTimeLeft->setMaximum(timeoutSec_);
   ui_->progressBarTimeLeft->setValue(diffSeconds.count());

   if (diffSeconds.count() < 0) {
      pullTimer_.stop();
   }
}

void PullOwnOTCRequestWidget::setupTimer(const std::chrono::steady_clock::time_point& offerTimestamp)
{
   currentOfferEndTimestamp_ = offerTimestamp + std::chrono::seconds(timeoutSec_);
   onUpdateTimerData();
   pullTimer_.start();
}

void PullOwnOTCRequestWidget::setupNegotiationInterface(const QString& headerText, bool isResponse /* = false */)
{
   ui_->progressBarTimeLeft->setVisible(!isResponse);
   ui_->labelTimeLeft->setVisible(!isResponse);
   ui_->horizontalWidgetSubmit->show();
   ui_->pullPushButton->setText(buttonTextPull);
   ui_->headerLabel->setText(headerText);
}

void PullOwnOTCRequestWidget::setupSignAwaitingInterface(const QString& headerText)
{
   ui_->progressBarTimeLeft->show();
   ui_->labelTimeLeft->show();
   ui_->horizontalWidgetSubmit->show();
   ui_->pullPushButton->setText(buttonTextCancel);
   ui_->headerLabel->setText(headerText);
}

void PullOwnOTCRequestWidget::setupOfferInfo(const bs::network::otc::Offer &offer)
{
   ourSide_ = offer.ourSide;
   ui_->sideValue->setText(QString::fromStdString(otc::toString(offer.ourSide)));
   ui_->quantityValue->setText(UiUtils::displayAmount(otc::satToBtc(offer.amount)));
   ui_->priceValue->setText(UiUtils::displayCurrencyAmount(otc::fromCents(offer.price)));
   ui_->priceWidget->show();
}

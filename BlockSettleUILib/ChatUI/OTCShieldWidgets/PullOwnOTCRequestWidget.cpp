#include "PullOwnOTCRequestWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "ui_PullOwnOTCRequestWidget.h"

using namespace bs::network;

namespace {
   const int kTimeoutSec = 120;
   const int kTimerRepeatTimeMSec = 500;
   const QString secondsRemaining = QObject::tr("second(s) remaining");

   const QString headerTextOTCRequest = QObject::tr("OTC REQUEST");
   const QString headerTextOTCResponse = QObject::tr("OTC RESPONSE");
   const QString headerTextOTCPendingBuyerSign = QObject::tr("OTC PENDING SETTLEMENT PAY-IN");
   const QString headerTextOTCPendingSellerSign = QObject::tr("OTC PENDING SETTLEMENT PAY-OUT");

   const QString buttonTextPull = QObject::tr("PULL");
   const QString buttonTextCancel = QObject::tr("CANCEL");
}

PullOwnOTCRequestWidget::PullOwnOTCRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase(parent)
   , ui_{new Ui::PullOwnOTCRequestWidget()}
{
   ui_->setupUi(this);

   connect(&pullTimer_, &QTimer::timeout, this, &PullOwnOTCRequestWidget::onUpdateTimerData);
   connect(ui_->pullPushButton, &QPushButton::clicked, this, &PullOwnOTCRequestWidget::currentRequestPulled);

   pullTimer_.setInterval(kTimerRepeatTimeMSec);
   pullTimer_.start();
}

PullOwnOTCRequestWidget::~PullOwnOTCRequestWidget() = default;

void PullOwnOTCRequestWidget::setOffer(const std::string& contactId, const bs::network::otc::Offer &offer)
{
   setupNegotiationInterface(headerTextOTCRequest);
   setupOfferInfo(offer);
   setupTimer(contactId);
}

void PullOwnOTCRequestWidget::setRequest(const std::string& contactId, const bs::network::otc::QuoteRequest &request)
{
   setupNegotiationInterface(headerTextOTCRequest);

   ui_->sideValue->setText(QString::fromStdString(otc::toString(request.ourSide)));
   ui_->quantityValue->setText(QString::fromStdString(otc::toString(request.rangeType)));
   ui_->priceValue->clear();
   ui_->priceWidget->hide();

   setupTimer(contactId);
}

void PullOwnOTCRequestWidget::setResponse(const std::string& contactId, const otc::QuoteResponse &response)
{
   setupNegotiationInterface(headerTextOTCResponse);

   ui_->sideValue->setText(QString::fromStdString(otc::toString(response.ourSide)));
   ui_->quantityValue->setText(getXBTRange(response.amount));
   ui_->priceValue->setText(getCCRange(response.price));
   ui_->priceWidget->show();

   setupTimer(contactId);
}

void PullOwnOTCRequestWidget::setPendingBuyerSign(const bs::network::otc::Offer &offer)
{
   setupSignAwaitingInterface(headerTextOTCPendingBuyerSign);
   setupOfferInfo(offer);
}

void PullOwnOTCRequestWidget::setPendingSellerSign(const bs::network::otc::Offer &offer)
{
   setupSignAwaitingInterface(headerTextOTCPendingSellerSign);
   setupOfferInfo(offer);
}

void PullOwnOTCRequestWidget::registerOTCUpdatedTime(const bs::network::otc::Peer* peer, QDateTime timestamp)
{
   if (!peer) {
      return;
   }

   using namespace bs::network::otc;

   bool isNeedTracking = false;
   switch (peer->state) {
   case State::Idle:
      if (peer->isOwnRequest) {
         isNeedTracking = true;
      }
      break;
   case State::QuoteSent:
   case State::OfferSent:
      isNeedTracking = true;
      break;
   default:
      break;
   }

   if (isNeedTracking) {
      timestamps_[peer->contactId] = { timestamp,  peer->type };
   }
   else {
      timestamps_.erase(peer->contactId);
   }
}

void PullOwnOTCRequestWidget::onLogout()
{
   timestamps_.clear();
}

void PullOwnOTCRequestWidget::onUpdateTimerData()
{
   const auto timeLeft = QDateTime::currentDateTime().secsTo(currentOfferEndTimestamp_);
   ui_->labelTimeLeft->setText(QString(QLatin1String("%1 %2")).arg(timeLeft).arg(secondsRemaining));
   ui_->progressBarTimeLeft->setMaximum(kTimeoutSec);
   ui_->progressBarTimeLeft->setValue(timeLeft);

   if (timestamps_.empty()) {
      return;
   }

   for (auto iBegin = timestamps_.begin(); iBegin != timestamps_.end();) {
      const auto pullEndTime = iBegin->second.arrivedTime_.addSecs(kTimeoutSec);
      const auto timeLeft = QDateTime::currentDateTime().secsTo(pullEndTime);
      if (timeLeft < 0) {
         const auto contactId = iBegin->first;
         const auto peerType = iBegin->second.peerType_;
         ++iBegin;
         emit requestPulled(contactId, peerType);
      }
      else {
         ++iBegin;
      }
   }
}

void PullOwnOTCRequestWidget::setupTimer(const std::string& contactId)
{
   auto iStartTimeStamp = timestamps_.find(contactId);
   Q_ASSERT(iStartTimeStamp != timestamps_.end());
   const QDateTime offerStartTimestamp = iStartTimeStamp->second.arrivedTime_;
   currentOfferEndTimestamp_ = offerStartTimestamp.addSecs(kTimeoutSec);

   onUpdateTimerData();
}

void PullOwnOTCRequestWidget::setupNegotiationInterface(const QString& headerText)
{
   ui_->progressBarTimeLeft->show();
   ui_->labelTimeLeft->show();
   ui_->pullPushButton->setText(buttonTextPull);
   ui_->headerLabel->setText(headerText);
}

void PullOwnOTCRequestWidget::setupSignAwaitingInterface(const QString& headerText)
{
   ui_->progressBarTimeLeft->hide();
   ui_->labelTimeLeft->hide();
   ui_->pullPushButton->setText(buttonTextCancel);
   ui_->headerLabel->setText(headerText);
}

void PullOwnOTCRequestWidget::setupOfferInfo(const bs::network::otc::Offer &offer)
{
   ui_->sideValue->setText(QString::fromStdString(otc::toString(offer.ourSide)));
   ui_->quantityValue->setText(UiUtils::displayAmount(otc::satToBtc(offer.amount)));
   ui_->priceValue->setText(UiUtils::displayCurrencyAmount(otc::fromCents(offer.price)));
   ui_->priceWidget->show();
}

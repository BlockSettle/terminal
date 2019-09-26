#include "PullOwnOTCRequestWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "ui_PullOwnOTCRequestWidget.h"

using namespace bs::network;

PullOwnOTCRequestWidget::PullOwnOTCRequestWidget(QWidget* parent)
   : OTCWindowsAdapterBase(parent)
   , ui_{new Ui::PullOwnOTCRequestWidget()}
{
   ui_->setupUi(this);

   connect(ui_->pullPushButton, &QPushButton::clicked, this, &PullOwnOTCRequestWidget::requestPulled);
}

PullOwnOTCRequestWidget::~PullOwnOTCRequestWidget() = default;

void PullOwnOTCRequestWidget::setOffer(const bs::network::otc::Offer &offer)
{
   // #new_logic : fix security & product checking
   ui_->headerLabel->setText(tr("OTC Request"));
   ui_->sideValue->setText(QString::fromStdString(otc::toString(offer.ourSide)));

   ui_->quantityValue->setText(UiUtils::displayAmount(otc::satToBtc(offer.amount)));

   ui_->priceValue->setText(UiUtils::displayCurrencyAmount(otc::fromCents(offer.price)));
   ui_->priceWidget->show();
}

void PullOwnOTCRequestWidget::setRequest(const bs::network::otc::QuoteRequest &request)
{
   ui_->headerLabel->setText(tr("OTC Request"));
   ui_->sideValue->setText(QString::fromStdString(otc::toString(request.ourSide)));

   ui_->quantityValue->setText(QString::fromStdString(otc::toString(request.rangeType)));

   ui_->priceValue->clear();
   ui_->priceWidget->hide();
}

void PullOwnOTCRequestWidget::setResponse(const otc::QuoteResponse &response)
{
   ui_->headerLabel->setText(tr("OTC Response"));
   ui_->sideValue->setText(QString::fromStdString(otc::toString(response.ourSide)));

   ui_->quantityValue->setText(QStringLiteral("%1 - %2")
      .arg(UiUtils::displayCurrencyAmount(response.amount.lower))
      .arg(UiUtils::displayCurrencyAmount(response.amount.upper)));

   ui_->priceValue->setText(QStringLiteral("%1 - %2")
      .arg(UiUtils::displayCurrencyAmount(otc::fromCents(response.price.lower)))
      .arg(UiUtils::displayCurrencyAmount(otc::fromCents(response.price.upper))));
   ui_->priceWidget->show();
}

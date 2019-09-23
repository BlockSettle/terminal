#include "PullOwnOTCRequestWidget.h"

#include "OtcTypes.h"
#include "UiUtils.h"
#include "ui_PullOwnOTCRequestWidget.h"

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
   ui_->sideValue->setText(QString::fromStdString(bs::network::otc::toString(offer.ourSide)));
   ui_->priceValue->setText(UiUtils::displayCurrencyAmount(bs::network::otc::fromCents(offer.price)));
   ui_->quantityValue->setText(UiUtils::displayAmount(bs::network::otc::satToBtc(offer.amount)));
}

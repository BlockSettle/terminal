#include "OTCNegotiationRequestWidget.h"

#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationRequestWidget::OTCNegotiationRequestWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::OTCNegotiationCommonWidget{}}
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Request Negotiation"));
   ui_->labelQuantityValue->hide();

   ui_->spinBoxOffer->setAccelerated(true);
   ui_->spinBoxQuantity->setAccelerated(true);

   ui_->pushButtonCancel->setText(tr("Reject"));
   ui_->pushButtonAccept->setText(tr("Respond"));
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() noexcept = default;

void OTCNegotiationRequestWidget::DisplayResponse(const bs::network::Side::Type& side, const bs::network::OTCPriceRange& priceRange, const bs::network::OTCQuantityRange& amountRange)
{
   if (side == bs::network::Side::Sell) {
      ui_->labelSide->setText(tr("Sell"));
   } else {
      ui_->labelSide->setText(tr("Buy"));
   }

   ui_->spinBoxOffer->setMinimum(priceRange.lower);
   ui_->spinBoxOffer->setMaximum(priceRange.upper);
   ui_->spinBoxOffer->setValue(priceRange.lower);

   ui_->spinBoxQuantity->setMinimum(amountRange.lower);
   ui_->spinBoxQuantity->setMaximum(amountRange.upper);
   ui_->spinBoxQuantity->setValue(amountRange.lower);
}

#include "OTCNegotiationResponseWidget.h"

#include "OtcTypes.h"
#include "ui_OTCNegotiationCommonWidget.h"

#include <QComboBox>
#include <QPushButton>

OTCNegotiationResponseWidget::OTCNegotiationResponseWidget(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::OTCNegotiationCommonWidget{}}
{
   ui_->setupUi(this);

   ui_->headerLabel->setText(tr("OTC Response Negotiation"));

   ui_->pushButtonCancel->setText(tr("Reject"));

   connect(ui_->doubleSpinBoxOffer, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);
   connect(ui_->doubleSpinBoxQuantity, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);

   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onAcceptOrUpdateClicked);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::responseRejected);

   ui_->widgetSideButtons->hide();
   ui_->doubleSpinBoxQuantity->setEnabled(false);

   onChanged();
}

void OTCNegotiationResponseWidget::setOffer(const bs::network::otc::Offer &offer)
{
   receivedOffer_ = offer;

   ui_->labelSide->setText(QString::fromStdString(bs::network::otc::toString(offer.ourSide)));
   ui_->doubleSpinBoxOffer->setValue(bs::network::otc::fromCents(offer.price));
   ui_->doubleSpinBoxQuantity->setValue(bs::network::otc::satToBtc(offer.amount));

   onChanged();
}

bs::network::otc::Offer OTCNegotiationResponseWidget::offer() const
{
   bs::network::otc::Offer result;
   result.ourSide = receivedOffer_.ourSide;
   result.price = bs::network::otc::toCents(ui_->doubleSpinBoxOffer->value());
   result.amount = bs::network::otc::btcToSat(ui_->doubleSpinBoxQuantity->value());
   return result;
}

void OTCNegotiationResponseWidget::onChanged()
{
   if (receivedOffer_ == offer()) {
      ui_->pushButtonAccept->setText(tr("Accept"));
   } else {
      ui_->pushButtonAccept->setText(tr("Update"));
   }
}

void OTCNegotiationResponseWidget::onAcceptOrUpdateClicked()
{
   if (receivedOffer_ == offer()) {
      emit responseAccepted();
   } else {
      emit responseUpdated();
   }
}

OTCNegotiationResponseWidget::~OTCNegotiationResponseWidget() = default;

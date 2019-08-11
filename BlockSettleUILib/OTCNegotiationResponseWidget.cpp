#include "OTCNegotiationResponseWidget.h"

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

   connect(ui_->spinBoxOffer, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);
   connect(ui_->spinBoxQuantity, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &OTCNegotiationResponseWidget::onChanged);

   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::onAcceptOrUpdateClicked);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &OTCNegotiationResponseWidget::responseRejected);

   ui_->widgetSideButtons->hide();
   ui_->spinBoxQuantity->setEnabled(false);

   onChanged();
}

void OTCNegotiationResponseWidget::setOffer(const bs::network::otc::Offer &offer)
{
   receivedOffer_ = offer;

   ui_->labelSide->setText(QString::fromStdString(bs::network::otc::toString(offer.ourSide)));
   ui_->spinBoxOffer->setValue(offer.price);
   ui_->spinBoxQuantity->setValue(offer.amount);

   onChanged();
}

bs::network::otc::Offer OTCNegotiationResponseWidget::offer() const
{
   bs::network::otc::Offer result;
   result.ourSide = receivedOffer_.ourSide;
   result.price = ui_->spinBoxOffer->value();
   result.amount = ui_->spinBoxQuantity->value();
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

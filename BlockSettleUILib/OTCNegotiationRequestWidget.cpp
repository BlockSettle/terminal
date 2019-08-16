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

   ui_->spinBoxOffer->setAccelerated(true);
   ui_->spinBoxQuantity->setAccelerated(true);

   ui_->pushButtonCancel->hide();
   ui_->pushButtonAccept->setText(tr("Submit"));

   connect(ui_->pushButtonBuy, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onBuyClicked);
   connect(ui_->pushButtonSell, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::onSellClicked);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &OTCNegotiationRequestWidget::requestCreated);

   connect(ui_->spinBoxOffer, qOverload<int>(&QSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);
   connect(ui_->spinBoxQuantity, qOverload<int>(&QSpinBox::valueChanged), this, &OTCNegotiationRequestWidget::onChanged);

   ui_->widgetSideInfo->hide();

   onSellClicked();
   onChanged();
}

OTCNegotiationRequestWidget::~OTCNegotiationRequestWidget() = default;

bs::network::otc::Offer OTCNegotiationRequestWidget::offer() const
{
   bs::network::otc::Offer result;
   result.ourSide = ui_->pushButtonSell->isChecked() ? bs::network::otc::Side::Sell : bs::network::otc::Side::Buy;
   result.price = ui_->spinBoxOffer->value();
   result.amount = ui_->spinBoxQuantity->value();
   return result;
}

void OTCNegotiationRequestWidget::onSellClicked()
{
   ui_->pushButtonSell->setChecked(true);
   ui_->pushButtonBuy->setChecked(false);
}

void OTCNegotiationRequestWidget::onBuyClicked()
{
   ui_->pushButtonSell->setChecked(false);
   ui_->pushButtonBuy->setChecked(true);
}

void OTCNegotiationRequestWidget::onChanged()
{
   ui_->pushButtonAccept->setEnabled(ui_->spinBoxOffer->value() > 0 && ui_->spinBoxQuantity->value() > 0);
}

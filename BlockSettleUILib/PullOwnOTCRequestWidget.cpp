#include "PullOwnOTCRequestWidget.h"

#include "ui_PullOwnOTCRequestWidget.h"

PullOwnOTCRequestWidget::PullOwnOTCRequestWidget(QWidget* parent)
   : QWidget(parent)
   , ui_{new Ui::PullOwnOTCRequestWidget()}
{
   ui_->setupUi(this);

   ui_->widgetRange->hide();

   connect(ui_->pushButtonPull, &QPushButton::clicked, this, &PullOwnOTCRequestWidget::requestPulled);
}

PullOwnOTCRequestWidget::~PullOwnOTCRequestWidget() = default;

void PullOwnOTCRequestWidget::setOffer(const bs::network::otc::Offer &offer)
{
   ui_->labelSide->setText(QString::fromStdString(bs::network::otc::toString(offer.ourSide)));
   ui_->labelPrice->setText(QString::number(offer.price));
   ui_->labelQuantity->setText(QString::number(offer.amount));
}

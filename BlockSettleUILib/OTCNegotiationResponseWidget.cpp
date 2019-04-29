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
   ui_->spinBoxQuantity->hide();

   ui_->pushButtonCancel->setText(tr("Pull"));
   ui_->pushButtonAccept->setText(tr("Accept"));
}

OTCNegotiationResponseWidget::~OTCNegotiationResponseWidget() noexcept = default;

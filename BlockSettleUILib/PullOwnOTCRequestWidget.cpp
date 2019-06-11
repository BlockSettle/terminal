#include "PullOwnOTCRequestWidget.h"
#include "ui_PullOwnOTCRequestWidget.h"

PullOwnOTCRequestWidget::PullOwnOTCRequestWidget(QWidget* parent)
   : QWidget(parent)
   , ui_{new Ui::PullOwnOTCRequestWidget()}
{
   ui_->setupUi(this);

   ui_->pushButtonPull->setEnabled(false);
   connect(ui_->pushButtonPull, &QPushButton::clicked, this, &PullOwnOTCRequestWidget::OnPullPressed);
}

PullOwnOTCRequestWidget::~PullOwnOTCRequestWidget() noexcept = default;

void PullOwnOTCRequestWidget::OnPullPressed()
{
   emit PullOTCRequested();
}

void PullOwnOTCRequestWidget::setRequestData(const std::shared_ptr<Chat::OTCRequestData>& otc)
{
   ui_->labelSide->setText(QString::fromStdString(bs::network::ChatOTCSide::toString(otc->otcRequest().side)));
   ui_->labelRange->setText(QString::fromStdString(bs::network::OTCRangeID::toString(otc->otcRequest().amountRange)));

   ui_->pushButtonPull->setEnabled(true);
}

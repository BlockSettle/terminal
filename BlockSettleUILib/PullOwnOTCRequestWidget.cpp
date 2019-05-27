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
   emit PullOTCRequested(currentOtcId_);
}

void PullOwnOTCRequestWidget::DisplayActiveOTC(const std::shared_ptr<Chat::OTCRequestData>& otc)
{
   ui_->labelSide->setText(QLatin1String(bs::network::ChatOTCSide::toString(otc->otcRequest().side)));
   ui_->labelRange->setText(QString::fromStdString(bs::network::OTCRangeID::toString(otc->otcRequest().amountRange)));
   currentOtcId_ = otc->serverRequestId();

   ui_->pushButtonPull->setEnabled(true);
}

void PullOwnOTCRequestWidget::DisplaySubmittedOTC(const bs::network::OTCRequest& otc)
{
   ui_->pushButtonPull->setEnabled(false);

   ui_->labelSide->setText(QLatin1String(bs::network::ChatOTCSide::toString(otc.side)));
   ui_->labelRange->setText(QString::fromStdString(bs::network::OTCRangeID::toString(otc.amountRange)));
   currentOtcId_.clear();
}

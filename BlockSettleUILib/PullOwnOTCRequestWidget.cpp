#include "PullOwnOTCRequestWidget.h"

#include "ui_PullOwnOTCRequestWidget.h"
#include "ChatProtocol/ChatUtils.h"

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

void PullOwnOTCRequestWidget::setRequestData(const std::shared_ptr<Chat::Data>& otc)
{
   assert(otc->has_message());
   assert(otc->message().has_otc_request());

   ui_->labelSide->setText(ChatUtils::toString(otc->message().otc_request().side()));
   ui_->labelRange->setText(ChatUtils::toString(otc->message().otc_request().range_type()));

   ui_->pushButtonPull->setEnabled(true);
}

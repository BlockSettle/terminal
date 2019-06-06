#include "RFQReplyLoginRequiredShield.h"
#include "ui_RFQReplyLoginRequiredShield.h"

RFQReplyLoginRequiredShield::RFQReplyLoginRequiredShield(QWidget *parent) :
   QWidget(parent),
   ui_(new Ui::RFQReplyLoginRequiredShield)
{
   ui_->setupUi(this);
}

RFQReplyLoginRequiredShield::~RFQReplyLoginRequiredShield() noexcept = default;

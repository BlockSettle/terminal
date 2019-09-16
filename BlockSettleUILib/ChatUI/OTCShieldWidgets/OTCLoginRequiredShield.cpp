#include "OTCLoginRequiredShield.h"
#include "ui_OTCLoginRequiredShield.h"


OTCLoginRequiredShield::OTCLoginRequiredShield(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::OTCLoginRequiredShield())
{
   ui_->setupUi(this);
}

OTCLoginRequiredShield::~OTCLoginRequiredShield() noexcept = default;

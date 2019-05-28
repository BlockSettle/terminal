#include "OTCParticipantShield.h"
#include "ui_OTCParticipantShield.h"


OTCParticipantShield::OTCParticipantShield(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::OTCParticipantShield())
{
   ui_->setupUi(this);
}

OTCParticipantShield::~OTCParticipantShield() noexcept = default;

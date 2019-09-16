#include "OTCGeneralRoomShield.h"
#include "ui_OTCGeneralRoomShield.h"


OTCGeneralRoomShield::OTCGeneralRoomShield(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::OTCGeneralRoomShield())
{
   ui_->setupUi(this);
}

OTCGeneralRoomShield::~OTCGeneralRoomShield() noexcept = default;

#include "OTCSupportRoomShield.h"
#include "ui_OTCSupportRoomShield.h"

OTCSupportRoomShield::OTCSupportRoomShield(QWidget *parent) :
   QWidget(parent),
   ui_(new Ui::OTCSupportRoomShield())
{
   ui_->setupUi(this);
}

OTCSupportRoomShield::~OTCSupportRoomShield() noexcept = default;

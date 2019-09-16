#include "OTCContactShield.h"
#include "ui_OTCContactShield.h"

OTCContactShield::OTCContactShield(QWidget *parent) :
   QWidget(parent),
   ui(new Ui::OTCContactShield)
{
   ui->setupUi(this);
}

OTCContactShield::~OTCContactShield()
{
   delete ui;
}

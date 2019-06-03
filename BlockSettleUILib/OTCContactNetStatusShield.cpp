#include "OTCContactNetStatusShield.h"
#include "ui_OTCContactNetStatusShield.h"

OTCContactNetStatusShield::OTCContactNetStatusShield(QWidget *parent) :
   QWidget(parent),
   ui(new Ui::OTCContactNetStatusShield)
{
   ui->setupUi(this);
}

OTCContactNetStatusShield::~OTCContactNetStatusShield()
{
   delete ui;
}

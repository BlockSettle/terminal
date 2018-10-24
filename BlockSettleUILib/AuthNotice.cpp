
#include "AuthNotice.h"
#include "ui_AuthNotice.h"


//
// AuthNotice
//

AuthNotice::AuthNotice(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::AuthNotice)
{
   ui_->setupUi(this);
}

AuthNotice::~AuthNotice()
{
}

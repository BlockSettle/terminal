
#include "FrejaNotice.h"
#include "ui_FrejaNotice.h"


//
// FrejaNotice
//

FrejaNotice::FrejaNotice(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::FrejaNotice)
{
   ui_->setupUi(this);
}

FrejaNotice::~FrejaNotice()
{
}

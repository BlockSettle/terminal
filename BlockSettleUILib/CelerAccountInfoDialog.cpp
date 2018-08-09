#include "CelerAccountInfoDialog.h"
#include "ui_CelerAccountInfoDialog.h"

#include "CelerClient.h"

CelerAccountInfoDialog::CelerAccountInfoDialog(std::shared_ptr<CelerClient> celerConnection, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::CelerAccountInfoDialog())
{
   ui_->setupUi(this);
   ui_->labelEmailAddress->setText(QString::fromStdString(celerConnection->userName()));
   ui_->labelUserType->setText(celerConnection->userType());
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &CelerAccountInfoDialog::reject);
}

#include "WalletWarningDialog.h"

#include "ui_WalletWarningDialog.h"

WalletWarningDialog::WalletWarningDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::WalletWarningDialog())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &WalletWarningDialog::accept);
}

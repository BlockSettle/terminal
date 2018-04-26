#include "WalletCompleteDialog.h"
#include "ui_WalletCompleteDialog.h"


WalletCompleteDialog::WalletCompleteDialog(const QString& walletName
 , bool asPrimary, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::WalletCompleteDialog())
   , walletName_(walletName)
   , primary_(asPrimary)
   , primaryPrefix_(tr("Primary "))
{
   ui_->setupUi(this);

   connect(ui_->pushButtonFinish, &QPushButton::clicked, this, &WalletCompleteDialog::accept);
}

int WalletCompleteDialog::exec()
{
   setWindowTitle(titleText());
   ui_->labelText->setText(infoText());
   return QDialog::exec();
}


QString WalletCreateCompleteDialog::titleText() const
{
   return tr("%1Wallet Created").arg(primary_ ? primaryPrefix_ : QString{});
}

QString WalletCreateCompleteDialog::infoText() const
{
   return tr("Wallet \"%1\" Successfully Created").arg(walletName_);
}


QString WalletImportCompleteDialog::titleText() const
{
   return tr("%1Wallet Imported").arg(primary_ ? primaryPrefix_ : QString{});
}

QString WalletImportCompleteDialog::infoText() const
{
   return tr("Wallet \"%1\" Successfully Imported").arg(walletName_);
}

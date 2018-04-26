#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const QString& walletName
 , bool oldPasswordRequired
 , QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::ChangeWalletPasswordDialog())
  , oldPasswordRequired_(oldPasswordRequired)
{
   ui_->setupUi(this);

   ui_->labelWalletName->setText(walletName);
   ui_->lineEditOldPassword->setEnabled(oldPasswordRequired_);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);

   connect(ui_->lineEditOldPassword, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);
   connect(ui_->lineEditNewPassword, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);
   connect(ui_->lineEditNewPasswordConfirm, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);

   ui_->pushButtonOk->setEnabled(false);
}

QString ChangeWalletPasswordDialog::GetOldPassword() const
{
   return ui_->lineEditOldPassword->text();
}

QString ChangeWalletPasswordDialog::GetNewPassword() const
{
   return ui_->lineEditNewPassword->text();
}

void ChangeWalletPasswordDialog::PasswordTextChanged()
{
   ui_->pushButtonOk->setEnabled((!oldPasswordRequired_ || !ui_->lineEditOldPassword->text().isEmpty())
                                 && !ui_->lineEditNewPassword->text().isEmpty()
                                 && (ui_->lineEditNewPassword->text() == ui_->lineEditNewPasswordConfirm->text()));
}

#include "SetOTPPasswordDialog.h"
#include "ui_SetOTPPasswordDialog.h"

SetOTPPasswordDialog::SetOTPPasswordDialog(QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::SetOTPPasswordDialog())
{
   ui_->setupUi(this);

   connect(ui_->lineEditPass1, &QLineEdit::textEdited, this, &SetOTPPasswordDialog::passwordChanged);
   connect(ui_->lineEditPass2, &QLineEdit::textEdited, this, &SetOTPPasswordDialog::passwordChanged);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &SetOTPPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &SetOTPPasswordDialog::accept);

   passwordChanged();
}

void SetOTPPasswordDialog::passwordChanged()
{
   if (ui_->lineEditPass1->text().isEmpty()) {
      setStatusString(tr("Enter password for OTP protection"));
   } else if (ui_->lineEditPass2->text().isEmpty()) {
      setStatusString(tr("Please repeat password for OTP protection"));
   } else if (ui_->lineEditPass1->text() != ui_->lineEditPass2->text()) {
      setStatusString(tr("Password do not match"));
   } else {
      clearStatusString();
   }
}

void SetOTPPasswordDialog::setStatusString(const QString& status)
{
   ui_->labelStatusLabel->setText(status);
   ui_->pushButtonOk->setEnabled(false);
   ui_->labelStatusLabel->setVisible(true);
}

void SetOTPPasswordDialog::clearStatusString()
{
   ui_->labelStatusLabel->setVisible(false);
   ui_->pushButtonOk->setEnabled(true);
}

QString SetOTPPasswordDialog::GetPassword()
{
   return ui_->lineEditPass1->text();
}

#include "EnterOTPPasswordDialog.h"
#include "ui_EnterOTPPasswordDialog.h"

EnterOTPPasswordDialog::EnterOTPPasswordDialog(const QString& reason, QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::EnterOTPPasswordDialog())
{
   ui_->setupUi(this);

   connect(ui_->lineEditPassword, &QLineEdit::textEdited, this, &EnterOTPPasswordDialog::passwordChanged);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterOTPPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterOTPPasswordDialog::accept);

   ui_->pushButtonOk->setEnabled(false);

   ui_->labelReason->setText(reason);
}


QString EnterOTPPasswordDialog::GetPassword()
{
   return ui_->lineEditPassword->text();
}

void EnterOTPPasswordDialog::passwordChanged()
{
   ui_->pushButtonOk->setEnabled(!ui_->lineEditPassword->text().isEmpty());
}

#include "EnterWalletPassword.h"
#include "ui_EnterWalletPassword.h"

EnterWalletPassword::EnterWalletPassword(const QString& walletName, const QString &prompt, QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::EnterWalletPassword())
{
   ui_->setupUi(this);

   ui_->labelWalletName->setText(walletName);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterWalletPassword::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterWalletPassword::reject);

   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &EnterWalletPassword::PasswordChanged);

   ui_->pushButtonOk->setEnabled(false);

   ui_->progressBar->setMaximum(timeLeft_ * 100);
   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, [this] {
      timeLeft_ -= 0.5;
      if (timeLeft_ <= 0) {
         reject();
      }
      else {
         ui_->progressBar->setFormat(tr("%1 seconds left").arg((int)timeLeft_));
         ui_->progressBar->setValue(timeLeft_ * 100);
      }
   });
   timer_.start();
}

QString EnterWalletPassword::GetPassword() const
{
   return ui_->lineEditPassword->text();
}
void EnterWalletPassword::PasswordChanged()
{
   ui_->pushButtonOk->setEnabled(!GetPassword().isEmpty());
}

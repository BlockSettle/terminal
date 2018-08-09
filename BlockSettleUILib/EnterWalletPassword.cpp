#include "EnterWalletPassword.h"
#include "ui_EnterWalletPassword.h"
#include <spdlog/spdlog.h>


EnterWalletPassword::EnterWalletPassword(const QString& walletName, const std::string &walletId
   , bs::wallet::EncryptionType encType, const SecureBinaryData &encKey, const QString &prompt
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterWalletPassword())
   , frejaSign_(spdlog::get(""))
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
   if (encType == bs::wallet::EncryptionType::Password) {
      timer_.start();
   }
   else if (encType == bs::wallet::EncryptionType::Freja) {
      ui_->label->setText(tr("Freja eID authentication in progress"));
      ui_->lineEditPassword->hide();
      connect(&frejaSign_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
         password_ = password;
         accept();
      });
      connect(&frejaSign_, &FrejaSign::failed, [this](const QString &) { reject(); });
      connect(&frejaSign_, &FrejaSign::statusUpdated, [this](const QString &status) {
         ui_->label->setText(tr("Freja eID auth: %1").arg(status));
      });

      const auto title = tr("Outgoing Transaction");
      frejaSign_.start(QString::fromStdString(encKey.toBinStr()), title, walletId);
      timer_.start();
   }
}

void EnterWalletPassword::PasswordChanged()
{
   ui_->pushButtonOk->setEnabled(!ui_->lineEditPassword->text().isEmpty());
   password_ = ui_->lineEditPassword->text().toStdString();
}

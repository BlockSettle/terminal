#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"
#include <spdlog/spdlog.h>
#include "HDWallet.h"


ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
      , bs::wallet::EncryptionType encType
      , const SecureBinaryData &encKey
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , wallet_(wallet)
   , encType_(encType)
   , encKey_(encKey)
   , frejaSignOld_(spdlog::get(""))
   , frejaSignNew_(spdlog::get(""))
{
   ui_->setupUi(this);

   ui_->labelWalletName->setText(QString::fromStdString(wallet->getName()));
   ui_->lineEditOldPassword->setEnabled(encType_ == bs::wallet::EncryptionType::Password);

   if (!encKey_.isNull()) {
      ui_->lineEditOldPassword->setText(QString::fromStdString(encKey_.toBinStr()));
   }

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);

   connect(ui_->lineEditOldPassword, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);
   connect(ui_->lineEditNewPassword, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);
   connect(ui_->lineEditNewPasswordConfirm, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::PasswordTextChanged);

   ui_->pushButtonOk->setEnabled(false);

   ui_->lineEditFreja->hide();
   ui_->labelFreja->hide();

   connect(ui_->lineEditFreja, &QLineEdit::textChanged, this, &ChangeWalletPasswordDialog::FrejaIdChanged);
   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &ChangeWalletPasswordDialog::EncTypeSelectionClicked);
   connect(ui_->radioButtonFreja, &QRadioButton::clicked, this, &ChangeWalletPasswordDialog::EncTypeSelectionClicked);

   if (encType == bs::wallet::EncryptionType::Freja) {
      connect(&frejaSignOld_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
         oldPassword_ = password;
         if (ui_->radioButtonPassword->isChecked()) {
            PasswordTextChanged();
         }
         else {
            FrejaIdChanged();
         }
      });
      connect(&frejaSignOld_, &FrejaSign::failed, [this](const QString &) { emit reject(); });

      const auto title = tr("Current password for wallet %1").arg(QString::fromStdString(wallet_->getName()));
      frejaSignOld_.start(QString::fromStdString(encKey.toBinStr()), title, wallet_->getWalletId());
   }

   connect(&frejaSignNew_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
      newPassword_ = password;
      QDialog::accept();
   });
   connect(&frejaSignNew_, &FrejaSign::failed, [this](const QString &) {
      newPassword_.clear();
      reject();
   });
   connect(&frejaSignNew_, &FrejaSign::statusUpdated, [this](const QString &status) {
      ui_->labelFreja->setText(status);
   });
}

bs::wallet::EncryptionType ChangeWalletPasswordDialog::GetNewEncryptionType() const
{
   return ui_->radioButtonPassword->isChecked() ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Freja;
}

SecureBinaryData ChangeWalletPasswordDialog::GetNewEncryptionKey() const
{
   return ui_->lineEditFreja->text().toStdString();
}

void ChangeWalletPasswordDialog::PasswordTextChanged()
{
   ui_->pushButtonOk->setEnabled(((encType_ != bs::wallet::EncryptionType::Unencrypted) || !ui_->lineEditOldPassword->text().isEmpty()) || !oldPassword_.isNull()
                                 && !ui_->lineEditNewPassword->text().isEmpty()
                                 && (ui_->lineEditNewPassword->text() == ui_->lineEditNewPasswordConfirm->text()));
}

void ChangeWalletPasswordDialog::FrejaIdChanged()
{
   ui_->pushButtonOk->setEnabled(((encType_ != bs::wallet::EncryptionType::Unencrypted) || !ui_->lineEditOldPassword->text().isEmpty()) || !oldPassword_.isNull()
                                 && !ui_->lineEditFreja->text().isEmpty());
}

void ChangeWalletPasswordDialog::EncTypeSelectionClicked()
{
   newPassword_.clear();
   if (ui_->radioButtonPassword->isChecked()) {
      ui_->lineEditFreja->hide();
      ui_->labelFreja->hide();
      ui_->lineEditNewPassword->show();
      ui_->lineEditNewPasswordConfirm->show();
      ui_->labelNewPass->show();
      ui_->labelConfirm->show();
      ui_->lineEditNewPassword->clear();
      ui_->lineEditNewPasswordConfirm->clear();
   }
   else if (ui_->radioButtonFreja->isChecked()) {
      ui_->lineEditNewPassword->hide();
      ui_->lineEditNewPasswordConfirm->hide();
      ui_->labelNewPass->hide();
      ui_->labelConfirm->hide();
      ui_->lineEditFreja->show();
      ui_->labelFreja->show();
      ui_->labelFreja->setText(tr("Freja ID"));
      ui_->lineEditFreja->clear();
   }
}

void ChangeWalletPasswordDialog::accept()
{
   if (encType_ == bs::wallet::EncryptionType::Password) {
      oldPassword_ = ui_->lineEditOldPassword->text().toStdString();
   }

   if (ui_->radioButtonFreja->isChecked()) {
      ui_->lineEditFreja->setEnabled(false);
      ui_->pushButtonOk->setEnabled(false);
      ui_->radioButtonPassword->setEnabled(false);
      ui_->radioButtonFreja->setEnabled(false);
      const auto title = tr("New password for wallet %1").arg(QString::fromStdString(wallet_->getName()));
      frejaSignNew_.start(ui_->lineEditFreja->text(), title, wallet_->getWalletId());
   }
   else {
      newPassword_ = ui_->lineEditNewPassword->text().toStdString();
      QDialog::accept();
   }
}

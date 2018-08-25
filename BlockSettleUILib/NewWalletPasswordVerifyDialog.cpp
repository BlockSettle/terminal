#include "NewWalletPasswordVerifyDialog.h"

#include "MessageBoxCritical.h"
#include "WalletKeysSubmitFrejaDialog.h"
#include "ui_NewWalletPasswordVerifyDialog.h"

NewWalletPasswordVerifyDialog::NewWalletPasswordVerifyDialog(const std::string& walletId
   , const std::vector<bs::wallet::PasswordData>& keys, bs::wallet::KeyRank keyRank
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::NewWalletPasswordVerifyDialog)
   , walletId_(walletId)
   , keys_(keys)
   , keyRank_(keyRank)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &NewWalletPasswordVerifyDialog::onContinueClicked);

   const bs::wallet::PasswordData &key = keys.at(0);

   if (key.encType == bs::wallet::EncryptionType::Freja) {
      initFreja(QString::fromStdString(key.encKey.toBinStr()));
   } else {
      initPassword();
   }
}

NewWalletPasswordVerifyDialog::~NewWalletPasswordVerifyDialog() = default;

void NewWalletPasswordVerifyDialog::initPassword()
{
   ui_->stackedWidget->setCurrentIndex(Pages::Check);
   ui_->lineEditFrejaId->hide();
   ui_->labelFrejaHint->hide();
   adjustSize();
}

void NewWalletPasswordVerifyDialog::initFreja(const QString& frejaId)
{
   ui_->stackedWidget->setCurrentIndex(Pages::FrejaInfo);
   ui_->lineEditFrejaId->setText(frejaId);
   ui_->lineEditPassword->hide();
   ui_->labelPasswordHint->hide();
   adjustSize();
}

void NewWalletPasswordVerifyDialog::onContinueClicked()
{
   Pages currentPage = Pages(ui_->stackedWidget->currentIndex());
   
   if (currentPage == FrejaInfo) {
      ui_->stackedWidget->setCurrentIndex(Pages::Check);
      return;
   }

   const bs::wallet::PasswordData &key = keys_.at(0);

   if (key.encType == bs::wallet::EncryptionType::Password) {
      if (ui_->lineEditPassword->text().toStdString() != key.password.toBinStr()) {
         MessageBoxCritical errorMessage(tr("Error"), tr("Password does not match. Please try again."), this);
         errorMessage.exec();
         return;
      }
   }
   
   if (key.encType == bs::wallet::EncryptionType::Freja) {
      std::vector<bs::wallet::EncryptionType> encTypes;
      std::vector<SecureBinaryData> encKeys;
      for (const bs::wallet::PasswordData& key : keys_) {
         encTypes.push_back(key.encType);
         encKeys.push_back(key.encKey);
      }

      WalletKeysSubmitFrejaDialog dialog(walletId_, keyRank_, encTypes, encKeys
         , tr("Activate Freja eID signing"), this);
      int result = dialog.exec();
      if (!result) {
         return;
      }
   }

   accept();
}

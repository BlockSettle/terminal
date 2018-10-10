#include "WalletPasswordVerifyDialog.h"

#include "EnterWalletPassword.h"
#include "MessageBoxCritical.h"
#include "ui_WalletPasswordVerifyDialog.h"

WalletPasswordVerifyDialog::WalletPasswordVerifyDialog(const std::string& walletId
   , const std::vector<bs::wallet::PasswordData>& keys
   , bs::wallet::KeyRank keyRank
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletPasswordVerifyDialog)
   , walletId_(walletId)
   , keys_(keys)
   , keyRank_(keyRank)
   , appSettings_(appSettings)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &WalletPasswordVerifyDialog::onContinueClicked);

   const bs::wallet::PasswordData &key = keys.at(0);

   if (key.encType == bs::wallet::EncryptionType::Freja) {
      initFreja(QString::fromStdString(key.encKey.toBinStr()));
   } else {
      initPassword();
   }
}

WalletPasswordVerifyDialog::~WalletPasswordVerifyDialog() = default;

void WalletPasswordVerifyDialog::initPassword()
{
   ui_->labelFrejaHint->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::initFreja(const QString&)
{
   ui_->lineEditPassword->hide();
   ui_->labelPasswordHint->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::onContinueClicked()
{
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

      EnterWalletPassword dialog(walletId_, appSettings_, keyRank_, encTypes, encKeys
         , tr("Confirm Freja eID signing"), tr("Confirm Wallet"), this);
      int result = dialog.exec();
      if (!result) {
         return;
      }
   }

   accept();
}

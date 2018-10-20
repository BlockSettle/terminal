#include "WalletPasswordVerifyDialog.h"

#include "EnterWalletPassword.h"
#include "MessageBoxCritical.h"
#include "ui_WalletPasswordVerifyDialog.h"

WalletPasswordVerifyDialog::WalletPasswordVerifyDialog(const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletPasswordVerifyDialog)
   , appSettings_(appSettings)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &WalletPasswordVerifyDialog::onContinueClicked);
}

WalletPasswordVerifyDialog::~WalletPasswordVerifyDialog() = default;

void WalletPasswordVerifyDialog::init(const std::string& walletId
   , const std::vector<bs::wallet::PasswordData>& keys
   , bs::wallet::KeyRank keyRank)
{
   walletId_ = walletId;
   keys_ = keys;
   keyRank_ = keyRank;
      
   const bs::wallet::PasswordData &key = keys.at(0);

   if (key.encType == bs::wallet::EncryptionType::Freja) {
      initFreja(QString::fromStdString(key.encKey.toBinStr()));
   }
   else {
      initPassword();
   }

   runPasswordCheck_ = true;
}

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
      EnterWalletPassword dialog(MobileClientRequest::VerifyWalletKey, this);
      dialog.init(walletId_, keyRank_, keys_, appSettings_, tr("Activate Freja eID signing"));
      int result = dialog.exec();
      if (!result) {
         return;
      }
   }

   accept();
}

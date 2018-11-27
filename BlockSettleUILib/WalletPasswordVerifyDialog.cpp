#include "WalletPasswordVerifyDialog.h"

#include "EnterWalletPassword.h"
#include "BSMessageBox.h"
#include "ui_WalletPasswordVerifyDialog.h"

WalletPasswordVerifyDialog::WalletPasswordVerifyDialog(const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletPasswordVerifyDialog)
   , appSettings_(appSettings)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &WalletPasswordVerifyDialog::onContinueClicked);
   connect(ui_->lineEditPassword, &QLineEdit::textEdited, this, [=](const QString &text) {
      ui_->pushButtonContinue->setDisabled(text.isEmpty());
   });

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

   if (key.encType == bs::wallet::EncryptionType::Auth) {
      initAuth(QString::fromStdString(key.encKey.toBinStr()));
   }
   else {
      initPassword();
   }

   runPasswordCheck_ = true;
}

void WalletPasswordVerifyDialog::initPassword()
{
   ui_->pushButtonContinue->setEnabled(false);
   ui_->labelAuthHint->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::initAuth(const QString&)
{
   ui_->lineEditPassword->hide();
   ui_->labelPasswordHint->hide();
   ui_->groupPassword->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::onContinueClicked()
{
   const bs::wallet::PasswordData &key = keys_.at(0);

   if (key.encType == bs::wallet::EncryptionType::Password) {
      if (ui_->lineEditPassword->text().toStdString() != key.password.toBinStr()) {
         BSMessageBox errorMessage(BSMessageBox::critical, tr("Warning")
            , tr("Incorrect password"), tr("The password you have entered is incorrect. Please try again."), this);
         errorMessage.exec();
         return;
      }
   }
   
   if (key.encType == bs::wallet::EncryptionType::Auth) {
      EnterWalletPassword dialog(MobileClientRequest::VerifyWalletKey, this);
      dialog.init(walletId_, keyRank_, keys_, appSettings_, tr("Confirm Auth eID Signing"), tr("Auth eID"));
      int result = dialog.exec();
      if (!result) {
         return;
      }
   }

   accept();
}

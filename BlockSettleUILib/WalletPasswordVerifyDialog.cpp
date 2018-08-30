#include "WalletPasswordVerifyDialog.h"

#include "EnterWalletPassword.h"
#include "MessageBoxCritical.h"
#include "ui_WalletPasswordVerifyDialog.h"

WalletPasswordVerifyDialog::WalletPasswordVerifyDialog(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletPasswordVerifyDialog)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &WalletPasswordVerifyDialog::onContinueClicked);

   ui_->stackedWidget->setCurrentIndex(Pages::FrejaInfo);
}

WalletPasswordVerifyDialog::~WalletPasswordVerifyDialog() = default;

void WalletPasswordVerifyDialog::init(const std::string& walletId, const std::vector<bs::wallet::PasswordData>& keys
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
   ui_->stackedWidget->setCurrentIndex(Pages::Check);
   ui_->lineEditFrejaId->hide();
   ui_->labelFrejaHint->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::initFreja(const QString& frejaId)
{
   ui_->stackedWidget->setCurrentIndex(Pages::FrejaInfo);
   ui_->lineEditFrejaId->setText(frejaId);
   ui_->lineEditPassword->hide();
   ui_->labelPasswordHint->hide();
   adjustSize();
}

void WalletPasswordVerifyDialog::onContinueClicked()
{
   if (!runPasswordCheck_) {
      accept();
      return;
   }

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
      EnterWalletPassword dialog(this);
      dialog.init(walletId_, keyRank_, keys_, tr("Activate Freja eID signing"));
      int result = dialog.exec();
      if (!result) {
         return;
      }
   }

   accept();
}

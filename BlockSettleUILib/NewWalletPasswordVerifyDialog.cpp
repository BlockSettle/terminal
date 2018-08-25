#include "NewWalletPasswordVerifyDialog.h"

#include "MessageBoxCritical.h"
#include "ui_NewWalletPasswordVerifyDialog.h"

NewWalletPasswordVerifyDialog::NewWalletPasswordVerifyDialog(const std::vector<bs::wallet::PasswordData>& keys
   , QWidget *parent) :
   QDialog(parent)
   , ui_(new Ui::NewWalletPasswordVerifyDialog)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &NewWalletPasswordVerifyDialog::onContinueClicked);

   const bs::wallet::PasswordData &key = keys.at(0);

   if (key.encType == bs::wallet::EncryptionType::Freja) {
      initFreja(QString::fromStdString(key.encKey.toBinStr()));
   } else {
      password_ = key.password;
      initPassword();
   }
}

NewWalletPasswordVerifyDialog::~NewWalletPasswordVerifyDialog() = default;

void NewWalletPasswordVerifyDialog::initPassword()
{
   encryptionType_ = bs::wallet::EncryptionType::Password;
   ui_->stackedWidget->setCurrentIndex(Pages::Check);
   ui_->lineEditFrejaId->hide();
   ui_->labelFrejaHint->hide();
   adjustSize();
}

void NewWalletPasswordVerifyDialog::initFreja(const QString& frejaId)
{
   encryptionType_ = bs::wallet::EncryptionType::Freja;
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

   if (ui_->lineEditPassword->text().toStdString() != password_.toBinStr()) {
      MessageBoxCritical errorMessage(tr("Error"), tr("Password does not match. Please try again."), this);
      errorMessage.exec();
      return;
   }

   accept();
}

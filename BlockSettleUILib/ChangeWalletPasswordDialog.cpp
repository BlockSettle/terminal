#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"
#include <spdlog/spdlog.h>
#include "HDWallet.h"


ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys
      , bs::hd::KeyRank keyRank
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , wallet_(wallet)
{
   ui_->setupUi(this);

   ui_->labelWalletName->setText(QString::fromStdString(wallet->getName()));

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);

   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { QApplication::processEvents(); adjustSize(); });
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateState(); });
   ui_->widgetCreateKeys->init(wallet_->getWalletId());

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   ui_->widgetSubmitKeys->init(wallet_->getWalletId(), keyRank, encTypes, encKeys);
   ui_->widgetSubmitKeys->setFocus();

   updateState();
}

void ChangeWalletPasswordDialog::showEvent(QShowEvent *evt)
{
   QApplication::processEvents();
   adjustSize();
   QDialog::showEvent(evt);
}

void ChangeWalletPasswordDialog::updateState()
{
   ui_->pushButtonOk->setEnabled(ui_->widgetSubmitKeys->isValid() && ui_->widgetCreateKeys->isValid());
}

void ChangeWalletPasswordDialog::reject()
{
   ui_->widgetSubmitKeys->cancel();
   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}

std::vector<bs::hd::PasswordData> ChangeWalletPasswordDialog::newPasswordData() const
{
   return ui_->widgetCreateKeys->keys();
}

bs::hd::KeyRank ChangeWalletPasswordDialog::newKeyRank() const
{
   return ui_->widgetCreateKeys->keyRank();
}

SecureBinaryData ChangeWalletPasswordDialog::oldPassword() const
{
   return ui_->widgetSubmitKeys->key();
}

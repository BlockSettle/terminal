#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

#include <spdlog/spdlog.h>
#include "HDWallet.h"
#include "EnterWalletPassword.h"


ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys
      , bs::wallet::KeyRank keyRank
      , const QString& username
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , wallet_(wallet)
{
   ui_->setupUi(this);

   ui_->labelWalletId->setText(QString::fromStdString(wallet->getWalletId()));

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::onContinueClicked);

   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateState(); });

   ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideGroupboxCaption 
      | WalletKeysSubmitWidget::SetPasswordLabelAsOld);
   ui_->widgetSubmitKeys->init(wallet_->getWalletId(), keyRank, encTypes, encKeys);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideFrejaConnectButton 
      | WalletKeysCreateWidget::HideWidgetContol
      | WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew);
   ui_->widgetCreateKeys->init(wallet_->getWalletId(), username);

   ui_->widgetSubmitKeys->setFocus();

   //updateState();
}

ChangeWalletPasswordDialog::~ChangeWalletPasswordDialog() = default;

//void ChangeWalletPasswordDialog::updateState()
//{
//   ui_->pushButtonOk->setEnabled(ui_->widgetSubmitKeys->isValid() && ui_->widgetCreateKeys->isValid());
//}

void ChangeWalletPasswordDialog::reject()
{
   ui_->widgetSubmitKeys->cancel();
   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}

void ChangeWalletPasswordDialog::onContinueClicked()
{
   std::vector<bs::wallet::PasswordData> keys = ui_->widgetCreateKeys->keys();
   
   if (!keys.empty() && keys.at(0).encType == bs::wallet::EncryptionType::Freja) {
      EnterWalletPassword enterWalletPassword(this);
      enterWalletPassword.init(wallet_->getWalletId(), ui_->widgetCreateKeys->keyRank()
         , keys, tr("Activate Freja eID signing"));
      int result = enterWalletPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }

      keys[0].password = enterWalletPassword.GetPassword();
   }

   newPasswordData_ = keys;
   accept();
}

std::vector<bs::wallet::PasswordData> ChangeWalletPasswordDialog::newPasswordData() const
{
   return newPasswordData_;
}

bs::wallet::KeyRank ChangeWalletPasswordDialog::newKeyRank() const
{
   return ui_->widgetCreateKeys->keyRank();
}

SecureBinaryData ChangeWalletPasswordDialog::oldPassword() const
{
   return ui_->widgetSubmitKeys->key();
}

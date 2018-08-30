#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

#include <spdlog/spdlog.h>
#include "EnterWalletPassword.h"
#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "WalletPasswordVerifyDialog.h"


ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys
      , bs::wallet::KeyRank keyRank
      , const QString& username
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , wallet_(wallet)
   , oldKeyRank_(keyRank)
{
   ui_->setupUi(this);

   ui_->labelWalletId->setText(QString::fromStdString(wallet->getWalletId()));

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::onContinueClicked);

   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateState(); });

   auto encTypesIt = encTypes.begin();
   auto encKeysIt = encKeys.begin();
   while (encTypesIt != encTypes.end() && encKeysIt != encKeys.end()) {
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = *encTypesIt;
      passwordData.encKey = *encKeysIt;
      oldPasswordData_.push_back(passwordData);
      ++encTypesIt;
      ++encKeysIt;
   }

   ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideGroupboxCaption 
      | WalletKeysSubmitWidget::SetPasswordLabelAsOld
      | WalletKeysSubmitWidget::HideFrejaConnectButton);
   ui_->widgetSubmitKeys->suspend();
   ui_->widgetSubmitKeys->init(wallet_->getWalletId(), keyRank, encTypes, encKeys);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideFrejaConnectButton 
      | WalletKeysCreateWidget::HideWidgetContol
      | WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew);
   ui_->widgetCreateKeys->init(wallet_->getWalletId(), username);

   ui_->widgetSubmitKeys->setFocus();
}

ChangeWalletPasswordDialog::~ChangeWalletPasswordDialog() = default;

void ChangeWalletPasswordDialog::reject()
{
   ui_->widgetSubmitKeys->cancel();
   ui_->widgetCreateKeys->cancel();
   QDialog::reject();
}

void ChangeWalletPasswordDialog::onContinueClicked()
{
   std::vector<bs::wallet::PasswordData> newKeys = ui_->widgetCreateKeys->keys();

   bool isOldFreja = !oldPasswordData_.empty() && oldPasswordData_[0].encType == bs::wallet::EncryptionType::Freja;
   bool isNewFreja = !newKeys.empty() && newKeys[0].encType == bs::wallet::EncryptionType::Freja;

   if (!ui_->widgetSubmitKeys->isValid() && !isOldFreja) {
      MessageBoxCritical messageBox(tr("Invalid password"), tr("Please check old password"), this);
      messageBox.exec();
      return;
   }

   if (!ui_->widgetCreateKeys->isValid() && !isNewFreja) {
      MessageBoxCritical messageBox(tr("Invalid passwords"), tr("Please check new passwords"), this);
      messageBox.exec();
      return;
   }

   bool showFrejaUsageInfo = true;
   
   if (isOldFreja)
   {
      showFrejaUsageInfo = false;

      if (oldPasswordData_[0].password.isNull()) {
         EnterWalletPassword enterWalletPassword(this);
         enterWalletPassword.init(wallet_->getWalletId(), oldKeyRank_
            , oldPasswordData_, tr("Change Password"));
         int result = enterWalletPassword.exec();
         if (result != QDialog::Accepted) {
            return;
         }

         oldKey_ = enterWalletPassword.GetPassword();
      }
   } else {
      oldKey_ = ui_->widgetSubmitKeys->key();
   }
   
   if (isNewFreja) {
      if (showFrejaUsageInfo) {
         WalletPasswordVerifyDialog walletPasswordVerifyDialog(this);
         int result = walletPasswordVerifyDialog.exec();
         if (result != QDialog::Accepted) {
            return;
         }
      }

      EnterWalletPassword enterWalletPassword(this);
      enterWalletPassword.init(wallet_->getWalletId(), ui_->widgetCreateKeys->keyRank()
         , newKeys, tr("Activate Freja eID signing"));
      int result = enterWalletPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }

      newKeys[0].password = enterWalletPassword.GetPassword();
   }

   newPasswordData_ = newKeys;
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
   return oldKey_;
}

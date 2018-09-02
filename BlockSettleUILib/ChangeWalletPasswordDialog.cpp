#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

#include <spdlog/spdlog.h>
#include "EnterWalletPassword.h"
#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "WalletKeyWidget.h"
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

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &ChangeWalletPasswordDialog::onTabChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::onContinueClicked);

   deviceKeyOld_ = new WalletKeyWidget(wallet->getWalletId(), 0, false, this);
   deviceKeyOld_->setFixedType(true);
   deviceKeyOld_->setEncryptionKeys(encKeys);
   deviceKeyOld_->setHideFrejaConnect(true);
   deviceKeyOld_->setHideFrejaCombobox(true);
   deviceKeyNew_ = new WalletKeyWidget(wallet->getWalletId(), 0, false, this);
   deviceKeyNew_->setFixedType(true);
   deviceKeyNew_->setEncryptionKeys(encKeys);
   deviceKeyNew_->setHideFrejaConnect(true);
   deviceKeyNew_->setHideFrejaCombobox(true);

   QBoxLayout *deviceLayout = dynamic_cast<QBoxLayout*>(ui_->tabAddDevice->layout());
   deviceLayout->insertWidget(deviceLayout->indexOf(ui_->labelDeviceOldInfo) + 1, deviceKeyOld_);
   deviceLayout->insertWidget(deviceLayout->indexOf(ui_->labelDeviceNewInfo) + 1, deviceKeyNew_);

   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateState(); });
   connect(deviceKeyOld_, &WalletKeyWidget::keyChanged, this, &ChangeWalletPasswordDialog::onSubmitKeysKeyChanged2);
   connect(deviceKeyOld_, &WalletKeyWidget::failed, this, &ChangeWalletPasswordDialog::onSubmitKeysFailed2);
   connect(deviceKeyNew_, &WalletKeyWidget::keyChanged, this, &ChangeWalletPasswordDialog::onCreateKeysKeyChanged2);
   connect(deviceKeyNew_, &WalletKeyWidget::failed, this, &ChangeWalletPasswordDialog::onCreateKeysFailed2);

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

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew
      | WalletKeysCreateWidget::HideFrejaConnectButton
      | WalletKeysCreateWidget::HideWidgetContol);
   ui_->widgetCreateKeys->init(wallet_->getWalletId(), username);

   ui_->widgetSubmitKeys->setFocus();

   ui_->tabWidget->setCurrentIndex(int(Pages::Basic));

   updateState();
}

ChangeWalletPasswordDialog::~ChangeWalletPasswordDialog() = default;

void ChangeWalletPasswordDialog::reject()
{
   ui_->widgetSubmitKeys->cancel();
   ui_->widgetCreateKeys->cancel();
   deviceKeyOld_->cancel();
   deviceKeyNew_->cancel();
   QDialog::reject();
}

void ChangeWalletPasswordDialog::onContinueClicked()
{
   if (ui_->tabWidget->currentIndex() == int(Pages::Basic)) {
      continueBasic();
   } else {
      continueAddDevice();
   }
}

void ChangeWalletPasswordDialog::continueBasic()
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

   if (isOldFreja && isNewFreja && oldPasswordData_[0].encKey == newKeys[0].encKey) {
      MessageBoxCritical messageBox(tr("Invalid new Freja eID")
         , tr("Please use different Freja eID. New Freje eID is already used."), this);
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
   }
   else {
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
   newKeyRank_ = ui_->widgetCreateKeys->keyRank();
   accept();
}

void ChangeWalletPasswordDialog::continueAddDevice()
{
   if (state_ != State::Idle) {
      return;
   }

   if (!deviceKeyOldValid_) {
      deviceKeyOld_->start();
      state_ = State::AddDeviceWaitOld;
   } else {
      deviceKeyNew_->start();
      state_ = State::AddDeviceWaitNew;
   }

   updateState();
}

void ChangeWalletPasswordDialog::onSubmitKeysKeyChanged2(int, SecureBinaryData password)
{
   deviceKeyOldValid_ = true;
   state_ = State::AddDeviceWaitNew;
   deviceKeyNew_->start();
   oldKey_ = password;
   updateState();
}

void ChangeWalletPasswordDialog::onSubmitKeysFailed2()
{
   state_ = State::Idle;
   updateState();
}

void ChangeWalletPasswordDialog::onCreateKeysKeyChanged2(int, SecureBinaryData password)
{
   newPasswordData_ = oldPasswordData_;
   
   newPasswordData_.at(0).password = oldKey_;

   bs::wallet::PasswordData newPassword{};
   newPassword.encType = bs::wallet::EncryptionType::Freja;
   newPassword.encKey = newPasswordData_.at(0).encKey;
   newPassword.password = password;
   newPasswordData_.push_back(newPassword);

   newKeyRank_ = oldKeyRank_;
   newKeyRank_.second = newPasswordData_.size();

   accept();
}

void ChangeWalletPasswordDialog::onCreateKeysFailed2()
{
   state_ = State::Idle;
   updateState();
}

void ChangeWalletPasswordDialog::onTabChanged(int index)
{
   state_ = State::Idle;
   updateState();
}

std::vector<bs::wallet::PasswordData> ChangeWalletPasswordDialog::newPasswordData() const
{
   return newPasswordData_;
}

bs::wallet::KeyRank ChangeWalletPasswordDialog::newKeyRank() const
{
   return newKeyRank_;
}

SecureBinaryData ChangeWalletPasswordDialog::oldPassword() const
{
   return oldKey_;
}

void ChangeWalletPasswordDialog::updateState()
{
   Pages currentPage = Pages(ui_->tabWidget->currentIndex());

   if (currentPage == Pages::Basic) {
      ui_->pushButtonOk->setText(tr("Continue"));
   } else {
      ui_->pushButtonOk->setText(tr("Add Device"));
      bool isOldFreja = !oldPasswordData_.empty() && oldPasswordData_[0].encType == bs::wallet::EncryptionType::Freja;
      ui_->pushButtonOk->setEnabled(isOldFreja && state_ == State::Idle);
   }

   ui_->labelAddDeviceInfo->setVisible(state_ == State::Idle);
   ui_->labelDeviceOldInfo->setVisible(state_ == State::AddDeviceWaitOld || state_ == State::AddDeviceWaitNew);
   ui_->labelAddDeviceSuccess->setVisible(state_ == State::AddDeviceWaitNew);
   ui_->labelDeviceNewInfo->setVisible(state_ == State::AddDeviceWaitNew);
   ui_->lineAddDevice->setVisible(state_ == State::AddDeviceWaitNew);
   deviceKeyOld_->setVisible(state_ == State::AddDeviceWaitOld);
   deviceKeyNew_->setVisible(state_ == State::AddDeviceWaitNew);
}

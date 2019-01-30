#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

#include <spdlog/spdlog.h>
#include <QToolButton>
#include "ApplicationSettings.h"
#include "EnterWalletPassword.h"
#include "HDWallet.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "WalletKeyWidget.h"
#include "WalletKeysDeleteDevice.h"


ChangeWalletPasswordDialog::ChangeWalletPasswordDialog(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<SignContainer> signingContainer
      , const std::shared_ptr<bs::hd::Wallet> &wallet
      , const std::vector<bs::wallet::EncryptionType> &encTypes
      , const std::vector<SecureBinaryData> &encKeys
      , bs::wallet::KeyRank keyRank
      , const QString& username
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , logger_(logger)
   , signingContainer_(signingContainer)
   , wallet_(wallet)
   , oldKeyRank_(keyRank)
   , appSettings_(appSettings)
{
   ui_->setupUi(this);

   resetKeys();

   ui_->labelWalletId->setText(QString::fromStdString(wallet->getWalletId()));

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &ChangeWalletPasswordDialog::onTabChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::onContinueClicked);

   connect(signingContainer_.get(), &SignContainer::PasswordChanged, this, &ChangeWalletPasswordDialog::onPasswordChanged);

   deviceKeyOld_ = new WalletKeyWidget(AutheIDClient::ActivateWalletOldDevice
      , wallet->getWalletId(), 0, false, appSettings->GetAuthKeys(), this);
   deviceKeyOld_->setFixedType(true);
   deviceKeyOld_->setEncryptionKeys(encKeys);
   deviceKeyOld_->setHideAuthConnect(true);
   deviceKeyOld_->setHideAuthCombobox(true);

   deviceKeyNew_ = new WalletKeyWidget(AutheIDClient::ActivateWalletNewDevice
      , wallet->getWalletId(), 0, false, appSettings->GetAuthKeys(), this);
   deviceKeyNew_->setFixedType(true);
   deviceKeyNew_->setEncryptionKeys(encKeys);
   deviceKeyNew_->setHideAuthConnect(true);
   deviceKeyNew_->setHideAuthCombobox(true);
   
   QBoxLayout *deviceLayout = dynamic_cast<QBoxLayout*>(ui_->tabAddDevice->layout());
   deviceLayout->insertWidget(deviceLayout->indexOf(ui_->labelDeviceOldInfo) + 1, deviceKeyOld_);
   deviceLayout->insertWidget(deviceLayout->indexOf(ui_->labelDeviceNewInfo) + 1, deviceKeyNew_);

   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyCountChanged, [this] { adjustSize(); });
   //connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::keyChanged, [this] { updateState(); });
   connect(deviceKeyOld_, &WalletKeyWidget::keyChanged, this, &ChangeWalletPasswordDialog::onOldDeviceKeyChanged);
   connect(deviceKeyOld_, &WalletKeyWidget::failed, this, &ChangeWalletPasswordDialog::onOldDeviceFailed);

   connect(deviceKeyNew_, &WalletKeyWidget::encKeyChanged, this, &ChangeWalletPasswordDialog::onNewDeviceEncKeyChanged);
   connect(deviceKeyNew_, &WalletKeyWidget::keyChanged, this, &ChangeWalletPasswordDialog::onNewDeviceKeyChanged);
   connect(deviceKeyNew_, &WalletKeyWidget::failed, this, &ChangeWalletPasswordDialog::onNewDeviceFailed);

   QString usernameAuthApp;


   int authCount = 0;
   for (const auto &encKey : encKeys) {   // assume we can have encKeys only for Auth type
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = bs::wallet::EncryptionType::Auth;
      passwordData.encKey = encKey;

      oldPasswordData_.push_back(passwordData);
      auto deviceInfo = AutheIDClient::getDeviceInfo(encKey.toBinStr());

      if (!deviceInfo.userId.empty()) {
         authCount += 1;

         usernameAuthApp = QString::fromStdString(deviceInfo.userId);

         QString deviceName;
         if (!deviceInfo.deviceName.empty()) {
            deviceName = QString::fromStdString(deviceInfo.deviceName);
         } else {
            deviceName = QString(tr("Device %1")).arg(authCount);
         }

         WalletKeysDeleteDevice *deviceWidget = new WalletKeysDeleteDevice(deviceName);

         ui_->verticalLayoutDeleteDevices->insertWidget(authCount - 1, deviceWidget);

         connect(deviceWidget, &WalletKeysDeleteDevice::deleteClicked, this, [this, deviceInfo] {
            deleteDevice(deviceInfo.deviceId);
         });
      }
   }

   for (const auto &encType : encTypes) {
      if (encType == bs::wallet::EncryptionType::Auth) {    // already added encKeys for Auth type
         continue;
      }
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = encType;
      oldPasswordData_.push_back(passwordData);
   }

   if (!usernameAuthApp.isEmpty()) {
      deviceKeyOld_->init(appSettings, usernameAuthApp);
      deviceKeyNew_->init(appSettings, usernameAuthApp);
   }

   ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideGroupboxCaption 
      | WalletKeysSubmitWidget::SetPasswordLabelAsOld
      | WalletKeysSubmitWidget::HideAuthConnectButton
      | WalletKeysSubmitWidget::HidePasswordWarning);
   ui_->widgetSubmitKeys->suspend();
   ui_->widgetSubmitKeys->init(AutheIDClient::DeactivateWallet, wallet_->getWalletId(), keyRank, encTypes, encKeys, appSettings);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew
      | WalletKeysCreateWidget::HideAuthConnectButton
      | WalletKeysCreateWidget::HideWidgetContol);
   ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet, wallet_->getWalletId(), username, appSettings);

   ui_->widgetSubmitKeys->setFocus();

   ui_->tabWidget->setCurrentIndex(int(Pages::Basic));

   updateState();
}

ChangeWalletPasswordDialog::~ChangeWalletPasswordDialog() = default;

void ChangeWalletPasswordDialog::accept()
{
   onContinueClicked();
}

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
   // Is this accurate? Shouldn't we wait until the change is confirmed?
   resetKeys();

   if (ui_->tabWidget->currentIndex() == int(Pages::Basic)) {
      continueBasic(); // Password
   } else {
      continueAddDevice(); // Auth eID
   }
}

// Change the wallet's password.
void ChangeWalletPasswordDialog::continueBasic()
{
   std::vector<bs::wallet::PasswordData> newKeys = ui_->widgetCreateKeys->keys();

   bool isOldAuth = !oldPasswordData_.empty() && oldPasswordData_[0].encType == bs::wallet::EncryptionType::Auth;
   bool isNewAuth = !newKeys.empty() && newKeys[0].encType == bs::wallet::EncryptionType::Auth;

   if (!ui_->widgetSubmitKeys->isValid() && !isOldAuth) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Invalid password"),
                                    tr("Please check old password."),
                                    this);
      messageBox.exec();
      return;
   }

   if (!ui_->widgetCreateKeys->isValid() && !isNewAuth) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Invalid password"),
                                    tr("Please check new password, and make " \
                                       "sure the length is at least six (6) " \
                                       "characters long."),
                                    this);
      messageBox.exec();
      return;
   }

   if (isOldAuth && isNewAuth) {
      bool sameAuthId = true;
      for (const auto &oldPassData : oldPasswordData_) {
         auto deviceInfo = AutheIDClient::getDeviceInfo(oldPassData.encKey.toBinStr());
         if (deviceInfo.userId != newKeys[0].encKey.toBinStr()) {
            sameAuthId = false;
         }
      }
      if (sameAuthId) {
         BSMessageBox messageBox(BSMessageBox::critical, tr("Invalid new Auth eID")
            , tr("Please use different Auth eID. Same Auth eID is already used."), this);
         messageBox.exec();
         return;
      }
   }

   bool showAuthUsageInfo = true;

   if (isOldAuth)
   {
      showAuthUsageInfo = false;

      if (oldPasswordData_[0].password.isNull()) {
         EnterWalletPassword enterWalletPassword(AutheIDClient::DeactivateWallet, this);
         enterWalletPassword.init(wallet_->getWalletId(), oldKeyRank_
            , oldPasswordData_, appSettings_, tr("Change Password"));
         int result = enterWalletPassword.exec();
         if (result != QDialog::Accepted) {
            return;
         }

         oldKey_ = enterWalletPassword.getPassword();
      }
   }
   else {
      oldKey_ = ui_->widgetSubmitKeys->key();
   }

   if (isNewAuth) {
      if (showAuthUsageInfo) {
         MessageBoxAuthNotice authNotice(this);
         int result = authNotice.exec();
         if (result != QDialog::Accepted) {
            return;
         }
      }

      EnterWalletPassword enterWalletPassword(AutheIDClient::ActivateWallet, this);
      enterWalletPassword.init(wallet_->getWalletId(), ui_->widgetCreateKeys->keyRank()
         , newKeys, appSettings_, tr("Activate Auth eID signing"));
      int result = enterWalletPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }

      newKeys[0].encKey = enterWalletPassword.getEncKey(0);
      newKeys[0].password = enterWalletPassword.getPassword();
   }

   newPasswordData_ = newKeys;
   newKeyRank_ = ui_->widgetCreateKeys->keyRank();
   changePassword();
}

// Add a new Auth eID device to the wallet.
void ChangeWalletPasswordDialog::continueAddDevice()
{
   if (state_ != State::Idle) {
      return;
   }

   if (oldKeyRank_.first != 1) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Add Device error")
         , tr("Only 1-of-N AuthApp encryption supported"), this);
      messageBox.exec();
      return;
   }

   if (oldPasswordData_.empty() || oldPasswordData_[0].encType != bs::wallet::EncryptionType::Auth) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Add Device")
         , tr("Auth eID encryption"), tr("Auth eID is not enabled"), this);
      messageBox.exec();
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

void ChangeWalletPasswordDialog::changePassword()
{
   if (wallet_->isWatchingOnly()) {
      signingContainer_->ChangePassword(wallet_, newPasswordData_, newKeyRank_, oldKey_
         , addNew_, removeOld_, false);
   }
   else {
      bool result = wallet_->changePassword(newPasswordData_, newKeyRank_, oldKey_
         , addNew_, removeOld_, false);
      onPasswordChanged(wallet_->getWalletId(), result);
   }
}

void ChangeWalletPasswordDialog::resetKeys()
{
   oldKey_.clear();
   deviceKeyOldValid_ = false;
   deviceKeyNewValid_ = false;
   isLatestChangeAddDevice_ = false;
   addNew_ = false;
   removeOld_ = false;
}

void ChangeWalletPasswordDialog::onOldDeviceKeyChanged(int, SecureBinaryData password)
{
   deviceKeyOldValid_ = true;
   state_ = State::AddDeviceWaitNew;
   deviceKeyNew_->start();
   oldKey_ = password;
   updateState();
}

void ChangeWalletPasswordDialog::onOldDeviceFailed()
{
   state_ = State::Idle;
   updateState();

   BSMessageBox(BSMessageBox::critical, tr("Wallet Encryption")
      , tr("A problem occured requesting old device key")
      , this).exec();
}

void ChangeWalletPasswordDialog::onNewDeviceEncKeyChanged(int index, SecureBinaryData encKey)
{
   bs::wallet::PasswordData newPassword{};
   newPassword.encType = bs::wallet::EncryptionType::Auth;
   newPassword.encKey = encKey;
   newPasswordData_.clear();
   newPasswordData_.push_back(newPassword);
}

void ChangeWalletPasswordDialog::onNewDeviceKeyChanged(int index, SecureBinaryData password)
{
   if (newPasswordData_.empty()) {
      logger_->error("Internal error: newPasswordData_.empty()");
      return;
   }

   newPasswordData_.back().password = password;

   newKeyRank_ = oldKeyRank_;
   newKeyRank_.second += 1;

   isLatestChangeAddDevice_ = true;
   addNew_ = true;
   changePassword();
}

void ChangeWalletPasswordDialog::onNewDeviceFailed()
{
   state_ = State::Idle;
   updateState();

   BSMessageBox(BSMessageBox::critical, tr("Wallet Encryption")
      , tr("A problem occured requesting new device key")
      , this).exec();
}

void ChangeWalletPasswordDialog::onPasswordChanged(const string &walletId, bool ok)
{
   if (walletId != wallet_->getWalletId()) {
      logger_->error("ChangeWalletPasswordDialog::onPasswordChanged: unknown walletId {}, expected: {}"
         , walletId, wallet_->getWalletId());
      return;
   }

   if (!ok) {
      logger_->error("ChangeWalletPassword failed for {}", walletId);

      if (isLatestChangeAddDevice_) {
         BSMessageBox(BSMessageBox::critical, tr("Wallet Encryption")
            , tr("Device adding failed")
            , this).exec();
      } else {
         BSMessageBox(BSMessageBox::critical, tr("Wallet Encryption")
            , tr("A problem occured when changing wallet password")
            , this).exec();
      }

      state_ = State::Idle;
      updateState();
      return;
   }

   if (isLatestChangeAddDevice_) {
      BSMessageBox(BSMessageBox::success, tr("Wallet Encryption")
         , tr("Device successfully added")
         , this).exec();
   } else {
      BSMessageBox(BSMessageBox::success, tr("Wallet Encryption")
         , tr("Wallet encryption successfully changed")
         , this).exec();
   }

   QDialog::accept();
}

void ChangeWalletPasswordDialog::deleteDevice(const string &deviceId)
{
   newPasswordData_.clear();
   for (const auto &passwordData : oldPasswordData_) {
      auto deviceInfo = AutheIDClient::getDeviceInfo(passwordData.encKey.toBinStr());
      if (deviceInfo.deviceId != deviceId) {
         newPasswordData_.push_back(passwordData);
      }
   }
   newKeyRank_ = oldKeyRank_;
   newKeyRank_.second -= 1;

   if (newKeyRank_.first != 1) {
      // Something went wrong. Only 1-on-N scheme is supported
      logger_->critical("ChangeWalletPasswordDialog: newKeyRank.first != 1");
      return;
   }

   if (newKeyRank_.second != newPasswordData_.size()) {
      // Something went wrong
      logger_->critical("internal error: oldKeyRank_.second != newPasswordData.size()");
      return;
   }

   if (newPasswordData_.size() == oldPasswordData_.size()) {
      // Something went wrong
      logger_->critical("internal error: newPasswordData.size() == oldPasswordData_.size()");
      return;
   }

   if (newKeyRank_.second == 0) {
      BSMessageBox(BSMessageBox::critical, tr("Error")
         , tr("Cannot remove last device. Please switch to password encryption instead."), this).exec();
      return;
   }

   EnterWalletPassword enterWalletPassword(AutheIDClient::DeactivateWalletDevice, this);
   enterWalletPassword.init(wallet_->getWalletId(), newKeyRank_
      , newPasswordData_, appSettings_, tr("Deactivate device"));
   int result = enterWalletPassword.exec();
   if (result != QDialog::Accepted) {
      return;
   }

   oldKey_ = enterWalletPassword.getPassword();
   removeOld_ = true;

   changePassword();
}

void ChangeWalletPasswordDialog::onTabChanged(int index)
{
   state_ = State::Idle;
   updateState();
}

void ChangeWalletPasswordDialog::updateState()
{
   Pages currentPage = Pages(ui_->tabWidget->currentIndex());

   if (currentPage == Pages::Basic) {
      ui_->pushButtonOk->setText(tr("Continue"));
   } else {
      ui_->pushButtonOk->setText(tr("Add Device"));
   }

   ui_->labelAddDeviceInfo->setVisible(state_ == State::Idle);
   ui_->labelDeviceOldInfo->setVisible(state_ == State::AddDeviceWaitOld || state_ == State::AddDeviceWaitNew);
   ui_->labelAddDeviceSuccess->setVisible(state_ == State::AddDeviceWaitNew);
   ui_->labelDeviceNewInfo->setVisible(state_ == State::AddDeviceWaitNew);
   ui_->lineAddDevice->setVisible(state_ == State::AddDeviceWaitNew);
   deviceKeyOld_->setVisible(state_ == State::AddDeviceWaitOld);
   deviceKeyNew_->setVisible(state_ == State::AddDeviceWaitNew);
}

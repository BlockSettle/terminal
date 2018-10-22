#include "ChangeWalletPasswordDialog.h"
#include "ui_ChangeWalletPasswordDialog.h"

#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "EnterWalletPassword.h"
#include "FrejaNotice.h"
#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "MessageBoxSuccess.h"
#include "SignContainer.h"
#include "WalletKeyWidget.h"


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
   , logger_(logger)
   , ui_(new Ui::ChangeWalletPasswordDialog())
   , signingContainer_(signingContainer)
   , wallet_(wallet)
   , oldKeyRank_(keyRank)
   , appSettings_(appSettings)
   , updateServerClient_(logger)
{
   ui_->setupUi(this);

   resetKeys();

   ui_->labelWalletId->setText(QString::fromStdString(wallet->getWalletId()));

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &ChangeWalletPasswordDialog::onTabChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ChangeWalletPasswordDialog::onContinueClicked);

   connect(signingContainer_.get(), &SignContainer::PasswordChanged, this, &ChangeWalletPasswordDialog::onPasswordChanged);

   deviceKeyOld_ = new WalletKeyWidget(MobileClientRequest::ActivateWalletOldDevice
      , wallet->getWalletId(), 0, false, this);
   deviceKeyOld_->setFixedType(true);
   deviceKeyOld_->setEncryptionKeys(encKeys);
   deviceKeyOld_->setHideFrejaConnect(true);
   deviceKeyOld_->setHideFrejaCombobox(true);

   deviceKeyNew_ = new WalletKeyWidget(MobileClientRequest::ActivateWalletNewDevice
      , wallet->getWalletId(), 0, false, this);
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

   QString usernameAuthApp;

   auto encTypesIt = encTypes.begin();
   auto encKeysIt = encKeys.begin();
   while (encTypesIt != encTypes.end() && encKeysIt != encKeys.end()) {
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = *encTypesIt;
      passwordData.encKey = *encKeysIt;

      if (passwordData.encType == bs::wallet::EncryptionType::Freja) {
         usernameAuthApp = QString::fromStdString(passwordData.encKey.toBinStr());
      }

      oldPasswordData_.push_back(passwordData);
      ++encTypesIt;
      ++encKeysIt;
   }

   if (!usernameAuthApp.isEmpty()) {
      deviceKeyOld_->init(appSettings, usernameAuthApp);
      deviceKeyNew_->init(appSettings, usernameAuthApp);
   }

   ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideGroupboxCaption 
      | WalletKeysSubmitWidget::SetPasswordLabelAsOld
      | WalletKeysSubmitWidget::HideFrejaConnectButton);
   ui_->widgetSubmitKeys->suspend();
   ui_->widgetSubmitKeys->init(MobileClientRequest::DectivateWallet, wallet_->getWalletId(), keyRank, encTypes, encKeys, appSettings);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew
      | WalletKeysCreateWidget::HideFrejaConnectButton
      | WalletKeysCreateWidget::HideWidgetContol);
   ui_->widgetCreateKeys->init(MobileClientRequest::ActivateWallet, wallet_->getWalletId(), username, appSettings);

   ui_->widgetSubmitKeys->setFocus();

   ui_->tabWidget->setCurrentIndex(int(Pages::Basic));

   std::string serverPubKey = appSettings->get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = appSettings->get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = appSettings->get<std::string>(ApplicationSettings::authServerPort);
   updateServerClient_.init(serverPubKey, serverHost, serverPort);
   connect(&updateServerClient_, &MobileClient::updateServerFinished
      , this, &ChangeWalletPasswordDialog::onUpdateServerFinished);

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
   resetKeys();

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
         EnterWalletPassword enterWalletPassword(MobileClientRequest::DectivateWallet, this);
         enterWalletPassword.init(wallet_->getWalletId(), oldKeyRank_
            , oldPasswordData_, appSettings_, tr("Change Password"));
         int result = enterWalletPassword.exec();
         if (result != QDialog::Accepted) {
            return;
         }

         oldKey_ = enterWalletPassword.GetPassword();
         // Switch to password or different AuthApp id here so need to remove
         // wallet registrations on all devices
         deleteAllDeviceId_ = enterWalletPassword.getDeviceId();
      }
   }
   else {
      oldKey_ = ui_->widgetSubmitKeys->key();
   }

   if (isNewFreja) {
      if (showFrejaUsageInfo) {
         FrejaNotice frejaNotice(this);
         int result = frejaNotice.exec();
         if (result != QDialog::Accepted) {
            return;
         }
      }

      EnterWalletPassword enterWalletPassword(MobileClientRequest::ActivateWallet, this);
      enterWalletPassword.init(wallet_->getWalletId(), ui_->widgetCreateKeys->keyRank()
         , newKeys, appSettings_, tr("Activate Freja eID signing"));
      int result = enterWalletPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }

      newKeys[0].password = enterWalletPassword.GetPassword();
      newDeviceId_ = enterWalletPassword.getDeviceId();
   }

   newPasswordData_ = newKeys;
   newKeyRank_ = ui_->widgetCreateKeys->keyRank();
   dryRun_ = true;
   changePassword();
}

void ChangeWalletPasswordDialog::continueAddDevice()
{
   if (state_ != State::Idle) {
      return;
   }

   if (oldPasswordData_.empty() || oldPasswordData_[0].encType != bs::wallet::EncryptionType::Freja) {
      MessageBoxCritical messageBox(tr("Add Device error")
         , tr("Please switch to Freja encryption first"), this);
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
         , addNew_, dryRun_);
   }
   else {
      bool result = wallet_->changePassword(newPasswordData_, newKeyRank_, oldKey_
         , addNew_, dryRun_);
      onPasswordChanged(wallet_->getWalletId(), result);
   }
}

void ChangeWalletPasswordDialog::resetKeys()
{
   newDeviceId_.clear();
   deleteAllDeviceId_.clear();
   oldKey_.clear();
   deviceKeyOldValid_ = false;
   deviceKeyNewValid_ = false;
   isLatestChangeAddDevice_ = false;
   addNew_ = false;
   dryRun_ = true;
}

void ChangeWalletPasswordDialog::updateServer()
{
   if (!newDeviceId_.empty()) {
      updateServerClient_.updateServer(newDeviceId_, wallet_->getWalletId(), true, false);
      newDeviceId_.clear();
      return;
   }

   if (!deleteAllDeviceId_.empty()) {
      updateServerClient_.updateServer(deleteAllDeviceId_, wallet_->getWalletId(), false, true);
      deleteAllDeviceId_.clear();
      return;
   }

   dryRun_ = false;
   changePassword();
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

   MessageBoxCritical(tr("Wallet Password")
      , tr("A problem occured requesting old device key")
      , this).exec();
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
   newDeviceId_ = deviceKeyNew_->deviceId();

   isLatestChangeAddDevice_ = true;
   dryRun_ = true;
   changePassword();
}

void ChangeWalletPasswordDialog::onCreateKeysFailed2()
{
   state_ = State::Idle;
   updateState();

   MessageBoxCritical(tr("Wallet Password")
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
         MessageBoxCritical(tr("Wallet Password")
            , tr("Device adding failed")
            , this).exec();
      } else {
         MessageBoxCritical(tr("Wallet Password")
            , tr("A problem occured when changing wallet password")
            , this).exec();
      }

      return;
   }

   if (dryRun_) {
      dryRun_ = false;
      updateServer();
      return;
   }

   if (isLatestChangeAddDevice_) {
      MessageBoxSuccess(tr("Wallet Password")
         , tr("Device successfully added")
         , this).exec();
   } else {
      MessageBoxSuccess(tr("Wallet Password")
         , tr("Wallet password successfully changed")
         , this).exec();
   }

   QDialog::accept();
}

void ChangeWalletPasswordDialog::onUpdateServerFinished(bool success)
{
   if (!success) {
      MessageBoxCritical(tr("Wallet Password")
         , tr("Updating server failed. Please try again later.")
         , this).exec();
      return;
   }

   updateServer();
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

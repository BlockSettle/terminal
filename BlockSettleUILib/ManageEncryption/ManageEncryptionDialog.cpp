#include "ManageEncryptionDialog.h"
#include "ui_ManageEncryptionDialog.h"

#include <spdlog/spdlog.h>
#include <QToolButton>
#include "ApplicationSettings.h"
#include "EnterWalletPassword.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "WalletKeyWidget.h"
#include "WalletKeysDeleteDevice.h"
#include "WalletKeysSubmitWidget.h"
#include "WalletKeysCreateWidget.h"
#include "Wallets/SyncHDWallet.h"


ManageEncryptionDialog::ManageEncryptionDialog(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<SignContainer> signingContainer
      , const std::shared_ptr<bs::sync::hd::Wallet> &wallet
      , const bs::hd::WalletInfo &walletInfo
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ManageEncryptionDialog())
   , logger_(logger)
   , signingContainer_(signingContainer)
   , wallet_(wallet)
   , walletInfo_(walletInfo)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
{
   ui_->setupUi(this);
   ui_->labelWalletId->setText(walletInfo.rootId());
   ui_->labelWalletName->setText(walletInfo.name());

   resetKeys();

   connect(ui_->tabWidget, &QTabWidget::currentChanged, this, &ManageEncryptionDialog::onTabChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ManageEncryptionDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ManageEncryptionDialog::onContinueClicked);

   connect(signingContainer_.get(), &SignContainer::PasswordChanged, this, &ManageEncryptionDialog::onPasswordChanged);

   QString usernameAuthApp;
   int authCount = 0;
   for (const auto &encKey : walletInfo_.encKeys()) {   // assume we can have encKeys only for Auth type
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = bs::wallet::EncryptionType::Auth;
      passwordData.encKey = encKey.toStdString();

      oldPasswordData_.push_back(passwordData);
      auto deviceInfo = AutheIDClient::getDeviceInfo(encKey.toStdString());

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

   for (const auto &encType : walletInfo_.encTypes()) {
      if (encType == bs::wallet::EncryptionType::Auth) {    // already added encKeys for Auth type
         continue;
      }
      bs::wallet::PasswordData passwordData{};
      passwordData.encType = encType;
      oldPasswordData_.push_back(passwordData);
   }

   if (authCount > 0) {
      ui_->groupBoxCurrentEncryption->setTitle(tr("CURRENT ENCRYPTION"));
   }
   else {
      ui_->groupBoxCurrentEncryption->setTitle(tr("ENTER PASSWORD"));
   }

   ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideGroupboxCaption
      | WalletKeysSubmitWidget::SetPasswordLabelAsOld
      | WalletKeysSubmitWidget::HideAuthConnectButton
      | WalletKeysSubmitWidget::HidePasswordWarning);
   ui_->widgetSubmitKeys->suspend();
   ui_->widgetSubmitKeys->init(AutheIDClient::DeactivateWallet, walletInfo, WalletKeyWidget::UseType::RequestAuthForDialog, logger_, appSettings, connectionManager);

   ui_->widgetCreateKeys->setFlags(WalletKeysCreateWidget::HideGroupboxCaption
      | WalletKeysCreateWidget::SetPasswordLabelAsNew
      | WalletKeysCreateWidget::HideAuthConnectButton
      | WalletKeysCreateWidget::HideWidgetContol);
   ui_->widgetCreateKeys->init(AutheIDClient::ActivateWallet, walletInfo_, WalletKeyWidget::UseType::ChangeAuthForDialog, appSettings, connectionManager_, logger_);

   ui_->widgetSubmitKeys->setFocus();

   ui_->tabWidget->setCurrentIndex(int(Pages::Basic));

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::returnPressed, ui_->widgetCreateKeys, &WalletKeysCreateWidget::setFocus);
   connect(ui_->widgetCreateKeys, &WalletKeysCreateWidget::returnPressed, this, &ManageEncryptionDialog::onContinueClicked);

   updateState();
}

ManageEncryptionDialog::~ManageEncryptionDialog() = default;

void ManageEncryptionDialog::accept()
{
   onContinueClicked();
}

void ManageEncryptionDialog::onContinueClicked()
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
void ManageEncryptionDialog::continueBasic()
{
   std::vector<bs::wallet::PasswordData> newKeys = ui_->widgetCreateKeys->passwordData();

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

   if (!ui_->widgetCreateKeys->isValid() && isNewAuth) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Invalid new Auth eID"),
                                    tr("Please use valid Auth eID."),
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
         enterWalletPassword.init(walletInfo_, appSettings_, connectionManager_, WalletKeyWidget::UseType::RequestAuthAsDialog, tr("Change Encryption"), logger_);
         int result = enterWalletPassword.exec();
         if (result != QDialog::Accepted) {
            return;
         }

         oldKey_ = enterWalletPassword.resultingKey();
      }
   }
   else {
      oldKey_ = ui_->widgetSubmitKeys->key();
   }

   // currently we support only 1 of m, on wallet creation 1-of-1 used
   if (isNewAuth) {
      EnterWalletPassword enterWalletPassword(AutheIDClient::ActivateWallet, this);
      // overwrite encKeys
      bs::hd::WalletInfo wi = walletInfo_;
      wi.setEncTypes(QList<bs::wallet::EncryptionType>() << ui_->widgetCreateKeys->passwordData(0).encType);
      wi.setEncKeys(QList<QString>() << QString::fromStdString(ui_->widgetCreateKeys->passwordData(0).encKey.toBinStr()));

      enterWalletPassword.init(wi, appSettings_, connectionManager_, WalletKeyWidget::UseType::ChangeToEidAsDialog, tr("Activate Auth eID signing"), logger_);
      int result = enterWalletPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }

      newKeys[0] = enterWalletPassword.passwordData(0);
   }
   else {
      newKeys[0] = ui_->widgetCreateKeys->passwordData(0);
   }

   newPasswordData_ = newKeys;
   newKeyRank_ = ui_->widgetCreateKeys->keyRank();
   changePassword();
}

// Add a new Auth eID device to the wallet.
void ManageEncryptionDialog::continueAddDevice()
{
   if (state_ != State::Idle) {
      return;
   }

   if (walletInfo_.keyRank().first != 1) {
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

   std::vector<bs::wallet::PasswordData> newKeys = ui_->widgetCreateKeys->passwordData();

   // Request eid auth to decrypt wallet
   {
      EnterWalletPassword enterWalletOldPassword(AutheIDClient::ActivateWalletOldDevice, this);
      enterWalletOldPassword.init(walletInfo_, appSettings_, connectionManager_, WalletKeyWidget::UseType::RequestAuthAsDialog, tr("Change Encryption"), logger_);
      int result = enterWalletOldPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }
      oldKey_ = enterWalletOldPassword.resultingKey();
   }


   // Request eid activate new device
   {
      EnterWalletPassword enterWalletNewPassword(AutheIDClient::ActivateWalletNewDevice, this);
      // overwrite encKeys
      bs::hd::WalletInfo wi = walletInfo_;
      //wi.setEncTypes(QList<bs::wallet::EncryptionType>() << ui_->widgetCreateKeys->passwordData(0).getEncType());
      //wi.setEncKeys(QList<QString>() << ui_->widgetCreateKeys->passwordData(0).getEncKey());

      enterWalletNewPassword.init(walletInfo_, appSettings_, connectionManager_, WalletKeyWidget::UseType::ChangeToEidAsDialog, tr("Activate Auth eID signing"), logger_);
      int result = enterWalletNewPassword.exec();
      if (result != QDialog::Accepted) {
         return;
      }
      newKeys[0] = enterWalletNewPassword.passwordData(0);
   }

   // Add device
   newPasswordData_ = newKeys;
   newKeyRank_ = walletInfo_.keyRank();
   newKeyRank_.second++;
   addNew_ = true;
   changePassword();
}

void ManageEncryptionDialog::changePassword()
{
//   //   TODO add to bs::hd::wallet overload for PasswordData
//   std::vector<bs::wallet::PasswordData> pwData;
//   pwData.assign(newPasswordData_.cbegin(), newPasswordData_.cend());
//   signingContainer_->changePassword(wallet_->walletId(), pwData, newKeyRank_
//      , oldKey_, addNew_, removeOld_, false);
}

void ManageEncryptionDialog::resetKeys()
{
   oldKey_.clear();
   deviceKeyOldValid_ = false;
   deviceKeyNewValid_ = false;
   isLatestChangeAddDevice_ = false;
   addNew_ = false;
   removeOld_ = false;
}



void ManageEncryptionDialog::onPasswordChanged(const std::string &walletId, bool ok)
{
   if (walletId != wallet_->walletId()) {
      logger_->error("ManageEncryptionDialog::onPasswordChanged: unknown walletId {}, expected: {}"
         , walletId, wallet_->walletId());
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

void ManageEncryptionDialog::deleteDevice(const std::string &deviceId)
{
   newPasswordData_.clear();
   for (const auto &passwordData : oldPasswordData_) {
      auto deviceInfo = AutheIDClient::getDeviceInfo(passwordData.encKey.toBinStr());
      if (deviceInfo.deviceId != deviceId) {
         newPasswordData_.push_back(passwordData);
      }
   }
   newKeyRank_ = walletInfo_.keyRank();
   newKeyRank_.second -= 1;

   if (newKeyRank_.first != 1) {
      // Something went wrong. Only 1-on-N scheme is supported
      logger_->critical("ManageEncryptionDialog: newKeyRank.first != 1");
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
   enterWalletPassword.init(walletInfo_, appSettings_, connectionManager_, WalletKeyWidget::UseType::RequestAuthAsDialog,tr("Deactivate device"), logger_);
   int result = enterWalletPassword.exec();
   if (result != QDialog::Accepted) {
      return;
   }

   oldKey_ = enterWalletPassword.resultingKey();
   removeOld_ = true;

   changePassword();
}

void ManageEncryptionDialog::onTabChanged(int index)
{
   state_ = State::Idle;
   updateState();
}

void ManageEncryptionDialog::updateState()
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
}

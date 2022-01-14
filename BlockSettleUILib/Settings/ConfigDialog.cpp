/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ConfigDialog.h"

#include "ArmoryServersProvider.h"
#include "AssetManager.h"
#include "GeneralSettingsPage.h"
#include "NetworkSettingsPage.h"
#include "SignersProvider.h"
#include "WalletSignerContainer.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "ui_ConfigDialog.h"

#include <QPushButton>


SettingsPage::SettingsPage(QWidget *parent)
   : QWidget(parent)
{}

void SettingsPage::init(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
   , const std::shared_ptr<SignersProvider> &signersProvider
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   appSettings_ = appSettings;
   armoryServersProvider_ = armoryServersProvider;
   signersProvider_ = signersProvider;
   signContainer_ = signContainer;
   walletsMgr_ = walletsMgr;
   initSettings();
   display();
}

void SettingsPage::init(const ApplicationSettings::State& state)
{
   settings_ = state;
   initSettings();
   display();
}

void SettingsPage::onSetting(int setting, const QVariant& value)
{
   settings_[static_cast<ApplicationSettings::Setting>(setting)] = value;
}


ConfigDialog::ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
      , const std::shared_ptr<SignersProvider> &signersProvider
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::ConfigDialog)
 , appSettings_(appSettings)
 , armoryServersProvider_(armoryServersProvider)
 , signersProvider_(signersProvider)
 , signContainer_(signContainer)
{
   ui_->setupUi(this);

   if (!appSettings_->get<bool>(ApplicationSettings::initialized)) {
      appSettings_->SetDefaultSettings(true);
      ui_->pushButtonCancel->setEnabled(false);
   }
   prevState_ = appSettings_->getState();

   pages_ = {ui_->pageGeneral, ui_->pageNetwork, ui_->pageSigner, ui_->pageDealing
      , ui_->pageAPI };

   for (const auto &page : pages_) {
      page->init(appSettings_, armoryServersProvider_, signersProvider_, signContainer_, walletsMgr);
      connect(page, &SettingsPage::illformedSettings, this, &ConfigDialog::onIllformedSettings);
   }

   ui_->listWidget->setCurrentRow(0);
   ui_->stackedWidget->setCurrentIndex(0);

   connect(ui_->listWidget, &QListWidget::currentRowChanged, this, &ConfigDialog::onSelectionChanged);
   connect(ui_->pushButtonSetDefault, &QPushButton::clicked, this, &ConfigDialog::onDisplayDefault);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ConfigDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ConfigDialog::onAcceptSettings);

   connect(ui_->pageNetwork, &NetworkSettingsPage::reconnectArmory, this, [this](){
      emit reconnectArmory();
   });

   // armory servers should be saved even if whole ConfigDialog rejected
   // we overwriting prevState_ with new vales once ArmoryServersWidget closed
   connect(ui_->pageNetwork, &NetworkSettingsPage::armoryServerChanged, this, [this](){
      for (ApplicationSettings::Setting s : {
           ApplicationSettings::armoryServers,
           ApplicationSettings::runArmoryLocally,
           ApplicationSettings::netType,
           ApplicationSettings::armoryDbName,
           ApplicationSettings::armoryDbIp,
           ApplicationSettings::armoryDbPort}) {
         prevState_[s] = appSettings_->get(s);
      }
   });

   connect(ui_->pageSigner, &SignerSettingsPage::signersChanged, this, [this](){
      for (ApplicationSettings::Setting s : {
           ApplicationSettings::remoteSigners,
           ApplicationSettings::signerIndex}) {
         prevState_[s] = appSettings_->get(s);
      }
   });

   connect(ui_->pageGeneral, &GeneralSettingsPage::requestDataEncryption, this, [this]() {
      signContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ManagePublicDataEncryption);
   });
}

ConfigDialog::ConfigDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::ConfigDialog)
{
   ui_->setupUi(this);

   pages_ = { ui_->pageGeneral, ui_->pageNetwork, ui_->pageSigner, ui_->pageDealing
      , ui_->pageAPI };

   for (const auto& page : pages_) {
      connect(page, &SettingsPage::illformedSettings, this, &ConfigDialog::onIllformedSettings);
      connect(page, &SettingsPage::putSetting, this, &ConfigDialog::putSetting);
      connect(page, &SettingsPage::resetSettings, this, &ConfigDialog::resetSettings);
   }
   connect(ui_->pageNetwork, &NetworkSettingsPage::reconnectArmory, this, &ConfigDialog::reconnectArmory);
   connect(ui_->pageNetwork, &NetworkSettingsPage::setArmoryServer, this, &ConfigDialog::setArmoryServer);
   connect(ui_->pageNetwork, &NetworkSettingsPage::addArmoryServer, this, &ConfigDialog::addArmoryServer);
   connect(ui_->pageNetwork, &NetworkSettingsPage::delArmoryServer, this, &ConfigDialog::delArmoryServer);
   connect(ui_->pageNetwork, &NetworkSettingsPage::updArmoryServer, this, &ConfigDialog::updArmoryServer);
   connect(ui_->pageSigner, &SignerSettingsPage::setSigner, this, &ConfigDialog::setSigner);

   ui_->listWidget->setCurrentRow(0);
   ui_->stackedWidget->setCurrentIndex(0);

   connect(ui_->listWidget, &QListWidget::currentRowChanged, this, &ConfigDialog::onSelectionChanged);
   connect(ui_->pushButtonSetDefault, &QPushButton::clicked, this, &ConfigDialog::onDisplayDefault);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ConfigDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ConfigDialog::onAcceptSettings);
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::onSettingsState(const ApplicationSettings::State& state)
{
   if (prevState_.empty()) {
      prevState_ = state;
   }
   for (const auto& page : pages_) {
      page->init(state);
   }
}

void ConfigDialog::onSetting(int setting, const QVariant& value)
{
   for (const auto& page : pages_) {
      page->onSetting(setting, value);
   }
}

void ConfigDialog::onArmoryServers(const QList<ArmoryServer>& servers, int idxCur, int idxConn)
{
   ui_->pageNetwork->onArmoryServers(servers, idxCur, idxConn);
}

void ConfigDialog::onSignerSettings(const QList<SignerHost>& signers
   , const std::string& ownKey, int idxCur)
{
   ui_->pageSigner->onSignerSettings(signers, ownKey, idxCur);
}

void ConfigDialog::popupNetworkSettings()
{
   ui_->stackedWidget->setCurrentWidget(ui_->pageNetwork);
   ui_->listWidget->setCurrentRow(ui_->stackedWidget->indexOf(ui_->pageNetwork));
}

QString ConfigDialog::encryptErrorStr(EncryptError error)
{
   switch (error) {
      case EncryptError::NoError:            return tr("No error");
      case EncryptError::NoPrimaryWallet:    return tr("No primary wallet");
      case EncryptError::NoEncryptionKey:    return tr("No encryption key");
      case EncryptError::EncryptError:       return tr("Encryption error");
   }
   assert(false);
   return tr("Unknown error");
}

void ConfigDialog::encryptData(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &signContainer
   , const SecureBinaryData &data
   , const ConfigDialog::EncryptCb &cb)
{
   cb(EncryptError::NoEncryptionKey, {});
}

void ConfigDialog::decryptData(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &signContainer
   , const SecureBinaryData &data, const ConfigDialog::EncryptCb &cb)
{
   cb(EncryptError::NoEncryptionKey, {});
}

void ConfigDialog::getChatPrivKey(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<SignContainer> &signContainer
   , const ConfigDialog::EncryptCb &cb)
{
   const auto &primaryWallet = walletsMgr->getPrimaryWallet();
   if (!primaryWallet) {
      cb(EncryptError::NoPrimaryWallet, {});
      return;
   }
   auto walletSigner = std::dynamic_pointer_cast<WalletSignerContainer>(signContainer);
   walletSigner->getChatNode(primaryWallet->walletId(), [cb](const BIP32_Node &node) {
      if (node.getPrivateKey().empty()) {
         cb(EncryptError::NoEncryptionKey, {});
         return;
      }
      cb(EncryptError::NoError, node.getPrivateKey());
   });
}

void ConfigDialog::onDisplayDefault()
{  // reset only currently selected page - maybe a subject to change
   pages_[ui_->stackedWidget->currentIndex()]->reset();
}

void ConfigDialog::onAcceptSettings()
{
   for (const auto &page : pages_) {
      page->apply();
   }
   if (appSettings_) {
      appSettings_->SaveSettings();
   }
   accept();
}

void ConfigDialog::onSelectionChanged(int currentRow)
{
   ui_->stackedWidget->setCurrentIndex(currentRow);
}

void ConfigDialog::onIllformedSettings(bool illformed)
{
   ui_->pushButtonOk->setEnabled(!illformed);
}

void ConfigDialog::reject()
{
   if (appSettings_) {
      appSettings_->setState(prevState_);
   }
   else {
      emit resetSettingsToState(prevState_);
   }
   QDialog::reject();
}

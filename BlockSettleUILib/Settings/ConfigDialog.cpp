/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

#include "ui_ConfigDialog.h"

#include <QPushButton>


SettingsPage::SettingsPage(QWidget *parent)
   : QWidget(parent)
{
}

void SettingsPage::init(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
   , const std::shared_ptr<SignersProvider> &signersProvider
   , std::shared_ptr<SignContainer> signContainer) {
   appSettings_ = appSettings;
   armoryServersProvider_ = armoryServersProvider;
   signersProvider_ = signersProvider;
   signContainer_ = signContainer;
   initSettings();
   display();
}


ConfigDialog::ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
      , const std::shared_ptr<SignersProvider> &signersProvider
      , std::shared_ptr<SignContainer> signContainer
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

   pages_ = {ui_->pageGeneral, ui_->pageNetwork, ui_->pageSigner, ui_->pageDealing };

   for (const auto &page : pages_) {
      page->init(appSettings_, armoryServersProvider_, signersProvider_, signContainer_);
      connect(page, &SettingsPage::illformedSettings, this, &ConfigDialog::illformedSettings);
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
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::onDisplayDefault()
{  // reset only currently selected page - maybe a subject to change
   pages_[ui_->stackedWidget->currentIndex()]->reset();
}

void ConfigDialog::onAcceptSettings()
{
   for (const auto &page : pages_) {
      page->apply();
   }

   appSettings_->SaveSettings();
   accept();
}

void ConfigDialog::onSelectionChanged(int currentRow)
{
   ui_->stackedWidget->setCurrentIndex(currentRow);
}

void ConfigDialog::illformedSettings(bool illformed)
{
   ui_->pushButtonOk->setEnabled(!illformed);
}

void ConfigDialog::reject()
{
   appSettings_->setState(prevState_);
   QDialog::reject();
}

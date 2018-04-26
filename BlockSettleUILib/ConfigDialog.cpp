#include "ConfigDialog.h"

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "WalletsManager.h"

#include "ui_ConfigDialog.h"

ConfigDialog::ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<WalletsManager>& walletsMgr
      , const std::shared_ptr<AssetManager> &assetMgr
      , QWidget* parent)
 : QDialog{parent}
 , ui_{new Ui::ConfigDialog{}}
 , applicationSettings_{appSettings}
 , walletsMgr_{walletsMgr}
 , assetMgr_{assetMgr}
{
   ui_->setupUi(this);

   if (!applicationSettings_->get<bool>(ApplicationSettings::initialized)) {
      applicationSettings_->SetDefaultSettings(true);
      ui_->pushButtonCancel->setEnabled(false);
   }

   ui_->pageGeneral->displaySettings(applicationSettings_, walletsMgr_, false);
   ui_->pageNetwork->setAppSettings(applicationSettings_);
   ui_->pageNetwork->displaySettings(false);

   ui_->pageSigner->setAppSettings(applicationSettings_);
   ui_->pageSigner->displaySettings();

   ui_->pageDealing->setAppSettings(applicationSettings_);
   ui_->pageDealing->displaySettings(assetMgr, false);

   ui_->listWidget->setCurrentRow(0);
   ui_->stackedWidget->setCurrentIndex(0);

   connect(ui_->listWidget, &QListWidget::currentRowChanged, this, &ConfigDialog::onSelectionChanged);
   connect(ui_->pushButtonSetDefault, &QPushButton::clicked, this, &ConfigDialog::onDisplayDefault);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ConfigDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ConfigDialog::onAcceptSettings);
}

void ConfigDialog::onDisplayDefault()
{
   ui_->pageGeneral->displaySettings(applicationSettings_, walletsMgr_, true);
   ui_->pageNetwork->displaySettings(true);
   ui_->pageDealing->displaySettings(assetMgr_, true);
   ui_->pageSigner->displaySettings(true);
}

void ConfigDialog::onAcceptSettings()
{
   ui_->pageGeneral->applyChanges(applicationSettings_, walletsMgr_);
   ui_->pageNetwork->applyChanges();
   ui_->pageDealing->applyChanges();
   ui_->pageSigner->applyChanges();

   applicationSettings_->SaveSettings();

   accept();
}

void ConfigDialog::onSelectionChanged(int currentRow)
{
   ui_->stackedWidget->setCurrentIndex(currentRow);
   ui_->stackedWidget->adjustSize();
   adjustSize();
}

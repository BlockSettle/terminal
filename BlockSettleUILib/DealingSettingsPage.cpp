#include "DealingSettingsPage.h"
#include "ui_DealingSettingsPage.h"

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "SecuritiesModel.h"

DealingSettingsPage::DealingSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::DealingSettingsPage{}}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonResetCnt, &QPushButton::clicked, this, &DealingSettingsPage::onResetCountes);
}

void DealingSettingsPage::setAppSettings(const std::shared_ptr<ApplicationSettings>& appSettings)
{
   appSettings_ = appSettings;
}

void DealingSettingsPage::displaySettings(const std::shared_ptr<AssetManager> &assetMgr
   , bool displayDefault)
{
   ui_->checkBoxDrop->setChecked(appSettings_->get<bool>(ApplicationSettings::dropQN, displayDefault));
}

void DealingSettingsPage::applyChanges()
{
   appSettings_->set(ApplicationSettings::dropQN, ui_->checkBoxDrop->isChecked());
}

void DealingSettingsPage::onResetCountes()
{
   appSettings_->reset(ApplicationSettings::Filter_MD_QN_cnt);
   ui_->pushButtonResetCnt->setEnabled(false);
}

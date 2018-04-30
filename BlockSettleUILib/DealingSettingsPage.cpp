#include "DealingSettingsPage.h"
#include "ui_DealingSettingsPage.h"

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "SecuritiesModel.h"

DealingSettingsPage::DealingSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::DealingSettingsPage{}}
   , secModel_{nullptr}
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

   if (assetMgr->hasSecurities()) {
      secModel_ = new SecuritiesModel(assetMgr, appSettings_->get<QStringList>(ApplicationSettings::Filter_MD_QN)
         , ui_->treeViewSecurities);
      ui_->treeViewSecurities->setModel(secModel_);
      secModel_->sort(0);
      ui_->treeViewSecurities->expandAll();
      ui_->treeViewSecurities->setHeaderHidden(true);
      ui_->label->hide();
   } else {
      ui_->treeViewSecurities->hide();
   }
}

void DealingSettingsPage::applyChanges()
{
   appSettings_->set(ApplicationSettings::dropQN, ui_->checkBoxDrop->isChecked());

   if (secModel_) {
      const auto visibilitySettings = secModel_->getVisibilitySettings();
      appSettings_->set(ApplicationSettings::Filter_MD_QN, visibilitySettings);
   }
}

void DealingSettingsPage::onResetCountes()
{
   appSettings_->reset(ApplicationSettings::Filter_MD_QN_cnt);
   ui_->pushButtonResetCnt->setEnabled(false);
}

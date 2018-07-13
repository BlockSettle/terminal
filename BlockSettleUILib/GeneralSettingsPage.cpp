#include "GeneralSettingsPage.h"

#include "ui_GeneralSettingsPage.h"

#include "ApplicationSettings.h"
#include "UiUtils.h"
#include "WalletsManager.h"

GeneralSettingsPage::GeneralSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{ new Ui::GeneralSettingsPage{}}
{
   ui_->setupUi(this);
}

void GeneralSettingsPage::displaySettings(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsMgr, bool displayDefault)
{
   ui_->checkBoxLaunchToTray->setChecked(appSettings->get<bool>(ApplicationSettings::launchToTray, displayDefault));
   ui_->checkBoxMinimizeToTray->setChecked(appSettings->get<bool>(ApplicationSettings::minimizeToTray, displayDefault));
   ui_->checkBoxCloseToTray->setChecked(appSettings->get<bool>(ApplicationSettings::closeToTray, displayDefault));
   ui_->checkBoxShowTxNotification->setChecked(appSettings->get<bool>(ApplicationSettings::notifyOnTX, displayDefault));
   ui_->checkBoxEnableLogging->setChecked(appSettings->get<bool>(ApplicationSettings::EnableLogging, displayDefault));
}

void GeneralSettingsPage::applyChanges(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsMgr)
{
   appSettings->set(ApplicationSettings::launchToTray, ui_->checkBoxLaunchToTray->isChecked());
   appSettings->set(ApplicationSettings::minimizeToTray, ui_->checkBoxMinimizeToTray->isChecked());
   appSettings->set(ApplicationSettings::closeToTray, ui_->checkBoxCloseToTray->isChecked());
   appSettings->set(ApplicationSettings::notifyOnTX, ui_->checkBoxShowTxNotification->isChecked());
   appSettings->set(ApplicationSettings::EnableLogging, ui_->checkBoxEnableLogging->isChecked());
}

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
   ui_->warnLabel->hide();
}

void GeneralSettingsPage::displaySettings(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsMgr, bool displayDefault)
{
   ui_->checkBoxLaunchToTray->setChecked(appSettings->get<bool>(ApplicationSettings::launchToTray, displayDefault));
   ui_->checkBoxMinimizeToTray->setChecked(appSettings->get<bool>(ApplicationSettings::minimizeToTray, displayDefault));
   ui_->checkBoxCloseToTray->setChecked(appSettings->get<bool>(ApplicationSettings::closeToTray, displayDefault));
   ui_->checkBoxShowTxNotification->setChecked(appSettings->get<bool>(ApplicationSettings::notifyOnTX, displayDefault));

   const auto cfg = appSettings->GetLogsConfig();
   ui_->logFileName->setText(QString::fromStdString(cfg.at(0).fileName));
   ui_->logMsgFileName->setText(QString::fromStdString(cfg.at(1).fileName));
   if (cfg.at(0).level < bs::LogLevel::off) {
      ui_->logLevel->setCurrentIndex(static_cast<int>(cfg.at(0).level));
      ui_->groupBoxLogging->setChecked(true);
   } else {
      ui_->groupBoxLogging->setChecked(false);
   }
   if (cfg.at(1).level < bs::LogLevel::off) {
      ui_->logLevelMsg->setCurrentIndex(static_cast<int>(cfg.at(1).level));
      ui_->groupBoxLoggingMsg->setChecked(true);
   } else {
      ui_->groupBoxLoggingMsg->setChecked(false);
   }
}

static inline QString logLevel(int level)
{
   switch(level) {
      case 0 : return QLatin1String("trace");
      case 1 : return QLatin1String("debug");
      case 2 : return QLatin1String("warn");
      case 3 : return QLatin1String("info");
      case 4 : return QLatin1String("error");
      case 5 : return QLatin1String("crit");
      default : return QString();
   }
}

void GeneralSettingsPage::applyChanges(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsMgr)
{
   appSettings->set(ApplicationSettings::launchToTray, ui_->checkBoxLaunchToTray->isChecked());
   appSettings->set(ApplicationSettings::minimizeToTray, ui_->checkBoxMinimizeToTray->isChecked());
   appSettings->set(ApplicationSettings::closeToTray, ui_->checkBoxCloseToTray->isChecked());
   appSettings->set(ApplicationSettings::notifyOnTX, ui_->checkBoxShowTxNotification->isChecked());

   auto cfg = appSettings->GetLogsConfig();

   {
      QStringList logSettings;
      logSettings << ui_->logFileName->text();
      logSettings << QString::fromStdString(cfg.at(0).category);
      logSettings << QString::fromStdString(cfg.at(0).pattern);

      if (ui_->groupBoxLogging->isChecked()) {
         logSettings << logLevel(ui_->logLevel->currentIndex());
      } else {
         logSettings << QString();
      }

      appSettings->set(ApplicationSettings::logDefault, logSettings);
   }

   {
      QStringList logSettings;
      logSettings << ui_->logMsgFileName->text();
      logSettings << QString::fromStdString(cfg.at(1).category);
      logSettings << QString::fromStdString(cfg.at(1).pattern);

      if (ui_->groupBoxLoggingMsg->isChecked()) {
         logSettings << logLevel(ui_->logLevelMsg->currentIndex());
      } else {
         logSettings << QString();
      }

      appSettings->set(ApplicationSettings::logMessages, logSettings);
   }
}

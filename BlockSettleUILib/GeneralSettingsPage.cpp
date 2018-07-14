#include "GeneralSettingsPage.h"

#include "ui_GeneralSettingsPage.h"

#include "ApplicationSettings.h"
#include "UiUtils.h"
#include "WalletsManager.h"

#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>


GeneralSettingsPage::GeneralSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{ new Ui::GeneralSettingsPage{}}
{
   ui_->setupUi(this);
   ui_->warnLabel->hide();

   connect(ui_->logFileName, &QLineEdit::textChanged,
      this, &GeneralSettingsPage::onLogFileNameEdited);
   connect(ui_->logMsgFileName, &QLineEdit::textChanged,
      this, &GeneralSettingsPage::onLogFileNameEdited);
   connect(ui_->chooseLogFileBtn, &QPushButton::clicked,
      this, &GeneralSettingsPage::onSelectLogFile);
   connect(ui_->chooseLogFileMsgBtn, &QPushButton::clicked,
      this, &GeneralSettingsPage::onSelectMsgLogFile);
   connect(ui_->groupBoxLogging, &QGroupBox::clicked,
      this, &GeneralSettingsPage::onGroupBoxClicked);
   connect(ui_->groupBoxLoggingMsg, &QGroupBox::clicked,
      this, &GeneralSettingsPage::onGroupBoxClicked);
   connect(ui_->logLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, &GeneralSettingsPage::onLogLevelChanged);
   connect(ui_->logLevelMsg, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, &GeneralSettingsPage::onLogLevelChanged);
}

void GeneralSettingsPage::displaySettings(const std::shared_ptr<ApplicationSettings>& appSettings
   , const std::shared_ptr<WalletsManager>& walletsMgr, bool displayDefault)
{
   ui_->checkBoxLaunchToTray->setChecked(appSettings->get<bool>(ApplicationSettings::launchToTray, displayDefault));
   ui_->checkBoxMinimizeToTray->setChecked(appSettings->get<bool>(ApplicationSettings::minimizeToTray, displayDefault));
   ui_->checkBoxCloseToTray->setChecked(appSettings->get<bool>(ApplicationSettings::closeToTray, displayDefault));
   ui_->checkBoxShowTxNotification->setChecked(appSettings->get<bool>(ApplicationSettings::notifyOnTX, displayDefault));

   const auto cfg = appSettings->GetLogsConfig(displayDefault);
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

   if (!displayDefault) {
      ui_->warnLabel->hide();
   }
}

static inline QString logLevel(int level)
{
   switch(level) {
      case 0 : return QLatin1String("trace");
      case 1 : return QLatin1String("debug");
      case 2 : return QLatin1String("info");
      case 3 : return QLatin1String("warn");
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

void GeneralSettingsPage::onSelectLogFile()
{
   QString fileName = QFileDialog::getOpenFileName(this,
      tr("Select file for General Terminal logs..."),
      QFileInfo(ui_->logFileName->text()).path());

   if (!fileName.isEmpty()) {
      ui_->logFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onSelectMsgLogFile()
{
   QString fileName = QFileDialog::getOpenFileName(this,
      tr("Select file for Matching Engine logs..."),
      QFileInfo(ui_->logMsgFileName->text()).path());

   if (!fileName.isEmpty()) {
      ui_->logMsgFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onLogFileNameEdited(const QString &)
{
   checkSettings();
}

void GeneralSettingsPage::onGroupBoxClicked(bool)
{
   checkSettings();
}

void GeneralSettingsPage::onLogLevelChanged(int)
{
   checkSettings();
}

void GeneralSettingsPage::checkSettings()
{
   ui_->warnLabel->show();

   if (ui_->groupBoxLogging->isChecked()) {
      if (ui_->logFileName->text().isEmpty()) {
         ui_->warnLabel->setText(tr("Log file name can't be empty."));

         emit illformedSettings(true);

         return;
      }
   }

   if (ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logMsgFileName->text().isEmpty()) {
         ui_->warnLabel->setText(tr("Log file name can't be empty."));

         emit illformedSettings(true);

         return;
      }
   }

   if (ui_->groupBoxLogging->isChecked() && ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logFileName->text() == ui_->logMsgFileName->text()) {
         ui_->warnLabel->setText(tr("Logging can't be done in one file."));

         emit illformedSettings(true);

         return;
      }
   }

   ui_->warnLabel->setText(tr("Changes to logging settings will take effect "
                              "after the application is restarted. "));

   emit illformedSettings(false);
}

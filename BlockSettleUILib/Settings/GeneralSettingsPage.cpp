/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "GeneralSettingsPage.h"

#include "ui_GeneralSettingsPage.h"

#include "ApplicationSettings.h"
#include "UiUtils.h"
#include "TerminalEncryptionDialog.h"

#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>


GeneralSettingsPage::GeneralSettingsPage(QWidget* parent)
   : SettingsPage{parent}
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
   connect(ui_->logLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, &GeneralSettingsPage::onLogLevelChanged);
   connect(ui_->logLevelMsg, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, &GeneralSettingsPage::onLogLevelChanged);

   connect(ui_->pushButtonEnableEncryption, &QPushButton::clicked, this, [this](){
      TerminalEncryptionDialog dialog(TerminalEncryptionDialog::EnableEncryption, this);
      dialog.exec();
   });

   connect(ui_->pushButtonDisableEncryption, &QPushButton::clicked, this, [this](){
      TerminalEncryptionDialog dialog(TerminalEncryptionDialog::DisableEncryption, this);
      dialog.exec();
   });
}

GeneralSettingsPage::~GeneralSettingsPage() = default;

void GeneralSettingsPage::display()
{
   ui_->checkBoxLaunchToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::launchToTray));
   ui_->checkBoxMinimizeToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::minimizeToTray));
   ui_->checkBoxCloseToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::closeToTray));
   ui_->checkBoxShowTxNotification->setChecked(appSettings_->get<bool>(ApplicationSettings::notifyOnTX));
   ui_->addvancedDialogByDefaultCheckBox->setChecked(appSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault));
   ui_->checkBox_subscribeToMdOnStart->setChecked(appSettings_->get<bool>(ApplicationSettings::SubscribeToMDOnStart));

   const auto cfg = appSettings_->GetLogsConfig();
   ui_->logFileName->setText(QString::fromStdString(cfg.at(0).fileName));
   ui_->logMsgFileName->setText(QString::fromStdString(cfg.at(1).fileName));
   ui_->logLevel->setCurrentIndex(static_cast<int>(cfg.at(0).level));
   ui_->logLevelMsg->setCurrentIndex(static_cast<int>(cfg.at(1).level));

   ui_->warnLabel->hide();
}

void GeneralSettingsPage::reset()
{
   for (const auto &setting : {ApplicationSettings::launchToTray, ApplicationSettings::minimizeToTray
      , ApplicationSettings::closeToTray, ApplicationSettings::notifyOnTX
      , ApplicationSettings::AdvancedTxDialogByDefault, ApplicationSettings::SubscribeToMDOnStart
      , ApplicationSettings::logDefault, ApplicationSettings::logMessages}) {
      appSettings_->reset(setting, false);
   }
   display();
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

void GeneralSettingsPage::apply()
{
   appSettings_->set(ApplicationSettings::launchToTray, ui_->checkBoxLaunchToTray->isChecked());
   appSettings_->set(ApplicationSettings::minimizeToTray, ui_->checkBoxMinimizeToTray->isChecked());
   appSettings_->set(ApplicationSettings::closeToTray, ui_->checkBoxCloseToTray->isChecked());
   appSettings_->set(ApplicationSettings::notifyOnTX, ui_->checkBoxShowTxNotification->isChecked());
   appSettings_->set(ApplicationSettings::AdvancedTxDialogByDefault,
      ui_->addvancedDialogByDefaultCheckBox->isChecked());

   appSettings_->set(ApplicationSettings::SubscribeToMDOnStart
      , ui_->checkBox_subscribeToMdOnStart->isChecked());

   auto cfg = appSettings_->GetLogsConfig();

   {
      QStringList logSettings;
      logSettings << ui_->logFileName->text();
      logSettings << QString::fromStdString(cfg.at(0).category);
      logSettings << QString::fromStdString(cfg.at(0).pattern);

      if (ui_->logLevel->currentIndex() < static_cast<int>(bs::LogLevel::off)) {
         logSettings << logLevel(ui_->logLevel->currentIndex());
      } else {
         logSettings << QString();
      }

      appSettings_->set(ApplicationSettings::logDefault, logSettings);
   }

   {
      QStringList logSettings;
      logSettings << ui_->logMsgFileName->text();
      logSettings << QString::fromStdString(cfg.at(1).category);
      logSettings << QString::fromStdString(cfg.at(1).pattern);

      if (ui_->logLevelMsg->currentIndex() < static_cast<int>(bs::LogLevel::off)) {
         logSettings << logLevel(ui_->logLevelMsg->currentIndex());
      } else {
         logSettings << QString();
      }

      appSettings_->set(ApplicationSettings::logMessages, logSettings);
   }
}

void GeneralSettingsPage::onSelectLogFile()
{
   QString fileName = QFileDialog::getSaveFileName(this,
      tr("Select file for General Terminal logs..."),
      QFileInfo(ui_->logFileName->text()).path(),
      QString(), nullptr, QFileDialog::DontConfirmOverwrite);

   if (!fileName.isEmpty()) {
      ui_->logFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onSelectMsgLogFile()
{
   QString fileName = QFileDialog::getSaveFileName(this,
      tr("Select file for Matching Engine logs..."),
      QFileInfo(ui_->logMsgFileName->text()).path(),
      QString(), nullptr, QFileDialog::DontConfirmOverwrite);

   if (!fileName.isEmpty()) {
      ui_->logMsgFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onLogFileNameEdited(const QString &)
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
         ui_->warnLabel->setText(tr("Log files must be named."));

         emit illformedSettings(true);

         return;
      }
   }

   if (ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logMsgFileName->text().isEmpty()) {
         ui_->warnLabel->setText(tr("Log files must be named."));

         emit illformedSettings(true);

         return;
      }
   }

   if (ui_->groupBoxLogging->isChecked() && ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logFileName->text() == ui_->logMsgFileName->text()) {
         ui_->warnLabel->setText(tr("Logging requires multiple files."));

         emit illformedSettings(true);

         return;
      }
   }

   ui_->warnLabel->setText(tr("Changes will take effect after the application is restarted."));

   emit illformedSettings(false);
}

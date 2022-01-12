/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "GeneralSettingsPage.h"

#include "ui_GeneralSettingsPage.h"

#include "ApplicationSettings.h"
#include "UiUtils.h"

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

   connect(ui_->pushButtonManage, &QPushButton::clicked, this, &GeneralSettingsPage::requestDataEncryption);
}

GeneralSettingsPage::~GeneralSettingsPage() = default;

void GeneralSettingsPage::init(const ApplicationSettings::State& state)
{
   if (state.find(ApplicationSettings::launchToTray) == state.end()) {
      return;  // not our snapshot
   }
   SettingsPage::init(state);
}

void GeneralSettingsPage::display()
{
   if (appSettings_ && walletsMgr_) {
      ui_->checkBoxLaunchToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::launchToTray));
      ui_->checkBoxMinimizeToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::minimizeToTray));
      ui_->checkBoxCloseToTray->setChecked(appSettings_->get<bool>(ApplicationSettings::closeToTray));
      ui_->checkBoxShowTxNotification->setChecked(appSettings_->get<bool>(ApplicationSettings::notifyOnTX));
      ui_->addvancedDialogByDefaultCheckBox->setChecked(appSettings_->get<bool>(ApplicationSettings::AdvancedTxDialogByDefault));
      ui_->subscribeToMdOnStartCheckBox->setChecked(appSettings_->get<bool>(ApplicationSettings::SubscribeToMDOnStart));
      ui_->detailedSettlementTxDialogByDefaultCheckBox->setChecked(
         appSettings_->get<bool>(ApplicationSettings::DetailedSettlementTxDialogByDefault));

      // DetailedSettlementTxDialogByDefault
      const auto cfg = appSettings_->GetLogsConfig();
      ui_->logFileName->setText(QString::fromStdString(cfg.at(0).fileName));
      ui_->logMsgFileName->setText(QString::fromStdString(cfg.at(1).fileName));
      ui_->logLevel->setCurrentIndex(static_cast<int>(cfg.at(0).level));
      ui_->logLevelMsg->setCurrentIndex(static_cast<int>(cfg.at(1).level));

      UiUtils::fillHDWalletsComboBox(ui_->comboBox_defaultWallet, walletsMgr_, UiUtils::WalletsTypes::All);

      auto walletId = appSettings_->getDefaultWalletId();
      bool setFirstWalletAsDefault = false;
      if (!walletId.empty()) {
         int selectedIndex = UiUtils::selectWalletInCombobox(ui_->comboBox_defaultWallet, walletId, UiUtils::WalletsTypes::All);
         if (selectedIndex == -1) {
            setFirstWalletAsDefault = true;
         }
      } else {
         setFirstWalletAsDefault = true;
      }

      if (setFirstWalletAsDefault) {
         walletId = UiUtils::getSelectedWalletId(ui_->comboBox_defaultWallet);
         appSettings_->setDefaultWalletId(walletId);
      }
   }
   else {
      ui_->checkBoxLaunchToTray->setChecked(settings_.at(ApplicationSettings::launchToTray).toBool());
      ui_->checkBoxMinimizeToTray->setChecked(settings_.at(ApplicationSettings::minimizeToTray).toBool());
      ui_->checkBoxCloseToTray->setChecked(settings_.at(ApplicationSettings::closeToTray).toBool());
      ui_->checkBoxShowTxNotification->setChecked(settings_.at(ApplicationSettings::notifyOnTX).toBool());
      ui_->addvancedDialogByDefaultCheckBox->setChecked(settings_.at(ApplicationSettings::AdvancedTxDialogByDefault).toBool());
      ui_->subscribeToMdOnStartCheckBox->setChecked(settings_.at(ApplicationSettings::SubscribeToMDOnStart).toBool());
      ui_->detailedSettlementTxDialogByDefaultCheckBox->setChecked(
         settings_.at(ApplicationSettings::DetailedSettlementTxDialogByDefault).toBool());

      const auto cfgLog = ApplicationSettings::parseLogConfig(settings_.at(
         ApplicationSettings::logDefault).toStringList());
      const auto cfgMessages = ApplicationSettings::parseLogConfig(settings_.at(
         ApplicationSettings::logMessages).toStringList());
      ui_->logFileName->setText(QString::fromStdString(cfgLog.fileName));
      ui_->logMsgFileName->setText(QString::fromStdString(cfgMessages.fileName));
      ui_->logLevel->setCurrentIndex(static_cast<int>(cfgLog.level));
      ui_->logLevelMsg->setCurrentIndex(static_cast<int>(cfgMessages.level));

      //TODO: handle default wallet if needed
   }
   ui_->warnLabel->hide();
}

void GeneralSettingsPage::reset()
{
   const std::vector<ApplicationSettings::Setting> resetList = {
      ApplicationSettings::launchToTray, ApplicationSettings::minimizeToTray
      , ApplicationSettings::closeToTray, ApplicationSettings::notifyOnTX
      , ApplicationSettings::AdvancedTxDialogByDefault
      , ApplicationSettings::SubscribeToMDOnStart
      , ApplicationSettings::logDefault, ApplicationSettings::logMessages
   };
   if (appSettings_) {
      for (const auto& setting : resetList) {
         appSettings_->reset(setting, false);
      }
      display();
   }
   else {
      emit resetSettings(resetList);
   }
}

static inline QString logLevel(int level)
{
   switch(level) {
   case 0 : return QObject::tr("trace");
   case 1 : return QObject::tr("debug");
   case 2 : return QObject::tr("info");
   case 3 : return QObject::tr("warn");
   case 4 : return QObject::tr("error");
   case 5 : return QObject::tr("crit");
   default : return QString();
   }
}

void GeneralSettingsPage::apply()
{
   const auto walletId = UiUtils::getSelectedWalletId(ui_->comboBox_defaultWallet);

   if (appSettings_) {
      appSettings_->set(ApplicationSettings::launchToTray, ui_->checkBoxLaunchToTray->isChecked());
      appSettings_->set(ApplicationSettings::minimizeToTray, ui_->checkBoxMinimizeToTray->isChecked());
      appSettings_->set(ApplicationSettings::closeToTray, ui_->checkBoxCloseToTray->isChecked());
      appSettings_->set(ApplicationSettings::notifyOnTX, ui_->checkBoxShowTxNotification->isChecked());
      appSettings_->set(ApplicationSettings::AdvancedTxDialogByDefault,
         ui_->addvancedDialogByDefaultCheckBox->isChecked());

      appSettings_->set(ApplicationSettings::SubscribeToMDOnStart
         , ui_->subscribeToMdOnStartCheckBox->isChecked());

      appSettings_->set(ApplicationSettings::DetailedSettlementTxDialogByDefault
         , ui_->detailedSettlementTxDialogByDefaultCheckBox->isChecked());

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
      appSettings_->setDefaultWalletId(walletId);
   }
   else {   // don't update local settings_ yet - the update will arrive explicitly
      emit putSetting(ApplicationSettings::launchToTray, ui_->checkBoxLaunchToTray->isChecked());
      emit putSetting(ApplicationSettings::minimizeToTray, ui_->checkBoxMinimizeToTray->isChecked());
      emit putSetting(ApplicationSettings::closeToTray, ui_->checkBoxCloseToTray->isChecked());
      emit putSetting(ApplicationSettings::notifyOnTX, ui_->checkBoxShowTxNotification->isChecked());
      emit putSetting(ApplicationSettings::AdvancedTxDialogByDefault, ui_->addvancedDialogByDefaultCheckBox->isChecked());
      emit putSetting(ApplicationSettings::SubscribeToMDOnStart, ui_->subscribeToMdOnStartCheckBox->isChecked());
      emit putSetting(ApplicationSettings::DetailedSettlementTxDialogByDefault
         , ui_->detailedSettlementTxDialogByDefaultCheckBox->isChecked());

      const auto netType = static_cast<NetworkType>(settings_.at(ApplicationSettings::netType).toInt());
      emit putSetting((netType == NetworkType::TestNet) ? ApplicationSettings::DefaultXBTTradeWalletIdTestnet
         : ApplicationSettings::DefaultXBTTradeWalletIdMainnet, QString::fromStdString(walletId));

      const auto cfgLog = ApplicationSettings::parseLogConfig(settings_.at(
         ApplicationSettings::logDefault).toStringList());
      {
         QStringList logSettings;
         logSettings << ui_->logFileName->text();
         logSettings << QString::fromStdString(cfgLog.category);
         logSettings << QString::fromStdString(cfgLog.pattern);

         if (ui_->logLevel->currentIndex() < static_cast<int>(bs::LogLevel::off)) {
            logSettings << logLevel(ui_->logLevel->currentIndex());
         } else {
            logSettings << QString();
         }
         emit putSetting(ApplicationSettings::logDefault, logSettings);
      }

      const auto cfgMessages = ApplicationSettings::parseLogConfig(settings_.at(
         ApplicationSettings::logMessages).toStringList());
      {
         QStringList logSettings;
         logSettings << ui_->logMsgFileName->text();
         logSettings << QString::fromStdString(cfgMessages.category);
         logSettings << QString::fromStdString(cfgMessages.pattern);

         if (ui_->logLevelMsg->currentIndex() < static_cast<int>(bs::LogLevel::off)) {
            logSettings << logLevel(ui_->logLevelMsg->currentIndex());
         } else {
            logSettings << QString();
         }
         emit putSetting(ApplicationSettings::logMessages, logSettings);
      }
   }
}

void GeneralSettingsPage::onSelectLogFile()
{
   QString fileName = QFileDialog::getSaveFileName(this,
      tr("Select file for General Terminal logs"),
      QFileInfo(ui_->logFileName->text()).path(),
      QString(), nullptr, QFileDialog::DontConfirmOverwrite);

   if (!fileName.isEmpty()) {
      ui_->logFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onSelectMsgLogFile()
{
   QString fileName = QFileDialog::getSaveFileName(this,
      tr("Select file for Matching Engine logs"),
      QFileInfo(ui_->logMsgFileName->text()).path(),
      QString(), nullptr, QFileDialog::DontConfirmOverwrite);

   if (!fileName.isEmpty()) {
      ui_->logMsgFileName->setText(fileName);
   }
}

void GeneralSettingsPage::onLogFileNameEdited(const QString &)
{
   checkLogSettings();
}

void GeneralSettingsPage::onLogLevelChanged(int)
{
   checkLogSettings();
}

void GeneralSettingsPage::checkLogSettings()
{
   ui_->warnLabel->show();

   if (ui_->groupBoxLogging->isChecked()) {
      if (ui_->logFileName->text().isEmpty()) {
         ui_->warnLabel->setText(tr("Log files must be named"));
         emit illformedSettings(true);
         return;
      }
   }

   if (ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logMsgFileName->text().isEmpty()) {
         ui_->warnLabel->setText(tr("Log files must be named"));
         emit illformedSettings(true);
         return;
      }
   }

   if (ui_->groupBoxLogging->isChecked() && ui_->groupBoxLoggingMsg->isChecked()) {
      if (ui_->logFileName->text() == ui_->logMsgFileName->text()) {
         ui_->warnLabel->setText(tr("Logging requires multiple files"));
         emit illformedSettings(true);
         return;
      }
   }
   ui_->warnLabel->setText(tr("Changes will take effect after the application is restarted"));
   emit illformedSettings(false);
}

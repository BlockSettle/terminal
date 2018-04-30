#include <QFileDialog>
#include "SignerSettingsPage.h"
#include "ui_SignerSettingsPage.h"
#include "ApplicationSettings.h"
#include "BtcUtils.h"


enum RunModeIndex {
   Local = 0,
   Remote,
   Offline
};


SignerSettingsPage::SignerSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::SignerSettingsPage{}}
{
   ui_->setupUi(this);
   connect(ui_->comboBoxRunMode, SIGNAL(activated(int)), this, SLOT(runModeChanged(int)));
   connect(ui_->pushButtonOfflineDir, &QPushButton::clicked, this, &SignerSettingsPage::onOfflineDirSel);
   connect(ui_->spinBoxAsSpendLimit, SIGNAL(valueChanged(double)), this, SLOT(onAsSpendLimitChanged(double)));
}

void SignerSettingsPage::runModeChanged(int index)
{
   onModeChanged(index, false);
}

void SignerSettingsPage::onOfflineDirSel()
{
   const auto dir = QFileDialog::getExistingDirectory(this, tr("Dir for offline signer files")
      , ui_->labelOfflineDir->text(), QFileDialog::ShowDirsOnly);
   if (dir.isEmpty()) {
      return;
   }
   ui_->labelOfflineDir->setText(dir);
}

void SignerSettingsPage::onModeChanged(int index, bool displayDefault)
{
   switch (static_cast<RunModeIndex>(index)) {
   case Local:
      showHost(false);
      showPort(true);
      ui_->spinBoxPort->setValue(appSettings_->get<int>(ApplicationSettings::signerPort, displayDefault));
      showPassword(true);
      ui_->lineEditPassword->setText(appSettings_->get<QString>(ApplicationSettings::signerPassword, displayDefault));
      showOfflineDir(false);
      showLimits(true);
      ui_->spinBoxAsSpendLimit->setValue(appSettings_->get<double>(ApplicationSettings::autoSignSpendLimit, displayDefault));
      break;

   case Remote:
      showHost(true);
      ui_->lineEditHost->setText(appSettings_->get<QString>(ApplicationSettings::signerHost, displayDefault));
      showPort(true);
      ui_->spinBoxPort->setValue(appSettings_->get<int>(ApplicationSettings::signerPort, displayDefault));
      showPassword(true);
      ui_->lineEditPassword->setText(appSettings_->get<QString>(ApplicationSettings::signerPassword, displayDefault));
      showOfflineDir(false);
      showLimits(false);
      break;

   case Offline:
      showHost(false);
      showPort(false);
      showPassword(false);
      showOfflineDir(true);
      ui_->labelOfflineDir->setText(appSettings_->get<QString>(ApplicationSettings::signerOfflineDir, displayDefault));
      showLimits(false);
      break;

   default:    break;
   }
}

void SignerSettingsPage::setAppSettings(const std::shared_ptr<ApplicationSettings>& appSettings)
{
   appSettings_ = appSettings;
}

void SignerSettingsPage::displaySettings(bool displayDefault)
{
   const auto modeIndex = appSettings_->get<int>(ApplicationSettings::signerRunMode, displayDefault) - 1;
   onModeChanged(modeIndex, displayDefault);
   ui_->comboBoxRunMode->setCurrentIndex(modeIndex);
}

void SignerSettingsPage::showHost(bool show)
{
   ui_->labelHost->setVisible(show);
   ui_->lineEditHost->setVisible(show);
}

void SignerSettingsPage::showPort(bool show)
{
   ui_->labelPort->setVisible(show);
   ui_->spinBoxPort->setVisible(show);
}

void SignerSettingsPage::showPassword(bool show)
{
   ui_->labelPassword->setVisible(show);
   ui_->lineEditPassword->setVisible(show);
}

void SignerSettingsPage::showOfflineDir(bool show)
{
   ui_->labelDirHint->setVisible(show);
   ui_->widgetOfflineDir->setVisible(show);
}

void SignerSettingsPage::showLimits(bool show)
{
   ui_->groupBoxAutoSign->setVisible(show);
   ui_->labelAsSpendLimit->setVisible(show);
   ui_->spinBoxAsSpendLimit->setVisible(show);
   onAsSpendLimitChanged(ui_->spinBoxAsSpendLimit->value());
}

void SignerSettingsPage::onAsSpendLimitChanged(double value)
{
   if (value > 0) {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit:"));
   }
   else {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit - unlimited"));
   }
}

void SignerSettingsPage::applyChanges()
{
   switch (static_cast<RunModeIndex>(ui_->comboBoxRunMode->currentIndex())) {
   case Local:
      appSettings_->set(ApplicationSettings::signerPort, ui_->spinBoxPort->value());
      appSettings_->set(ApplicationSettings::autoSignSpendLimit, ui_->spinBoxAsSpendLimit->value());
      savePassword();
      break;

   case Remote:
      appSettings_->set(ApplicationSettings::signerHost, ui_->lineEditHost->text());
      appSettings_->set(ApplicationSettings::signerPort, ui_->spinBoxPort->value());
      savePassword();
      break;

   case Offline:
      appSettings_->set(ApplicationSettings::signerOfflineDir, ui_->labelOfflineDir->text());
      break;

   default:    break;
   }
   appSettings_->set(ApplicationSettings::signerRunMode, ui_->comboBoxRunMode->currentIndex() + 1);

}

void SignerSettingsPage::savePassword()
{
   const auto &password = ui_->lineEditPassword->text().toStdString();
   if (!password.empty() && (password != appSettings_->get<std::string>(ApplicationSettings::signerPassword))) {
      const auto hash = BtcUtils::getSha256(password);
      appSettings_->set(ApplicationSettings::signerPassword, QString::fromStdString(hash.getSliceCopy(0, 8).toHexStr()));
   }
}

#include <QFileDialog>
#include <QStandardPaths>
#include "SignerSettingsPage.h"
#include "ui_SignerSettingsPage.h"
#include "ApplicationSettings.h"
#include "BtcUtils.h"
#include "BSMessageBox.h"
#include "SignContainer.h"


enum RunModeIndex {
   Local = 0,
   Remote,
   Offline
};


SignerSettingsPage::SignerSettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::SignerSettingsPage{}}
{
   ui_->setupUi(this);
   ui_->lineEditSignerKey->hide();

   connect(ui_->comboBoxRunMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignerSettingsPage::runModeChanged);
   connect(ui_->pushButtonOfflineDir, &QPushButton::clicked, this, &SignerSettingsPage::onOfflineDirSel);
   connect(ui_->spinBoxAsSpendLimit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SignerSettingsPage::onAsSpendLimitChanged);

   connect(ui_->comboBoxSignerKeyImportType, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
      ui_->lineEditSignerKey->setVisible(index == 1);
      ui_->lineEditSignerKeyPath->setVisible(index == 0);
      ui_->pushButtonSignerKey->setVisible(index == 0);
   });

   connect(ui_->pushButtonSignerKey, &QPushButton::clicked, [this](){
      QString fileName = QFileDialog::getOpenFileName(this
                                   , tr("Open Signer Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                   , tr("Key files (*.pub)"));

      if (!fileName.isEmpty()) {
         ui_->lineEditSignerKeyPath->setText(fileName);
      }
   });
}

SignerSettingsPage::~SignerSettingsPage() = default;

void SignerSettingsPage::runModeChanged(int index)
{
   onModeChanged(index);
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

void SignerSettingsPage::onModeChanged(int index)
{
   switch (static_cast<RunModeIndex>(index)) {
   case Local:
      showHost(false);
      showPort(true);
      ui_->spinBoxPort->setValue(appSettings_->get<int>(ApplicationSettings::signerPort));
      showOfflineDir(false);
      showLimits(true);
      showSignerKeySettings(false);
      ui_->spinBoxAsSpendLimit->setValue(appSettings_->get<double>(ApplicationSettings::autoSignSpendLimit));
      ui_->formLayoutConnectionParams->setSpacing(3);
      break;

   case Remote:
      showHost(true);
      ui_->lineEditHost->setText(appSettings_->get<QString>(ApplicationSettings::signerHost));
      showPort(true);
      ui_->spinBoxPort->setValue(appSettings_->get<int>(ApplicationSettings::signerPort));
      showOfflineDir(false);
      showLimits(false);
      showSignerKeySettings(true);
      ui_->formLayoutConnectionParams->setSpacing(6);
      break;

   case Offline:
      showHost(false);
      showPort(false);
      showOfflineDir(true);
      ui_->labelOfflineDir->setText(appSettings_->get<QString>(ApplicationSettings::signerOfflineDir));
      showLimits(false);
      showSignerKeySettings(false);
      ui_->formLayoutConnectionParams->setSpacing(0);
      break;

   default:    break;
   }
}

void SignerSettingsPage::display()
{
   const auto modeIndex = appSettings_->get<int>(ApplicationSettings::signerRunMode) - 1;
   onModeChanged(modeIndex);
   ui_->comboBoxRunMode->setCurrentIndex(modeIndex);
   ui_->checkBoxTwoWayAuth->setChecked(appSettings_->get<bool>(ApplicationSettings::twoWayAuth));
}

void SignerSettingsPage::reset()
{
   for (const auto &setting : {ApplicationSettings::signerRunMode, ApplicationSettings::signerHost
      , ApplicationSettings::signerPort, ApplicationSettings::signerOfflineDir
      , ApplicationSettings::zmqRemoteSignerPubKey, ApplicationSettings::autoSignSpendLimit
      , ApplicationSettings::twoWayAuth}) {
      appSettings_->reset(setting, false);
   }
   display();
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

void SignerSettingsPage::showSignerKeySettings(bool show)
{
   ui_->widgetTwoWayAuth->setVisible(show);
   ui_->checkBoxTwoWayAuth->setVisible(show);
   ui_->widgetSignerKeyLabel->setVisible(show);
   ui_->widgetSignerKeyComboBox->setVisible(show);
   ui_->widgetSignerKeyContent->setVisible(show);
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

void SignerSettingsPage::apply()
{
   switch (static_cast<RunModeIndex>(ui_->comboBoxRunMode->currentIndex())) {
   case Local:
      appSettings_->set(ApplicationSettings::signerPort, ui_->spinBoxPort->value());
      appSettings_->set(ApplicationSettings::autoSignSpendLimit, ui_->spinBoxAsSpendLimit->value());
      break;

   case Remote:
      appSettings_->set(ApplicationSettings::signerHost, ui_->lineEditHost->text());
      appSettings_->set(ApplicationSettings::signerPort, ui_->spinBoxPort->value());
      break;

   case Offline:
      appSettings_->set(ApplicationSettings::signerOfflineDir, ui_->labelOfflineDir->text());
      break;

   default:    break;
   }

   // first comboBoxRunMode index is '--Select--' placeholder
   appSettings_->set(ApplicationSettings::signerRunMode, ui_->comboBoxRunMode->currentIndex() + 1);
   appSettings_->set(ApplicationSettings::twoWayAuth, ui_->checkBoxTwoWayAuth->isChecked());

   // save signer key from file or from line input
   QString signerKey;
   if (ui_->comboBoxSignerKeyImportType->currentIndex() == 0) {
      QFile file(ui_->lineEditSignerKeyPath->text());
      if (file.open(QIODevice::ReadOnly)) {
         signerKey = QString::fromLatin1(file.readAll());
      }
   }
   else {
      signerKey = ui_->lineEditSignerKey->text();
   }

   if (!signerKey.isEmpty()) {
      appSettings_->set(ApplicationSettings::zmqRemoteSignerPubKey, signerKey);
   }
}
